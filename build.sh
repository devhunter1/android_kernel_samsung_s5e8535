#!/bin/bash

if [ ! -d "$PWD/kernel_build" ] || [ -z "$(ls -A "$PWD/kernel_build")" ]; then
    echo "kernel_build directory is missing or empty. cloning ..."
    git clone https://github.com/s5e8535/kernel_build_script.git kernel_build --depth=1
fi

if [ -z "$DEVICE" ]; then
    echo "Error: Target device is not set."
    echo "You can set it by: export DEVICE=*"
    echo "exiting.."
    exit 1
fi

./kernel_build/build.sh "$(pwd)" || exit 1
