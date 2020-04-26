#include <stdio.h>
#include <unistd.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <sys/stat.h>

#include "download.h"
#include "package_repo.h"

using pullit::Retriever;
using pullit::PackageRepo;

Options g_options;

int main(int argc, char **argv) {

    if (argc < 2) {
        cout << "PullIt 0.1.4 usage: " << endl;
        cout << "pullit <command> [package...]" << endl;
        return -1;
    }

    g_options.command = (const char *)argv[1];

    for (int i=2; i<argc; i++) {
        g_options.packages.push_back((const char *)argv[i]);
    }

    cout << "\r\nPullIt 0.1.4\r\n" << endl;

    cout << "Updating..." << endl;
    PackageRepo::inst().update();
    cout << "Updated." << endl;
    
    if (CMD_INSTALL()) {
        PackageRepo::inst().install_packages();
    } else if (CMD_VALIDATE()) {
        cout << endl << "Validation Complete" << endl;
    }
}

