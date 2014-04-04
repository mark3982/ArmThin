#!/bin/sh
arm-eabi-gcc -nostdlib -nostartfiles -ffreestanding -std=gnu99 main.c -o main -Ttext 0x80000000
