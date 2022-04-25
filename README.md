## Description

A helper function to make a highpass on a blur's difference, based on an [AviSynth script by Didée](https://forum.doom9.org/showthread.php?p=1584186#post1584186) and originally written by tritical and tp7.

### Requirements:

- AviSynth 2.60 / AviSynth+ 3.4 or later

- Microsoft VisualC++ Redistributable Package 2022 (can be downloaded from [here](https://github.com/abbodi1406/vcredist/releases)) (Windows only)

### Usage:

```
sbr (clip input, int "y", int "u", int "v", int "opt")
```
```
sbrV (clip input, int "y", int "u", int "v", int "opt")
```

### Parameters:

- input\
    A clip to process.\
    Must be in YUV 8..16-bit planar format.

- y, u, v\
    Planes to process.\
    1: Return garbage.\
    2: Copy plane.\
    3: Process plane.\
    Default: y = 3, u = v = 2.

- opt\
    Sets which cpu optimizations to use.\
    -1: Auto-detect.\
    0: Use C++ code.\
    1: Use SSE2 code.\
    2: Use AVX2 code.\
    3: Use AVX512 code.\
    Default: -1.

### Building:

- Windows\
    Use solution files.

- Linux
    ```
    Requirements:
        - Git
        - C++17 compiler
        - CMake >= 3.16
    ```
    ```
    git clone https://github.com/Asd-g/AviSynth-sbr && \
    cd AviSynth-sbr && \
    mkdir build && \
    cd build && \

    cmake ..
    make -j$(nproc)
    sudo make install
    ```
