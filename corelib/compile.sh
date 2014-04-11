#!/bin/sh
arm-eabi-gcc -c -nostdlib -nostartfiles -ffreestanding -std=gnu99 core.c
arm-eabi-gcc -c -nostdlib -nostartfiles -ffreestanding -std=gnu99 rb.c
