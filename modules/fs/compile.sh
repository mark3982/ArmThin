#!/bin/sh
arm-eabi-gcc -I../../ -nostdlib -nostartfiles -ffreestanding -std=gnu99 main.c -c
arm-eabi-ld main.o ../../corelib/core.o ../../corelib/rb.o ../../xarmdiv.o -Ttext 0x80000000 -o fs
