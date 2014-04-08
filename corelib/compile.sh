#!/bin/sh
arm-eabi-gcc -c -nostdlib -nostartfiles -ffreestanding -std=gnu99 core.c
