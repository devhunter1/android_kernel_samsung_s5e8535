#!/bin/bash

if [ ! -d "$pwd/kernel_build" ] || [ -z "$(ls -A $pwd/kernel_build)" ]; then
    echo "kernel_build directory is missing or empty. cloning from submodule"
    git submodule update --init
fi

if [ -z "$DEVICE" ]; then
    echo "Error: Target device is not set."
    echo "You can set it by: export DEVICE=*"
    echo "exiting.."
    exit 1
fi

./kernel_build/build.sh "$(pwd)" || exit 1
