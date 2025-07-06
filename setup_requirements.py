# ======================================================================================================================
#   The MIT License (MIT)
#
#   Copyright (c) 2024-2025 MengLun,Cai
#
#   All rights reserved.
# ======================================================================================================================
import os
import sys
import subprocess
import shutil

# ----------------------------------------------------------------------------------------------------------------------
def run_cmd(cmd, cwd=None):
    result = subprocess.run(
        cmd,
        cwd=cwd,
        capture_output=True,
        text=True
    )

    if result.returncode != 0:
        print(result.stderr)
        raise subprocess.CalledProcessError(result.returncode, cmd)

# ----------------------------------------------------------------------------------------------------------------------
def fetch_repository(dep_name, url, branch, temp_dir):
    dep_path = os.path.join(temp_dir, dep_name)

    if not os.path.exists(dep_path):
        print(f"-- Fetch {dep_name}:{branch} from `{url}`")
        run_cmd(["git", "clone", "--depth", "1", "--branch", branch, url, dep_path])

    return dep_path

# ----------------------------------------------------------------------------------------------------------------------
def build_dep(dep_name, install_dir, dep_path, build_type, *cmake_args):
    cmake_cmd = ["cmake",
                    "-S", dep_path,
                    "-B", "build",
                    f"-DCMAKE_INSTALL_PREFIX={install_dir}",
                    f"-DCMAKE_BUILD_TYPE={build_type}",]
    cmake_cmd.extend(cmake_args)
    run_cmd(cmake_cmd, cwd=dep_path)

    run_cmd(["cmake",
                "--build", "build",
                "--target", "install",
                "--config", build_type,
            ], cwd=dep_path)

# ----------------------------------------------------------------------------------------------------------------------
def setup_dep(install_dir, temp_dir, dep_name, url, branch, build_type, *cmake_args):
    print(f"-- Setup dependency : `{dep_name}:{branch}` ...")

    # fetch source from repository
    dep_path = fetch_repository(dep_name, url, branch, temp_dir)

    # check build
    build_dep(dep_name, install_dir, dep_path, build_type, *cmake_args)

# ----------------------------------------------------------------------------------------------------------------------
def main():
    if len(sys.argv) != 4:
        print("Usage: setup_gtest.py <install_dir> <temp_dir> <build_type>")
        sys.exit(1)

    install_dir = os.path.abspath(sys.argv[1])
    temp_dir    = os.path.abspath(sys.argv[2])
    build_type  = sys.argv[3]

    install_dir = os.path.join(install_dir, build_type)

    setup_dep(install_dir, temp_dir,
              "googletest", "https://github.com/google/googletest.git", "v1.17.0", build_type,
              "-Dgtest_force_shared_crt=ON")

    setup_dep(install_dir, temp_dir,
              "spdlog", "https://github.com/gabime/spdlog.git", "v1.15.3", build_type)

# ----------------------------------------------------------------------------------------------------------------------
if __name__ == "__main__":
    main()