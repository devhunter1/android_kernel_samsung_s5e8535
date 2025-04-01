#!/bin/bash

if [ ! -d "$pwd/kernel_build" ] || [ -z "$(ls -A $pwd/kernel_build)" ]; then
    echo "kernel_build directory is missing or empty. cloning from submodule"
    git submodule update --init
fi

./kernel_build/build.sh "$(pwd)" || exit 1
