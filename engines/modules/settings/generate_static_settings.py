import json
import sys
from pathlib import Path


# ------------------------------------------------------------------------------
def parse_category_settings(header, category, entries):
    header.append(
f"""
// `{category}` settings
struct {category} {{
""")

    for entry in entries:
        ctype = entry["Type"]
        name = entry["Name"]
        default = entry["Default"]
        desc = entry.get("Description", "")
        header.append(f"    // {desc}")
        if ctype == "std::string_view":
            header.append(f"    static constexpr {ctype} {name} = \"{default}\";")
        elif ctype == "uint8_t":
            header.append(f"    static constexpr {ctype} {name} = {default};")
        else:
            header.append(f"    // Unsupported type: {ctype}")
    header.append("};\n")

# ------------------------------------------------------------------------------
def generate_header(json_file, output_file):
    with open(json_file, "r") as f:
        data = json.load(f)

    settings = data["StaticSettings"]

    header = []
    header.append(
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
#include <string_view>

namespace cgl {
namespace Settings {
""")

    # Generate category structs
    for category, entries in settings.items():
        parse_category_settings(header, category, entries)

    header.append(
"""
}   // namespace Settings
}   // namespace cgl

""")

    Path(output_file).write_text("\n".join(header), encoding="utf-8")

# ------------------------------------------------------------------------------
if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python generator.py input.json output.hpp")
        sys.exit(1)

    generate_header(sys.argv[1], sys.argv[2])
    print(f"Header generated at {sys.argv[2]}")
