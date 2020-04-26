#pragma once
#include "common.h"

namespace pullit {

class PackageRepo {
public:
    static PackageRepo &inst();
    ~PackageRepo();

    void update();

    void load();

    void install_packages();

protected:
    PackageRepo();

    void load_installed(void *node);
    void build_map(void* node);
    void store_package_info(shared_ptr<Package> pck);
    void extract_file(shared_ptr<Package> pck);
    void install_package(shared_ptr<Package> pck);

private:
    map<string, shared_ptr<Package>> packageMap;
    map<string, shared_ptr<Package>> installedPackages;

};

};
