#!/bin/sh
export LIBGCC=/home/kmcguire/opt/cross/lib/gcc/arm-eabi/4.8.2/libgcc.a

cd corelib/
./compile.sh
cd ..
cd modules/testuelf
./compile.sh
cd ..
cd fs
./compile.sh

cd ../../

arm-eabi-gcc -s -I../inc -nostdlib -nostartfiles -ffreestanding -std=gnu99 -c *.c
arm-eabi-gcc -s -I../inc -nostdlib -nostartfiles -ffreestanding -std=gnu99 -c kheap_bm.c
arm-eabi-gcc -s -I../inc -nostdlib -nostartfiles -ffreestanding -std=gnu99 -c vmm.c
arm-eabi-gcc -s -I../inc -nostdlib -nostartfiles -ffreestanding -std=gnu99 -c dbgout.c
arm-eabi-gcc -s -I../inc -nostdlib -nostartfiles -ffreestanding -std=gnu99 -c kmod.c
arm-eabi-gcc -s -I../inc -nostdlib -nostartfiles -ffreestanding -std=gnu99 -c ./corelib/rb.c -DKERNEL
arm-eabi-ld -T link.ld -o __armos.bin main.o kheap_bm.o vmm.o dbgout.o kmod.o rb.o $LIBGCC
echo doing object copy..
arm-eabi-objcopy -j .text -O binary __armos.bin armos.bin
./attachmod.py ./modules/fs/fs ./armos.bin 1
./attachmod.py ./modules/testuelf/main ./armos.bin 1
