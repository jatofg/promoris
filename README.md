# ProMoRIS
A Process Monitoring and Resource Information System

## Summary
A tool for monitoring a process using different monitoring tools and combining the results to a single, unified,
and periodic output format that can be evaluated further.

Monitoring tools can be integrated into ProMoRIS by developing a module implementing the abstract class `Module` provided
in the file `ProMoRIS/module.h`.

## Contents
This repository contains the following directories:
- `ProMoRIS`: The main program
- `ProMoRIS_modTop`: ProMoRIS module for the tool `top`
- `ProMoRIS_modNethogs`: ProMoRIS module for the tool `nethogs`
- `ProMoRIS_modIperf3`: ProMoRIS module for the tool `iperf3`
- `ResourceUtilizer`: A small benchmark tool for testing ProMoRIS

## Dependencies
ProMoRIS works on Linux only. For running the modules, the corresponding monitoring tools need to be installed on the machine
and should be located in `/usr/bin/`.

## Building
For building, a version of `g++` supporting the C++14 standard is required.

Switch to the directory of the program you want to compile, and run `make`. The compiled binary will be placed in the  
`./dist/Debug/GNU-Linux/` directory.

## Invoking
- Run `./promoris -h` to get a summary on how to use ProMoRIS.
- Run `./resourceutilizer -h` to get a summary on how to use ResourceUtilizer.
- Place the shared object file (e.g., `module.so`) in the same directory as ProMoRIS and run  
`./promoris -h ./module.so` to get a summary on the options provided by this module.
