#include <stdio.h>
#include <unistd.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <sys/stat.h>

#include "download.h"
#include "package_repo.h"
#include "package_db.h"

using pullit::Retriever;
using pullit::PackageRepo;
using pullit::PackageDb;

Options g_options;

bool parsePackageDetails(Package &pck, vector<Package> &deps, int argc, char **argv);
void showUsage();
bool parsePackage(string pckString, Package &pck);

int main(int argc, char **argv) {

    cout << "PullIt 1.0.0\r\n" << endl;

    if (argc < 2) {
        return -1;
    }

    g_options.command = (const char *)argv[1];

    if (CMD_ADD_PACKAGE()) {
        Package pck;
        vector<Package> deps;
        
        if (!parsePackageDetails(pck, deps, argc, argv)) {
            showUsage();
            return -1;
        }

        return PackageDb::inst().insertPackage(pck, deps) ? 0 : -1;
    }

    if (CMD_VALIDATE()) {
        return PackageDb::inst().validate();
    }

    for (int i=2; i<argc; i++) {
        g_options.packages.push_back((const char *)argv[i]);
    }

    if (CMD_MARK_INST()) {
        bool success = true;

        for (uint32_t i = 0; i < g_options.packages.size(); i++) {
            Package pck;
            pck.name = g_options.packages[i];

            success &= PackageDb::inst().markInstalled(pck);
        }

        return success ? 0 : -1;
    }

    if (CMD_LIST_INSTALLED()) {
        Package pck;
        vector<Package> allPck = PackageDb::inst().listInstalled(pck);

        for (uint32_t i = 0; i < allPck.size(); i++) {
            cout << allPck[i].name << " - " << allPck[i].version << endl;
        }

        return 0;
    }

    if (CMD_INSTALL()) {
        bool success = true;

        for (uint32_t i = 0; i < g_options.packages.size(); i++) {
            Package pck;
            
            if (!parsePackage(g_options.packages[i], pck)) {
                return -1;
            }

            success &= PackageDb::inst().installPackage(pck);
        }

        return success ? 0 : -1;
    }

    cout << "Updating..." << endl;
    PackageRepo::inst().update();
    cout << "Updated." << endl;
    
    PackageRepo::inst().load();

    if (CMD_INSTALL()) {
        PackageRepo::inst().install_packages();
    } else if (CMD_VALIDATE()) {
        cout << endl << "Validation Complete" << endl;
    } else if (CMD_LIST_MISSING()) {
        
    } else if (CMD_INSTALL_MANUAL()) {

    }
}

bool parsePackage(string pckString, Package &pck) {
    vector<string> tokens = split(arg, ',');

    pck.name = tokens[0];
    pck.version = tokens.size() > 1 ? tokens[1] : "";

    if (pck.name == "") {
        cout << "Invalid package: " << pckString << endl;
        return false;
    }

    return true;
}

bool parsePackageDetails(Package &pck, vector<Package> &deps, int argc, char **argv) {
    for (int i=2; i<argc; i++) {
        string arg = (const char *)argv[i];
        vector<string> tokens = split(arg, ',');

        Package p;
        p.name = tokens[0];
        p.version = tokens.size() > 1 ? tokens[1] : "";
        p.file = tokens.size() > 2 ? tokens[2] : "";

        if (i == 2) {
            pck = p;
        } else {
            deps.push_back(p);
        }
    }

    if (pck.name == "" || pck.version == "") {
        cout << "Invalid package: " << argv[2] << endl;
        return false;
    }

    return true;
}

void showUsage() {
    cout << "usage: " << endl;
    cout << "\tpullit <command> [package...]" << endl;
}