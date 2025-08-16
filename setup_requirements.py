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
import argparse

# ----------------------------------------------------------------------------------------------------------------------
def run_cmd(cmd, cwd=None):
    print(f"Running command: {cmd}")
    result = subprocess.run(cmd, cwd=cwd, text=True)

    if result.returncode != 0:
        print(result.stderr)
        raise subprocess.CalledProcessError(result.returncode, cmd)

# ----------------------------------------------------------------------------------------------------------------------
def build_cmake_depend(workspace, build_type, depend_name, *cmake_args):
    soruce_path  = os.path.join(workspace, "third_party", depend_name)
    install_path = os.path.join(workspace, "third_party", "build", build_type)
    build_path   = os.path.join(workspace, "build", "third_party", depend_name)

    print("-------------------------------------------------------------------")
    print(f"Build depend: {soruce_path} ...")
    print("-------------------------------------------------------------------")

    cmd = [
        "cmake",
            "-S", soruce_path,
            "-B", build_path,
            f"-DCMAKE_INSTALL_PREFIX={install_path}",
            f"-DCMAKE_BUILD_TYPE={build_type}"
        ]
    cmd.extend(cmake_args)
    run_cmd(cmd, cwd = soruce_path)

    run_cmd([
        "cmake",
            "--build", build_path,
            "--target", "install",
            "--config", build_type,
        ],
        cwd = soruce_path)

# ----------------------------------------------------------------------------------------------------------------------
def install_python_requirements(workspace):
    requirements_path = os.path.join(workspace, "python", "requirements.txt")
    run_cmd([
        sys.executable, "-m", "pip", "install", "-r", requirements_path])

# ----------------------------------------------------------------------------------------------------------------------
def main(args):
    workspace    = os.path.dirname(os.path.abspath(__file__))

    # update submodules
    print("-------------------------------------------------------------------")
    print(f"Update submodules in workspace: {workspace}")
    print("-------------------------------------------------------------------")
    run_cmd(["git", "submodule", "update", "--init", "--recursive"], cwd=workspace)

    # install python dependencies
    print("-------------------------------------------------------------------")
    print(f"Install Python requirements in workspace: {workspace}")
    print("-------------------------------------------------------------------")
    install_python_requirements(workspace)

    # build third-party dependencies
    print("-------------------------------------------------------------------")
    print(f"Build third-party dependencies to the path: {workspace}")
    print("-------------------------------------------------------------------")
    build_cmake_depend(workspace, args.build_type,
                       "googletest",
                       "-Dgtest_force_shared_crt=ON")

    build_cmake_depend(workspace, args.build_type,
                       "spdlog")

# ----------------------------------------------------------------------------------------------------------------------
if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--build_type",
                        type=str,
                        choices=["Debug", "Release", "RelWithDebInfo"],
                        default="RelWithDebInfo",
                        help="Specify build type")
    args = parser.parse_args()

    main(args)