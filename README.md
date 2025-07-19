# CrossGate-Legacy

The project is inspired by the page : https://cgsword.com/filesystem_graphicmap.htm which is a great resource for learning how to decode the data of Cross Gate an online game that has been popular since 2000.

This project is not a private server or an attempt to infringe upon the rights of SOFTSTAR ENTERTAINMENT INC. (https://cg.softstar.com.tw/). It is simply a fan-made implementation for non-commercial use.


## Prerequisites

- Install and play Cross-Gate : https://cg.softstar.com.tw/

    > To test the CrossGate-Legacy project, please first enjoy the original Cross Gate game by installing and playing it from https://cg.softstar.com.tw/. As you explore the CG world and visit different maps, you will collect the map data that CrossGate-Legacy relies on.

- Python 3.10 or higher
- Python Development Headers
- Visual C++ Redistributable for Windows build
- miniforge


## Build

### 1. Clone the repository

    git clone https://github.com/ML-Cai/CrossGate-Legacy.git
    cd CrossGate-Legacy

### 2. Install requirements (optional to build bin/*)

    conda create name cgl python=3.10
    conda activate cgl
    pip install -r python/requirements.txt

### 3. Building CGL

    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
    cmake --build . --config RelWithDebInfo

## Run

- GraphicsDataViewer : see [bin\GraphicsDataViewer](bin/GraphicsDataViewer)