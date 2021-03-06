#pragma once

#include <string>
#include <iostream>
#include <map>
#include <vector>
#include <memory>

using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::map;
using std::make_shared;
using std::shared_ptr;
using std::cerr;

struct Package {
    string id;
    string name;
    string file;
    string version;
    vector<shared_ptr<Package>> dependencies;
};

struct Options {
    string command;
    vector<string> packages;
};

extern Options g_options;

#define CMD_VALIDATE() (g_options.command == "validate")
#define CMD_INSTALL() (g_options.command == "install")
#define CMD_MARK_INST() (g_options.command == "mark_inst")
#define CMD_UNINSTALL() (g_options.command == "uninstall")
#define CMD_LIST_INSTALLED() (g_options.command == "list_installed")
#define CMD_LIST_ALL() (g_options.command == "list_all")
#define CMD_LIST_MISSING() (g_options.command == "list_missing")
#define CMD_INSTALL_MANUAL() (g_options.command == "install_manual")
#define CMD_ADD_PACKAGE() (g_options.command == "add")

#define STR_EQ(a, b) (string((const char *)a) == string((const char *)b))

std::vector<std::string> split(const std::string &s, char delim);
