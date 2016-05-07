/*
 * Furos v2.0  -  Copyright Marlet Limited 2012
 *
 * File:			kernel.ld
 * Description:		Linker file for kernel binary
 * Author:			James Smith
 * Created:			18-Aug-2012
 *
 */
 
OUTPUT_FORMAT("binary")
INPUT(obj\startup.o)
INPUT(obj\utils.o)
INPUT(obj\hardware.o)
INPUT(obj\kernel.o)
INPUT(obj\klib.o)
INPUT(obj\exp.o)
INPUT(obj\irq.o)
INPUT(obj\console.o)
INPUT(obj\memory.o)
INPUT(obj\heap.o)
INPUT(obj\cpu.o)
INPUT(obj\rtc.o)
INPUT(obj\keyboard.o)
INPUT(obj\mouse.o)
INPUT(obj\object.o)
INPUT(obj\acpi.o)
INPUT(obj\apic.o)
INPUT(obj\pci.o)
INPUT(obj\sys.o)
INPUT(obj\floppy.o)
INPUT(obj\sch.o)
SECTIONS
{
  .text  0x9000 : {
    *(.text)
  }
  .data  : {
    *(.data)
  }
  _bss_start = .;
  .bss  :
  { 		
    *(.bss)
  }
  _bss_end = .;
}
