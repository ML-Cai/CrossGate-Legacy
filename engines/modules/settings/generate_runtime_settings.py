import json
import sys
from pathlib import Path

# ------------------------------------------------------------------------------
class HeaderGenerator:
    def __init__(self):
        self.lines = []

    def append(self, line):
        self.lines.append(line)

    def extend(self, lines):
        self.lines.extend(lines)

    def to_string(self):
        return "\n".join(self.lines)

    def save_to_file(self, output_file):
        Path(output_file).write_text(self.to_string(), encoding="utf-8")


    def generate_header_prefix(self):
        self.append(
"""
// =============================================================================
// -- Warning !!     Warning !!     Warning !!     Warning !!     Warning !! --
//
// This file is auto-generated. Do not edit directly.
//
// -- Warning !!     Warning !!     Warning !!     Warning !!     Warning !! --
// =============================================================================

#pragma once

#include <cstdint>
#include <string>

namespace cgl {
namespace _runtime_settings {
""")

    def generate_category_for_header_constructor(self, category, entries):
        self.append(f"    {category}() {{")

        for entry in entries:
            ctype = entry["Type"]
            name = entry["Name"]
            default = entry["Default"]
            if ctype == "std::string":
                self.append(f"        {name} = \"{default}\";")
            else:
                self.append(f"        {name} = {default};")

        self.append(f"    }}")


    def generate_category_for_header(self, settings):
        for category, entries in settings.items():
            self.append(f"// `{category}` settings")
            self.append(f"struct {category} {{")

            self.generate_category_for_header_constructor(category, entries)

            # append equality operator
            self.append(f"    bool operator==(const {category}& other) const;")

            for entry in entries:
                ctype = entry["Type"]
                name = entry["Name"]
                desc = entry.get("Description", "")
                self.append(f"    // {desc}")
                self.append(f"    {ctype} {name};")
            self.append("};\n")

        self.append("}   // namespace _runtime_settings")

    #----------------------------------------------------------------------------
    def generate_setting_struct(self, settings):
        self.append("struct RuntimeSettings {")

        # append equality operator
        self.append(f"    bool operator==(const cgl::RuntimeSettings& other) const;")

        # Generate category defines
        for category, entries in settings.items():
            self.append(f"    cgl::_runtime_settings::{category} {category.lower()};")

        self.append("};     // struct RuntimeSettings")
        self.append("}      // namespace cgl")

# ------------------------------------------------------------------------------
def generate_header(json_file, output_file):
    with open(json_file, "r") as f:
        data = json.load(f)

    settings = data["RuntimeSettings"]

    generator = HeaderGenerator()
    generator.generate_header_prefix()
    generator.generate_category_for_header(settings)
    generator.generate_setting_struct(settings)
    generator.save_to_file(output_file)


# ------------------------------------------------------------------------------
def generate_ini(json_file, output_file):
    with open(json_file, "r") as f:
        data = json.load(f)

    settings = data["RuntimeSettings"]

    ini = []
    for category, entries in settings.items():
        ini.append(f"[{category}]")
        for entry in entries:
            if entry.get("ExportToINI", False):
                name = entry["Name"]
                default = entry["Default"]
                ini.append(f"{name}={default}")
        ini.append("")

    Path(output_file).write_text("\n".join(ini), encoding="utf-8")



# ------------------------------------------------------------------------------
class CPPGenerator:
    def __init__(self):
        self.lines = []

    def append(self, line):
        self.lines.append(line)

    def extend(self, lines):
        self.lines.extend(lines)

    def to_string(self):
        return "\n".join(self.lines)

    def save_to_file(self, output_file):
        Path(output_file).write_text(self.to_string(), encoding="utf-8")

    def generate_cpp_prefix(self):
        self.append(
"""// =============================================================================
// -- Warning !!     Warning !!     Warning !!     Warning !!     Warning !! --
//
// This file is auto-generated. Do not edit directly.
//
// -- Warning !!     Warning !!     Warning !!     Warning !!     Warning !! --
// =============================================================================

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include "cgl/utils/ini.h"
#include "cgl/settings/settings.h"

""")

    def generate_category_for_cpp_equals(self, settings):
        for category, entries in settings.items():
            self.append(f"bool cgl::_runtime_settings::{category}::operator==(const cgl::_runtime_settings::{category}& other) const {{")
            comparisons = []
            for entry in entries:
                name = entry["Name"]
                comparisons.append(f"{name} == other.{name}")
            self.append("    return " + " && ".join(comparisons) + ";")
            self.append("}")

    def generate_equal(self, settings):
        self.append("bool cgl::RuntimeSettings::operator==(const cgl::RuntimeSettings& other) const {")
        for category, entries in settings.items():
            self.append(f"    if (this->{category.lower()} != other.{category.lower()}) return false;")

        self.append("    return true;")
        self.append("}")

    def generate_constructor(self, settings):

        self.append(
"""
cgl::RuntimeSettingsPtr cgl::LoadRuntimeSettings(
    const std::string_view int_path
) noexcept {
    cgl::RuntimeSettingsPtr settings = std::make_unique<cgl::RuntimeSettings>();
    cgl::INI ini;

    if (int_path.empty()) {
        return settings;
    }

    if (!cgl::LoadIniFile(std::string(int_path), &ini)) {
        std::cerr << "Error: Failed to load INI file: " << int_path << std::endl;
        return nullptr;
    }
""")
        for category, entries in settings.items():
            self.append(f"    // Load settings for category: {category}")
            self.append(f"    if (ini.find(\"{category}\") != ini.end()) {{")
            self.append(f"        const auto& section = ini[\"{category}\"];")
            for entry in entries:
                ctype = entry["Type"]
                name = entry["Name"]
                if ctype == "std::string":
                    self.append('        if (section.find("' + name + '") != section.end()) {')
                    self.append(f'            settings->{category.lower()}.{name} = section.at("{name}");')
                    self.append("        }")
                elif ctype == "bool":
                    self.append('        if (section.find("' + name + '") != section.end()) {')
                    self.append(f'            settings->{category.lower()}.{name} = (section.at("{name}") == "1" || section.at("{name}") == "true");')
                    self.append("        }")
                elif ctype in ["int", "int32_t", "int64_t", "uint32_t", "uint64_t"]:
                    self.append('        if (section.find("' + name + '") != section.end()) {')
                    self.append(f'            settings->{category.lower()}.{name} = static_cast<{ctype}>(std::stoll(section.at("{name}")));')
                    self.append("        }")
                elif ctype in ["float", "double"]:
                    self.append('        if (section.find("' + name + '") != section.end()) {')
                    self.append(f'            settings->{category.lower()}.{name} = static_cast<{ctype}>(std::stod(section.at("{name}")));')
                    self.append("        }")
                else:
                    raise ValueError(f"Unsupported type: {ctype}")
            self.append("    }")
            self.append("")

        self.append(
    """
    return settings;
}
    """)

# ------------------------------------------------------------------------------
# generate cpp to read ini file and populate RuntimeSettings struct
def generate_cpp(json_file, output_file):
    with open(json_file, "r") as f:
        data = json.load(f)

    settings = data["RuntimeSettings"]

    generator = CPPGenerator()
    generator.generate_cpp_prefix()
    generator.generate_category_for_cpp_equals(settings)
    generator.generate_equal(settings)
    generator.generate_constructor(settings)
    generator.save_to_file(output_file)


# ------------------------------------------------------------------------------
if __name__ == "__main__":
    generate_header(sys.argv[1], sys.argv[2])
    generate_cpp(sys.argv[1], sys.argv[3])
    generate_ini(sys.argv[1], sys.argv[4])
    print(f"Header generated at {sys.argv[2]}")
