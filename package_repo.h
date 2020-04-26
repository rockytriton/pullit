#pragma once
#include "common.h"

#include <libxml/parser.h>

namespace pullit {

class PackageRepo {
public:
    static PackageRepo &inst();
    ~PackageRepo();

    void update();

    void load();

    void install_packages();

protected:
    void load_installed(xmlNode *a_node);

private:
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;

    map<string, shared_ptr<Package>> packageMap;
    map<string, shared_ptr<Package>> installedPackages;

};

};
