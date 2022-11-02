# HEDALS

Highly efficient delay-driven approximate logic synthesis.
This tool targets at optimizing the delay of approximate circuits under a given error metric, such as error rate (ER), mean error distance (MED), and mean hamming distance (MHD), as shown in the following figure.

<img src="images/ALS.png" alt="ALS" style="zoom: 50%;" />

HEDALS is a highly efficient flow that can produce an approximate version of a circuit with tens of thousands of gates in several hours.
On some arithmetic circuits from the BACS benchmark suite, HEDALS can reduce delay by 70% under an normalized MED bound of 3%.

# Publications

* TODO
  


# Dependency 

- Ubuntu 20.04 LTS
- gcc 10.3.0
- cmake 3.16.3
- libboost 1.75.0
- libreadline 8.0-4

To pull git submodules in the root directory
```shell
git submodule init
git submodule update
```

Or alternatively, pull all the submodules when cloning the repository. 
```shell
https://github.com/SJTU-ECTL/HEDALS.git
```

# How to Build 

Two options are provided for building: with and without [Docker](https://hub.docker.com). 

## Build with Docker

TODO

## Build without Docker

[CMake](https://cmake.org) is adopted as the makefile system. 
To build, go to the root directory. 

```shell
mkdir build
cd build
cmake ..
make
cd ..
```

An executable program called *hedals.out* will be generated at the project root path.

To clean, go to the root directory. 
```shell
rm -r build
```

# How to Run

TODO

# Configurations

TODO
