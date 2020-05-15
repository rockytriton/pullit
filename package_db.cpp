#include "package_db.h"
#include "package_installer.h"

#include <sqlite3.h>
#include <unistd.h>
#include <sys/stat.h>
#include <filesystem>

#include <sstream>

namespace pullit {

#define DB ((sqlite3 *)dbHandle)

PackageDb::PackageDb() {
    if (sqlite3_open("/src/pullinux-1.0.0/db/packages.db", (sqlite3 **)&dbHandle)) {
        cerr << "Failed to open db: " << sqlite3_errmsg(DB) << endl;
        sqlite3_close(DB);
        exit(0);
    }
}

PackageDb::~PackageDb() {
    sqlite3_close(DB);
}

PackageDb &PackageDb::inst() {
    static PackageDb INST;
    return INST;
}

int PackageDb::fetchCallback(void *userData, int argc, char **argv, char **colNames) {
    vector<std::map<string, string>> *data = (vector<std::map<string, string>> *)userData;
    std::map<string, string> map;

    for (int i=0; i<argc; i++) {
        string val = argv[i] ? (const char *)argv[i] : "";
        map[(const char *)colNames[i]] = val;
    }

    (*data).push_back(map);

    return 0;
}

bool PackageDb::dbInsert(string query) {
    char *errMsg = 0;
    int rc = 0;

    if ((rc = sqlite3_exec(DB, query.c_str(), NULL, NULL, &errMsg)) != SQLITE_OK) {
        if (errMsg) {
            lastErrorMsg = (const char *)errMsg;
            sqlite3_free(errMsg);
            cout << "ERROR: " << errMsg << endl;
        }

        return false;
    }

    return true;
}

bool PackageDb::isPackageInstalled(Package &pck) {
    vector<Package> installed = listInstalled(pck);

    if (installed.empty()) {
        return false;
    }

    return true;
}

bool PackageDb::markInstalled(Package &pck) {
    if (!findPackage(pck, false)) {
        cout << "Package does not exist: " << pck.name << " " << pck.version << endl;
        return false;
    }

    vector<Package> installed = listInstalled(pck);

    if (!installed.empty()) {
        cout << "Package is already installed: " << pck.name << " " << pck.version << endl;
        return false;
    }

    int rc = sqlite3_exec(DB, string("insert into installed values(" + pck.id + ", '" + 
            pck.name + "', '" + pck.version + "')").c_str(), PackageDb::fetchCallback, NULL, NULL);

    if (rc != SQLITE_OK) {
        cout << "Failed to add installed: " << rc << endl;
        return false;
    }

    cout << "Package marked as installed: " << pck.name << " " << pck.version << endl; 

    return true;
}

bool PackageDb::installPackage(Package &pck) {

    if (isPackageInstalled(pck)) {
        cout << "Package already installed: " << pck.name << " " << pck.version << endl;
        return true;
    }

    cout << "Installing package: " << pck.name << " " << pck.version << "..." << endl;

    vector<Package> deps = listDependencies(pck);

    for (auto &p : deps) {
        if (!isPackageInstalled(p)) {
            cout << "Installing dependency " << p.name << " " << p.version << "..." << endl;
            if (!installPackage(p)) {
                return false;
            }
        }
    }

    return PackageInstaller::inst().install(pck);
}

vector<Package> PackageDb::listInstalled(Package &pck) {
    vector<Package> packages;
    vector<std::map<string, string>> rows;

    string query = "select * from installed";

    if (pck.name != "") {
        query += " where name = '" + pck.name + "'";

        if (pck.version != "") {
            query += " and version = '" + pck.version + "'";
        }
    }

    sqlite3_exec(DB, string(query).c_str(), 
            PackageDb::fetchCallback, &rows, NULL);

    for (uint32_t i=0; i<rows.size(); i++) {
        Package pck;

        pck.id = rows[i]["id"];
        pck.name = rows[i]["name"];
        pck.version = rows[i]["version"];

        packages.push_back(pck);
    }

    return packages;
}

bool PackageDb::findPackage(Package &pck, bool showNotFoundMsg) {
    vector<std::map<string, string>> rows;
    int rc = 0;
    char *msg = 0;

    if (pck.version == "")
        rc = sqlite3_exec(DB, string("select * from packages where name = '" + pck.name + "'").c_str(), 
            PackageDb::fetchCallback, &rows, &msg);
    else 
        rc = sqlite3_exec(DB, string("select * from packages where name = '" + pck.name + "' and version = '" + pck.version + "'").c_str(), 
            PackageDb::fetchCallback, &rows, &msg);

    if (rows.empty()) {
        if (showNotFoundMsg) {
            cout << "No package found: " << pck.name << " " << pck.version << endl;
        }

        return false;
    }

    if (rows.size() != 1) {
        cout << "Multiple packages found for package: " << pck.name << endl;

        for (size_t i=0; i<rows.size(); i++) {
            cout << "\t" << rows[i]["version"] << endl;
        }

        return false;
    }

    pck.id = rows[0]["id"];
    pck.name = rows[0]["name"];
    pck.version = rows[0]["version"];
    pck.file = rows[0]["file"];

    return true;
}

string PackageDb::getPackagesPath() {
    vector<std::map<string, string>> rows;

    sqlite3_exec(DB, string("select * from config where key = 'packages_path'").c_str(), 
            PackageDb::fetchCallback, &rows, NULL);

    if (rows.empty()) {
        cout << "No DB Configuration" << endl;
        return "";
    }

    string path = rows[0]["value"];

    if (!std::filesystem::is_directory(path)) {
        cout << "Invalid DB Configuration" << endl;
        return "";
    }

    return path;
}

bool PackageDb::packageFileExists(const string &file) {

    vector<std::map<string, string>> rows;

    sqlite3_exec(DB, string("select * from config where key = 'packages_path'").c_str(), 
            PackageDb::fetchCallback, &rows, NULL);

    if (rows.empty()) {
        cout << "No DB Configuration" << endl;
        return false;
    }

    string path = rows[0]["value"];

    if (!std::filesystem::is_directory(path)) {
        cout << "Invalid DB Configuration" << endl;
        return false;
    }

    return std::filesystem::exists(path + "/" + file);
}

bool PackageDb::insertPackage(Package pck, vector<Package> &deps) {
    string msg;

    bool failed = false;

    if (findPackage(pck, false)) {
        failed = true;
        cout << "Package already exists: " << pck.name << " " << pck.version << endl;
    }

    for (size_t i=0; i<deps.size(); i++) {
        if (!findPackage(deps[i])) {
            failed = true;
        }
    }

    if (failed) {
        return false;
    }

    if (pck.file == "") {
        pck.file = pck.name + "-" + pck.version + "-plx-x86_64.tar.xz";
    }

    if (!packageFileExists(pck.file)) {
        cout << endl << "WARNING: File missing: " << pck.file << endl << endl;
    }

    if (!dbInsert("insert into packages ( name, version, file) values('" + pck.name + "', '" + pck.version + "', '" + pck.file + "')")) {
        cout << "Failed to insert package: " << lastErrorMsg << endl;
        return false;
    }

    std::stringstream ss;
    ss << sqlite3_last_insert_rowid(DB);
    pck.id = ss.str();

    cout << "Inserted Package: " << pck.name << endl;

    for (size_t i=0; i<deps.size(); i++) {
        if (!dbInsert("insert into dependencies ( pck_id, dep_id) values(" + pck.id + ", " + deps[i].id + ")")) {
            cout << "Failed to relate dependent package: " << lastErrorMsg << endl;
            failed = true;
        }
    }

    return !failed;
}

vector<Package> PackageDb::listDependencies(Package &pck) {
    vector<Package> pcks;

    if (!findPackage(pck)) {
        cout << "Invalid Package: " << pck.name << " " << pck.version << endl;
        return pcks;
    }

    vector<std::map<string, string>> rows;

    sqlite3_exec(DB, string("select * from dependencies where pck_id = " + pck.id).c_str(), 
            PackageDb::fetchCallback, &rows, NULL);

    for (uint32_t i=0; i<rows.size(); i++) {
        Package pck;

        pck.id = rows[i]["id"];
        pck.name = rows[i]["name"];
        pck.version = rows[i]["version"];
        pck.file = rows[i]["file"];

        pcks.push_back(pck);
    }

    return pcks;
}

bool PackageDb::validate() {
    bool success = true;
    vector<std::map<string, string>> rows;

    sqlite3_exec(DB, string("select * from config where key = 'packages_path'").c_str(), 
            PackageDb::fetchCallback, &rows, NULL);

    if (rows.empty()) {
        cout << "No DB Configuration" << endl;
        return false;
    }

    string path = rows[0]["value"];

    if (!std::filesystem::is_directory(path)) {
        cout << "Invalid DB Configuration" << endl;
        return false;
    }

    rows.clear();
    sqlite3_exec(DB, string("select * from packages").c_str(), 
            PackageDb::fetchCallback, &rows, NULL);

    if (rows.empty()) {
        cout << "No packages" << endl;
        return false;
    }

    for (size_t i = 0; i < rows.size(); i++) {
        if (!std::filesystem::exists(path + "/" + rows[i]["file"])) {
            cout << "Missing file " << rows[i]["file"] << endl;
            success = false;
        }
    }

    if (access("/tmp/pullit/tmp.tar.gz", F_OK ) == -1) {
        cerr << "Failed to download file" << endl;
        exit(-1);
    }

    return success;
}

}


