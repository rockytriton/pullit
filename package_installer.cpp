#include "package_installer.h"
#include "package_db.h"
#include "download.h"

#include <sqlite3.h>
#include <unistd.h>
#include <sys/stat.h>
#include <filesystem>

#include <sstream>

namespace pullit {

PackageInstaller::PackageInstaller() {

}

PackageInstaller::~PackageInstaller() {

}

PackageInstaller &PackageInstaller::inst() {
    static PackageInstaller INST;
    return INST;
}

bool checkFailure(int sys, string msg) {
    if (sys) {
        cout << msg << " Continue (y,N)? ";

        string resp;
        std::getline(std::cin, resp);

        if (resp != "y") {
            return false;
        }
    }

    return true;
}

bool PackageInstaller::install(Package &pck) {
    string outPath = tempPath + pck.file;
    
    if (!extract(pck.file)) {
        return false;
    }

    bool installer = false;
    bool filesToCopy = false;
    bool preInstaller = false;

    if (std::filesystem::exists(string(outPath + "/_install/preinstall.sh"))) {
        preInstaller = true;
    }

    if (std::filesystem::exists(string(outPath + "/_install/install.sh"))) {
        preInstaller = true;
    }

    if(std::filesystem::exists(string(outPath + "/_install"))) {
        std::filesystem::rename(outPath + "/_install", tempPath + "_install");
    }

    if (preInstaller) {
        cout << "Running pre-installer" << endl;
        
        if (!checkFailure(system(string("/bin/bash -c 'cd " + tempPath + "_install && bash " + 
                tempPath + "_install/preinstall.sh'").c_str()),
                "FAILING INVOKING PRE-INSTALLER!")) {
            return false;
        }
    }

    cout << "Copying files..." << endl;

    std::filesystem::copy(outPath, "/", std::filesystem::copy_options::recursive | 
        std::filesystem::copy_options::overwrite_existing);

    if (installer) {
        cout << "Running installer" << endl;
        
        if (!checkFailure(system(string("/bin/bash -c 'cd " + tempPath + "_install && bash " + 
                tempPath + "_install/install.sh'").c_str()),
                "FAILING INVOKING INSTALLER!")) {
            return false;
        }
    }

    cout << "Cleaning up files..." << endl;

    system(string("rm -rf " + tempPath + "_install 2> /dev/null").c_str());
    system(string("rm -rf " + outPath).c_str());

    cout << "Package " << pck.name << " " << pck.version << " installation complete." << endl << endl;

    return true;
}

bool PackageInstaller::extract(string &file) {
    string outPath = tempPath + file;
    mkdir(tempPath.c_str(), S_IRUSR | S_IWUSR | S_IXUSR);

    string path = tempPath + "tmp.tar.gz";

    std::filesystem::remove(path);

    if (PackageDb::inst().packageFileExists(file)) {
        std::filesystem::copy(PackageDb::inst().getPackagesPath() + "/" + file, path, 
                std::filesystem::copy_options::overwrite_existing);
    } 
    else if (!Retriever::inst().download("http://pullit.org/" + file, path)) {
        cerr << "Failed to download package" << endl;
        return false;
    }

    if (access("/tmp/pullit/tmp.tar.gz", F_OK ) == -1) {
        cerr << "Failed to download file" << endl;
        return false;
    }

    mkdir(string(outPath).c_str(), S_IRUSR | S_IWUSR | S_IXUSR);

    if (access(outPath.c_str(), F_OK ) == -1) {
        cerr << "Failed to create extraction directory: " << outPath << endl;
        return false;
    }

    return true;
}

}

