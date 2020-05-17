#include "pullit_config.h"

#include <fstream>

namespace pullit {

Config::Config() {
    std::ifstream in("/etc/pullit.conf");
    string line;

    while(std::getline(in, line)) {
        int firstEq = line.find_first_of('=');
        
        if (firstEq == string::npos || line[0] == '#') {
            continue;
        }

        props[line.substr(0, firstEq)] = line.substr(firstEq + 1);
    }
}

}
