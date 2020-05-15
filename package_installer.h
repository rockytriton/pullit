#pragma once

#include "common.h"

namespace pullit {

class PackageInstaller {
public:
    static PackageInstaller &inst();
    ~PackageInstaller();

    bool install(Package &pck);

protected:
    PackageInstaller();

private:
    bool extract(string &file);

    const string tempPath = "/tmp/pullit/";
};


}

