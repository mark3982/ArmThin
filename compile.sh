#!/bin/sh
arm-eabi-gcc -s -I../inc -nostdlib -nostartfiles -ffreestanding -std=gnu99 -c *.c
arm-eabi-gcc -s -I../inc -nostdlib -nostartfiles -ffreestanding -std=gnu99 -c kheap_bm.c
arm-eabi-gcc -s -I../inc -nostdlib -nostartfiles -ffreestanding -std=gnu99 -c xarmdiv.c
arm-eabi-gcc -s -I../inc -nostdlib -nostartfiles -ffreestanding -std=gnu99 -c vmm.c
arm-eabi-ld -T link.ld -o __armos.bin main.o kheap_bm.o xarmdiv.o vmm.o
echo doing object copy..
arm-eabi-objcopy -j .text -O binary __armos.bin armos.bin
