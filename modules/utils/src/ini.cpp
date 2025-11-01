// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include "cgl/utils/ini.h"

// -----------------------------------------------------------------------------
namespace {

std::string trim(const std::string& s) {
    const std::string whitespace = " \t\n\r";
    size_t start = s.find_first_not_of(whitespace);
    size_t end = s.find_last_not_of(whitespace);
    if (start == std::string::npos || end == std::string::npos) {
        return "";
    }
    return s.substr(start, end - start + 1);
}

}   // namespace


// -----------------------------------------------------------------------------
bool cgl::LoadIniFile(const std::string& filename, cgl::INI* pIni) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error : Failed to open file : " << filename << std::endl;
        return false;
    }

    std::string line;
    std::string currentSection;

    while (std::getline(file, line)) {
        line = trim(line);

        // skip empty lines and comments
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        // check for section headers
        if (line[0] == '[' && line.back() == ']') {
            currentSection = trim(line.substr(1, line.size() - 2));
            continue;
        }

        // parse key-value pairs
        size_t equalSignPos = line.find('=');
        if (equalSignPos != std::string::npos) {
            std::string key = trim(line.substr(0, equalSignPos));
            std::string value = trim(line.substr(equalSignPos + 1));
            (*pIni)[currentSection][key] = value;
        }
    }

    file.close();
    return true;
}
