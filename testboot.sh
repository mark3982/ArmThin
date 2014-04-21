#!/bin/sh
#qemu-system-arm -m 64 -kernel armos.bin -serial stdio
/home/kmcguire/qemu/qemu-2.0.0-rc0/arm-softmmu/qemu-system-arm -M realview-pbx-a9 -smp 4 -m 64 -kernel armos.bin -serial stdio -nographic
