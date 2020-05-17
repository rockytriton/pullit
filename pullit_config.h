#pragma once
#include "common.h"

namespace pullit
{

class Config {
public:
    string getDb() {
        return getProp("db.path");
    }

    string getProp(string key) {
        return props[key];
    }

    static Config &inst() {
        static Config INST;
        return INST;
    }

private:
    Config();
    std::map<string, string> props;
};



}
