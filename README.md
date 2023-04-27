# HEDALS

HEDALS: highly efficient delay-driven approximate logic synthesis.

HEDALS optimizes the delay of approximate circuits under a given error constraint, such as error rate (ER) constraint, mean error distance (MED) constraint, and mean hamming distance (MHD) constraint. Its diagram is as follows.

<img src="images/ALS.png" alt="ALS" style="zoom: 50%;" />

## Publication

* [C. Meng, Z. Zhou, Y. Yao, S. Huang, Y. Chen, W. Qian, “HEDALS: Highly Efficient Delay-driven Approximate Logic Synthesis,” in IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems (TCAD), 2023.](https://ieeexplore.ieee.org/document/10104162)


## Dependencies 

- Reference environment, **Ubuntu 20.04 LTS** with the following tools and libraries:

  - gcc 10.3.0 & g++ 10.3.0

    You can install these tools with the following command:

    ```shell
    sudo apt install gcc-10
    sudo apt install g++-10
    ```

    You also need to check whether the default versions of gcc and g++ are 10.3.0:

    ```shell
    gcc --version
    g++ --version
    ```

    If the default versions of gcc and g++ are not 10.3.0, please change them to 10.3.0.

  - [cmake](https://cmake.org/) 3.16.3

    You can install the tool using the following command:

    ```shell
    sudo apt install cmake
    ```

  - [libboost](https://www.boost.org/) 1.75.0

    You can download libboost 1.75.0, manually compile it, and then install it.

  - libreadline 8.0-4

    You can install the library using the following command

    ```shell
    sudo apt install libreadline-dev
    ```

- **Alternatively, we package a docker image containing the above dependencies:**

  https://hub.docker.com/r/changmeng/als_min

## How to Download

HEDALS relies on three submodules:

- Open-source logic synthesis and verification tool [abc](https://github.com/changmg/abc/tree/f1b64be84071a431eac9871f8cdc71bc912fd75a)
- Open-source logic synthesis tool [espresso](https://github.com/changmg/espresso/tree/71e028a63f5b37814ef4bd6f7cdd602537d4c4ce)
- Open-source approximate circuit library [EvoApproxLib](https://github.com/changmg/evoapproxlib/tree/d8a5820613573dddbe474a440d3a0ff7ce1f7fd9)

There are two ways of downloading HEDALS:

1. Clone the HEDALS project, and then update the submodules:

```shell
git clone https://github.com/SJTU-ECTL/HEDALS.git
git submodule init
git submodule update
```

2. Alternatively, clone the HEDALS project as well as the submodules: 

```shell
git clone --recursive https://github.com/SJTU-ECTL/HEDALS.git
```

## How to Build 

- To build, go to the root directory of the project, and then execute:

```shell
mkdir build
cd build
cmake ..
make
cd ..
```

An executable program called *hedals.out* will be generated at the project root directory.

- To clean up, go to the root directory of the project, and then execute:

```shell
rm -r build
```

## How to Run

- Example command:

```shell
./hedals.out -i ./input/benchmark/bacs/mult8.blif -l ./input/standard-cell/nangate_45nm_typ.lib --lacType RAC -f 100032 --metrType MED -e 128 -m 0 -o ./tmp
```

In this example,

- HEDALS inputs the accurate circuit "./input/benchmark/bacs/mult8.blif".
- It uses the standard cell library "./input/standard-cell/nangate_45nm_typ.lib" for technology mapping. 
- It uses the "RAC" local approximate change to simplify the circuit.
- It uses 100032 random input patterns to measure the error.
- The error constraint is $MED \le 128$
- It uses mode 0, that is, applying the priority cut-based method proposed in our paper.
- The approximate circuits will be outputted to the folder ./tmp

## Configurations

- Use the following command to get help:

```shell
./hedals.out -h
```

The following information will be returned:

```shell
usage: ./hedals.out --accCirc=string [options] ... 
options:
  -i, --accCirc        path to accurate circuit (string)
      --appCirc        path to approximate circuit (string [=])
  -l, --standCell      path to standard cell library (string [=input/standard-cell/mcnc.genlib])
  -o, --outpPath       path to approximate circuits (string [=tmp])
      --lacType        lac type: CONST, SASIMI, RAC (string [=CONST])
      --metrType       error metric type: ER, MED, MHD (string [=ER])
      --distrType      error distribution type: UNIF, ENUM (string [=UNIF])
      --mapType        mapping type: SCL, LUT (string [=SCL])
  -s, --sourceSeed     seed for randomness (unsigned int [=0])
  -f, --nFrame         #Monte Carlo samples, nFrame should be an integer multiple of 64 (int [=100032])
  -m, --mode           mode selection, 0 or 1
			0: priority cut
			1: maximum flow (int [=0])
      --usePostProc    whether to use post processing for further reducing the area (int [=0])
      --cutSizeLim     size limit of priority cuts (int [=16])
  -e, --errUppBound    error upper bound (double [=0.15])
  -h, --help           print this message
```

| Long parameter | Short parameter | Default value                   | Function                                                     |
| -------------- | --------------- | ------------------------------- | ------------------------------------------------------------ |
| --accCirc      | -i              | None                            | Path to accurate circuit, e.g., *./input/benchmark/bacs/mult8.blif* |
| --appCirc      |                 | ""                              | Path to approximate circuit. When "--appCirc" specifies a non-empty string, then the program will iteratively simplify the accurate circuit provided by "--accCirc". Otherwise, if "--appCirc" specifies an empty string "", then the program will report the error of the approximate circuit provided by "--appCirc", compared to the accurate circuit provided by "--accCirc". |
| --standCell    | -l              | input/standard-cell/mcnc.genlib | Path to standard cell library, e.g., *./input/standard-cell/nangate_45nm_typ.lib* |
| --outpPath     | -o              | tmp                             | Specify the directory where the approximate circuits are outputed. |
| --lacType      |                 | CONST                           | The type of local approximate changes. There are three types: CONST, SASIMI, RAC. CONST means constant replacement. SASIMI means signal replacement. RAC means approximate resubstitution. Please refer to our paper to see more details about the LAC type. |
| --metrType     |                 | ER                              | The type of error metrics. There are three metrics: ER, MED,  MHD. Please refer to our paper to see their detailed definition. |
| --distrType    |                 | UNIF                            | The type of input distribution. There are two types. UNIF means uniform distribution. ENUM means  enumerating all possible input patterns (only for small circuits). |
| --mapType      |                 | SCL                             | The type of final mapping target. There are two types. SCL means mapping into standard cell library. LUT means mapping into look up tables. |
| --sourceSeed   | -s              | 0                               | The seed used for generating random input patterns for logic simulation. When seed "0" is used, then HEDALS randomly picks a seed. Otherwise, HEDALS uses the specified seed. |
| --nFrame       | -f              | 100032                          | The number of random input patterns used for error measurement. |
| --mode         | -m              | 0                               | Mode selection. There are two modes: 0, 1. Mode 0 corresponds to the priority cut method proposed in our paper. Mode 1 corresponds to the maximum flow method proposed in our paper. |
| --usePostProc  |                 | 0                               | Whether to use post processing for further reducing the area. When it is 0, post processing is disabled. When it is 1, post processing is enabled. |
| --cutSizeLim   |                 | 16                              | Size limit of priority cuts. Its detailed meaning is defined in our paper. |
| --errUppBound  | -e              | 0.15                            | Error upper bound used for approximate logic synthesis. HEDALS will generate an approximate circuits satisfying the bound. |
| --help         | -h              | None                            | Print help.                                                  |

