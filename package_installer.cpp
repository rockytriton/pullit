#include "package_installer.h"
#include "package_db.h"
#include "download.h"

#include <sqlite3.h>
#include <unistd.h>
#include <sys/stat.h>
#include <filesystem>

#include <sstream>

namespace fs = std::filesystem;

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

void copyFiles(string from, string to) {
    for(auto& p: fs::directory_iterator(from)) {
        auto fromFile = from + "/" + p.path().filename();
        auto toFile = to + "/" + p.path().filename();

        if (p.is_directory()) {

            if (p.is_symlink()) {
                cout << "IS_SYM_LINK_DIR: " << fromFile << endl;
            }

            copyFiles(fromFile, toFile);
        } else {
            cout << "Copying file: " << fromFile << " to " << toFile << endl;
            fs::copy(fromFile, toFile);
        }
    }
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
        installer = true;
    }

    if(std::filesystem::exists(string(outPath + "/_install"))) {
        if (!checkFailure(system(string(
                "mv " + outPath + "/_install " + tempPath + "_install").c_str()), "FAILED MOVING FILES")) {
            return false;
        }

        cout << "Renamed: " << tempPath << "_install" << endl;
    }

    if (preInstaller) {
        cout << "Running pre-installer" << endl;
        
        if (!checkFailure(system(string("/bin/bash -c 'cd " + tempPath + "_install && bash " + 
                tempPath + "_install/preinstall.sh'").c_str()),
                "FAILING INVOKING PRE-INSTALLER!")) {
            return false;
        }
    }

    cout << "Copying files: " << outPath << endl;

    copyFiles(outPath, "/");

    //std::filesystem::copy(outPath, "/", std::filesystem::copy_options::recursive | 
    //    std::filesystem::copy_options::overwrite_existing);

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

    string tarCommand = "tar -xf /tmp/pullit/tmp.tar.gz ";
    tarCommand += "-C ";
    tarCommand += outPath;

    cout << "Extracting " << file << " ..." << endl;
    
    if (system(tarCommand.c_str())) {
        cerr << "Error extracting tar." << endl;
        return false;
    }

    return true;
}

}

