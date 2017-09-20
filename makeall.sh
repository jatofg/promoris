#!/bin/bash
cd ProMoRIS
make
cd ../ProMoRIS_modIperf3
make
cd ../ProMoRIS_modNethogs
make
cd ../ProMoRIS_modTop
make
cd ../ResourceUtilizer
make
cd ..
mkdir dist
cp ProMoRIS/dist/Debug/GNU-Linux/promoris dist/promoris
cp ProMoRIS_modIperf3/dist/Debug/GNU-Linux/libProMoRIS_modIperf3.so dist/modIperf3.so
cp ProMoRIS_modNethogs/dist/Debug/GNU-Linux/libProMoRIS_modNethogs.so dist/modNethogs.so
cp ProMoRIS_modTop/dist/Debug/GNU-Linux/libProMoRIS_modTop.so dist/modTop.so
cp ResourceUtilizer/dist/Debug/GNU-Linux/resourceutilizer dist/resourceutilizer
