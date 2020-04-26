#include <stdio.h>
#include <unistd.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <sys/stat.h>
#include <curl/curl.h>

#include "download.h"

using pullit::Retriever;

Options g_options;

const string INSTALL_FILE = "/usr/share/pullit/installed.xml";

map<string, shared_ptr<Package>> packageMap;

string inputPackage;

map<string, shared_ptr<Package>> installedPackages;

static void storePackageInfo(shared_ptr<Package> pck) {
    xmlDoc *doc = xmlParseFile("/usr/share/pullit/installed.xml");
    xmlNode *root = xmlDocGetRootElement(doc);

    xmlNodePtr pNode = xmlNewNode(0, (xmlChar*)"pck");

    xmlIndentTreeOutput = 1;

	xmlAttrPtr newattr = xmlNewProp (pNode, (const xmlChar *)"name", (const xmlChar *)pck->name.c_str());
    xmlNewProp (pNode, (const xmlChar *)"version", (const xmlChar *)pck->version.c_str());

    xmlAddChild(root, pNode);
    xmlDocSetRootElement(doc, root);

    xmlSaveFormatFile ("/usr/share/pullit/installed.xml", doc, 1);
    
    xmlFreeDoc(doc);
}

static void extractFile(shared_ptr<Package> pck) {
    string outPath = string("/tmp/pullit/") + pck->file;
    mkdir("/tmp/pullit", S_IRUSR | S_IWUSR | S_IXUSR);

    string path = "/tmp/pullit/tmp.tar.gz";

    if (!Retriever::inst().download("http://pullit.org/" + pck->file, path)) {
        cerr << "Failed to download package" << endl;
        exit(-1);
    }

    if (access("/tmp/pullit/tmp.tar.gz", F_OK ) == -1) {
        cerr << "Failed to download file" << endl;
        exit(-1);
    }

    mkdir(string(outPath).c_str(), S_IRUSR | S_IWUSR | S_IXUSR);

    if (access(outPath.c_str(), F_OK ) == -1) {
        cerr << "Failed to create extraction directory: " << outPath << endl;
        exit(-1);
    }

    string tarCommand = "tar -xf /tmp/pullit/tmp.tar.gz ";
    tarCommand += "-C ";
    tarCommand += outPath;

    cout << "Extracting " << pck->file << " ..." << endl;
    
    if (system(tarCommand.c_str())) {
        cerr << "Error extracting tar." << endl;
        exit(-1);
    }

    bool installer = false;

    bool preInstaller = false;

    if(access(string(outPath + "/_install/preinstall.sh").c_str(), F_OK ) != -1 ) {
        preInstaller = true;
    }

    if(access(string(outPath + "/_install/install.sh").c_str(), F_OK ) != -1 ) {
        installer = true;
    }

    if(access(string(outPath + "/_install").c_str(), F_OK ) != -1 ) {
        string mvCommand = "mv " + outPath;
        mvCommand += "/_install /tmp/pullit/";

        system(mvCommand.c_str());
    }

    if (preInstaller) {
        cout << "Running pre-installer" << endl;
        system("cd /tmp/pullit && /bin/bash /tmp/pullit/_install/preinstall.sh");
    }

    cout << "Copying files..." << endl;
    system(string("cp -r " + outPath + "/* /").c_str());

    if (installer) {
        cout << "Running installer..." << endl;
        system("cd /tmp/pullit && /bin/bash /tmp/pullit/_install/install.sh");
    }

    cout << "Cleaning up files..." << endl;
    system("rm -rf /tmp/pullit/_install 2> /dev/null");
    system(string("rm -rf " + outPath).c_str());

    cout << "Package '" << pck->name << "' installation complete." << endl;
    installedPackages[pck->name] = pck;
}

static void installPackage(shared_ptr<Package> pck) {
    map<string, shared_ptr<Package>>::iterator it = installedPackages.find(pck->name);

    if (it != installedPackages.end()) {
        cout << "Package " << pck->name << " already installed." << endl;
        return;
    }

    if (!pck->dependencies.empty()) {
        for (int i=0; i<pck->dependencies.size(); i++) {
            cout << "Found dependency '" << pck->dependencies[i]->name << "' for '" << pck->name << "'" << endl;
            installPackage(pck->dependencies[i]);
        }
    } else {
        //cout << "    - '" << pck->name << "' has no depenedencies..." << endl;
    }

    cout << "Installing '" << pck->name << "'..." << endl;
    extractFile(pck);

    storePackageInfo(pck);
}

static void loadInstalled(xmlNode *a_node) {
    xmlNode *curPck = nullptr;

    for (curPck = a_node->children; curPck; curPck = curPck->next) {

        if (curPck->type == XML_ELEMENT_NODE) {
            shared_ptr<Package> pck = make_shared<Package>();

            for (xmlAttr *att = curPck->properties; att; att = att->next) {
                string name = (const char *)att->name;
                string val = (const char *)att->children->content;

                if (name == "file") {
                    pck->file = val;
                } else if (name == "name") {
                    pck->name = val;
                } else if (name == "version") {
                    pck->version = val;
                } else {
                    cout << "UNKNOWN ATT: " << name << endl;
                }
            }

            cout << "Installed: " << pck->name << " - " << pck->version << " - " << pck->file << endl;
            installedPackages[pck->name] = pck;
        }
    }
}

static void build_map(xmlNode * a_node)
{
    xmlNode *curPck = nullptr;

    for (curPck = a_node->children; curPck; curPck = curPck->next) {

        if (curPck->type == XML_ELEMENT_NODE) {

            //cout << "Package: " << (const char *)curPck->name << endl;

            shared_ptr<Package> pck = make_shared<Package>();

            for (xmlAttr *att = curPck->properties; att; att = att->next) {
                //
                string name = (const char *)att->name;
                string val = (const char *)att->children->content;

                //cout << "\tRead Att: " << name << " = " << val << endl;
                if (name == "file") {
                    pck->file = val;
                } else if (name == "name") {
                    pck->name = val;
                } else if (name == "version") {
                    pck->version = val;
                } else {
                    cout << "UNKNOWN ATT: " << name << endl;
                }
            }

            packageMap[pck->name] = pck;

            xmlNode *deps = nullptr;

            for (deps = curPck->children; deps; deps = deps->next) {
                string depName;
                string depVersion;

                if (deps->type != XML_ELEMENT_NODE) {
                    deps = deps->next;

                    if (!deps) {
                        break;
                    }
                }

                for (xmlAttr *att = deps->properties; att; att = att->next) {
                    string name = (const char *)att->name;
                    string val = (const char *)att->children->content;

                    //cout << "\tRead Att: " << name << " = " << val << endl;
                    if (name == "name") {
                        depName = val;
                    } else if (name == "version") {
                        depVersion = val;
                    } else {
                        cout << "UNKNOWN ATT: " << name << endl;
                    }
                }

                map<string, shared_ptr<Package>>::iterator it = packageMap.find(depName);

                if (it == packageMap.end()) {
                    std::cerr << "PACKAGE NOT FOUND: '" << depName << "'" << endl;
                    exit(-1);
                } 

                shared_ptr<Package> dep = packageMap[depName];
                pck->dependencies.push_back(dep);
            }

            if (pck->name == inputPackage) {
                cout << "Found package '" << pck->name << "'" << endl;
                installPackage(pck);
            }
        }

    }
}

int main(int argc, char **argv) {

    LIBXML_TEST_VERSION

    if (argc < 2) {
        cout << "Pullit usage: " << endl;
        cout << "pullit <command> [package...]" << endl;
        return -1;
    }

    g_options.command = (const char *)argv[1];

    for (int i=2; i<argc; i++) {
        g_options.packages.push_back((const char *)argv[i]);
    }

    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;

    inputPackage = (const char *)argv[1];
    cout << "\r\nPullIt 0.1.4\r\n" << endl;

    cout << "Updating..." << endl;
    Retriever::inst().download("http://pullit.org/packages.xml", "/usr/share/pullit/packages.xml");
    cout << "Updated." << endl;
    
    cout << "Loading installed packages..." << endl;
    doc = xmlReadFile("/usr/share/pullit/installed.xml", NULL, 0);
    loadInstalled(xmlDocGetRootElement(doc));
    xmlFreeDoc(doc);

    cout << "\r\nFinding package '" << inputPackage << "'..." << endl;

    doc = xmlReadFile("/usr/share/pullit/packages.xml", NULL, 0);
    root_element = xmlDocGetRootElement(doc);

    build_map(root_element);

    xmlFreeDoc(doc);

    xmlCleanupParser();
}

