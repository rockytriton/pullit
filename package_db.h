#include "common.h"

namespace pullit {

class PackageDb {
public:
    static PackageDb &inst();
    ~PackageDb();

    bool insertPackage(Package pck, vector<Package> &deps);

    vector<Package> listDependencies(Package pck);

    bool findPackage(Package &pck, bool showNotFoundMsg = true);

    bool validate();
    bool packageFileExists(const string &file);
    
protected:
    PackageDb();
    bool dbInsert(string query);

private:
    static int fetchCallback(void *userData, int argc, char **argv, char **colNames);

    void *dbHandle;
    string lastErrorMsg;
};

}

