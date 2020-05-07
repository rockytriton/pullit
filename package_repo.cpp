#include "package_repo.h"

#include <stdio.h>
#include <unistd.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <sys/stat.h>

#include "download.h"

namespace pullit {

static const string INSTALL_FILE = "/usr/share/pullit/installed.xml";


PackageRepo::PackageRepo() {
    LIBXML_TEST_VERSION
}

PackageRepo::~PackageRepo() {
    xmlCleanupParser();
}

PackageRepo &PackageRepo::inst() {
    static PackageRepo INST;
    return INST;
}

void PackageRepo::update() {
    Retriever::inst().download("http://pullit.org/packages.xml", "/usr/share/pullit/packages.xml");
}

void PackageRepo::load() {
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;

    doc = xmlReadFile("/usr/share/pullit/installed.xml", NULL, 0);
    load_installed(xmlDocGetRootElement(doc));
    xmlFreeDoc(doc);

    doc = xmlReadFile("/usr/share/pullit/packages.xml", NULL, 0);
    root_element = xmlDocGetRootElement(doc);

    build_map(root_element);

    xmlFreeDoc(doc);

}

void PackageRepo::load_installed(void *node) {
    xmlNode *a_node = (xmlNode *)node;
    xmlNode *curPck = nullptr;

    if (CMD_LIST_INSTALLED()) {
        cout << endl << "Installed Packages: " << endl;
    }

    for (curPck = a_node->children; curPck; curPck = curPck->next) {

        if (curPck->type == XML_ELEMENT_NODE) {
            if (!STR_EQ(curPck->name, "pck")) {
                if (CMD_VALIDATE()) {
                    cout << "installed.xml: Invalid Element Type: " << (const char *)curPck->name << endl;
                }

                continue;
            }

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
                } else if (CMD_VALIDATE()) {
                    cout << "installed.xml: Invalid pck Attribute: " << name << endl;
                }
            }

            if (CMD_LIST_INSTALLED()) {
                cout << "\t" << pck->name << " " << pck->version << endl;   
            }

            //if (CMD_VALIDATE() && pck->file == "") 
            //    cout << "installed.xml: Invalid pck, missing file attribute" << endl;

            if (CMD_VALIDATE() && pck->name == "") 
                cout << "installed.xml: Invalid pck, missing name attribute" << endl;

            if (CMD_VALIDATE() && pck->version == "") 
                cout << "installed.xml: Invalid pck, missing version attribute" << endl;

            if (CMD_LIST_INSTALLED())
                cout << "Installed: " << pck->name << " - " << pck->version << " - " << pck->file << endl;
            
            installedPackages[pck->name] = pck;
        }
    }
}

void PackageRepo::build_map(void* node)
{
    xmlNode * a_node = (xmlNode *)node;
    xmlNode *curPck = nullptr;

    for (curPck = a_node->children; curPck; curPck = curPck->next) {

        if (curPck->type == XML_ELEMENT_NODE) {
            if (!STR_EQ(curPck->name, "pck")) {
                if (CMD_VALIDATE()) {
                    cout << "packages.xml: Invalid Element Type: " << (const char *)curPck->name << endl;
                }

                continue;
            }

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
                } else if (CMD_VALIDATE()) {
                    cout << "packages.xml: Invalid pck Attribute: " << name << endl;
                }
            }

            if (CMD_VALIDATE() && pck->file == "") 
                cout << "packages.xml: Invalid pck " << pck->name << ", missing file attribute" << endl;

            if (CMD_VALIDATE() && pck->name == "") 
                cout << "packages.xml: Invalid pck " << pck->name << ", missing name attribute" << endl;

            if (CMD_VALIDATE() && pck->version == "") 
                cout << "packages.xml: Invalid pck " << pck->name << ", missing version attribute" << endl;

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

                    if (name == "name") {
                        depName = val;
                    } else if (name == "version") {
                        depVersion = val;
                    } else if (CMD_VALIDATE()) {
                        cout << "packages.xml: Invalid dep Attribute: " << name << endl;
                    }
                }

                map<string, shared_ptr<Package>>::iterator it = packageMap.find(depName);

                if (it == packageMap.end()) {
                    if (CMD_VALIDATE()) {
                        cout << "packages.xml: Invalid pck " << pck->name << ", missing dependency " << depName << endl;
                    } else {
                        std::cerr << "PACKAGE NOT FOUND: '" << depName << "'" << endl;
                        exit(-1);
                    }
                }

                if (CMD_VALIDATE() && (*it).second->version != depVersion) {
                    cout << "packages.xml: Invalid pck " << pck->name << ", dependency version mismatch " << depName << endl;
                }

                shared_ptr<Package> dep = packageMap[depName];
                pck->dependencies.push_back(dep);
            }
        }
    }
}

void PackageRepo::install_packages() {
    for (size_t i = 0; i < g_options.packages.size(); i++) {
        string pckName = g_options.packages[i];
        cout << "Preparing to install package " << pckName << endl;
        shared_ptr<Package> pck = packageMap[pckName];
        cout << "Found version " << pck->version << endl;

        install_package(pck);
    }
}

void PackageRepo::install_package(shared_ptr<Package> pck) {
    map<string, shared_ptr<Package>>::iterator it = installedPackages.find(pck->name);

    if (it != installedPackages.end()) {
        cout << "Package " << pck->name << " already installed." << endl;
        return;
    }

    if (!pck->dependencies.empty()) {
        for (int i=0; i<pck->dependencies.size(); i++) {
            cout << "Found dependency '" << pck->dependencies[i]->name << "' for '" << pck->name << "'" << endl;
            install_package(pck->dependencies[i]);
        }
    }

    if (pck->file == "") {
	    cout << "Installed package " << pck->name << endl;
	    return;
    }

    cout << "Installing '" << pck->name << "'..." << endl;

    extract_file(pck);

    store_package_info(pck);
}

void PackageRepo::extract_file(shared_ptr<Package> pck) {
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
        system("cd /tmp/pullit/_install && /bin/bash /tmp/pullit/_install/preinstall.sh");
    }

    cout << "Copying files..." << endl;
    system(string("cp -r " + outPath + "/* /").c_str());

    if (installer) {
        cout << "Running installer..." << endl;
        system("cd /tmp/pullit/_install && /bin/bash /tmp/pullit/_install/install.sh");
    }

    cout << "Cleaning up files..." << endl;
    system("rm -rf /tmp/pullit/_install 2> /dev/null");
    system(string("rm -rf " + outPath).c_str());

    cout << "Package '" << pck->name << "' installation complete." << endl << endl;

    installedPackages[pck->name] = pck;
}

void PackageRepo::store_package_info(shared_ptr<Package> pck) {
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

};
