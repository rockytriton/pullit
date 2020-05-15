#include "package_db.h"

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

vector<Package> PackageDb::listDependencies(Package pck) {
    vector<Package> pcks;

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


