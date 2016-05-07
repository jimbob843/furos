#
# Furos v2.0  -  Copyright Marlet Limited 2012
#
# File:				makefile
# Description:		Main kernel makefile
# Author:			James Smith
# Created:			18-Aug-2012
#
#  ***** RUN USING GNU MAKE *****
#

#
# ALL
#
all : furos_fd.img

#
# CLEAN
#
clean :
	del obj\*.o
	del obj\*.bin
	del furos_fd.img

#
# FAT12 Floppy Image
#
furos_fd.img : obj/fdboot.bin obj/kernel.bin
	copy obj\kernel.bin Bootdisk\kernel.bin
	bfi -o=kernel.bin -b=obj\fdboot.bin -f=furos_fd.img Bootdisk

#
# Bootblocks
#
obj/fdboot.bin: fdboot.asm
	nasm fdboot.asm -o obj\fdboot.bin

#
# Kernel object files
#
OBJ := obj/startup.o obj/utils.o obj/hardware.o obj/kernel.o obj/klib.o \
	obj/exp.o obj/irq.o obj/console.o obj/memory.o obj/heap.o obj/cpu.o \
	obj/rtc.o obj/keyboard.o obj/object.o obj/acpi.o obj/apic.o obj/pci.o \
	obj/mouse.o obj/sys.o obj/floppy.o obj/sch.o

#
# Kernel Binary
#
obj/kernel.bin : $(OBJ)
	ld -E -Map kernel.map -T kernel.ld -o obj\kernel.bin

obj/startup.o : startup.asm
	nasm -f coff startup.asm -o obj\startup.o
	
obj/utils.o : utils.asm
	nasm -f coff utils.asm -o obj\utils.o

obj/hardware.o : hardware.asm
	nasm -f coff hardware.asm -o obj\hardware.o
	
obj/%.o : %.c
	gcc -nostdinc -ffreestanding -c $< -o $@
