#!/bin/bash
if [ $(uname -m) != "aarch64" ]; then
    echo "Cross compiling!"
    aarch64-linux-gnu-gcc main.c arm64-asmtests.c -lglfw -lGL -lGLEW -g -o mapping
else
    gcc main.c arm64-asmtests.c -lglfw -lGL -lGLEW -g -o mapping
fi