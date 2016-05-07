//
// Furos v2.0  -  Copyright Marlet Limited 2012
//
// File:			kernel.c
// Description:		Main kernel entry point
// Author:			James Smith
// Created:			18-Aug-2012
//

#include "kernel.h"

// These are set by the linker script. The values themselves are invalid,
//  but the ADDRESS of each variable is the address we require.
extern DWORD code_start; // First byte of code
extern DWORD bss_start;  // First byte of BSS
extern DWORD bss_end;    // First byte after BSS

extern ThreadTSS *sch_CreateKernelThread( THREADENTRYFUNC EntryPoint );

void StandardDeviceSearch( void )
{
	// PS/2 Keyboard and Mouse
	key_InitDevice();
	mse_InitDevice();

	// Get the equipment word from the BIOS Data Area
	WORD *equip = (WORD*)0x410;

	// Work out what we've got
	DWORD floppy_count = (((*equip) >> 6) & 0x0003) + 1;
	DWORD serial_count = ((*equip) >> 9) & 0x0007;
	DWORD gameport_count = ((*equip) >> 12) & 0x0001;
	DWORD parallel_count = ((*equip) >> 14) & 0x0003;

	kprintf("[DEV] %d Floppy, %d Serial, %d Game, %d Parallel\n",
				floppy_count, serial_count, gameport_count, parallel_count );

	// Floppy controller 1 found at 0x3F0 which can support 2 cables
	//  with 2 drives per cable. There may possibly be another controller
	//  at 0x370, but we aren't going to support that.
	if (floppy_count > 0)
	{
		kprintf("[DEV] Floppy Port 2 at 0x03F0\n");
		flp_InitDevice( 0x3F0 );
	}
	if (floppy_count > 1)   kprintf("[DEV] Floppy Port 2 at 0x0370 [NOT SUPPORTED]\n");

	// Initialize serial ports
	if (serial_count > 0)  kprintf("[DEV] Serial Port 1 at 0x%w\n", *((WORD*)0x400) );
	if (serial_count > 1)  kprintf("[DEV] Serial Port 2 at 0x%w\n", *((WORD*)0x402) );
	if (serial_count > 2)  kprintf("[DEV] Serial Port 3 at 0x%w\n", *((WORD*)0x404) );
	if (serial_count > 3)  kprintf("[DEV] Serial Port 4 at 0x%w\n", *((WORD*)0x406) );

	// Initialize game port

	// Initialize parallel ports
	if (parallel_count > 0)  kprintf("[DEV] Parallel Port 1 at 0x%w\n", *((WORD*)0x408) );
	if (parallel_count > 1)  kprintf("[DEV] Parallel Port 2 at 0x%w\n", *((WORD*)0x40A) );
	if (parallel_count > 2)  kprintf("[DEV] Parallel Port 3 at 0x%w\n", *((WORD*)0x40C) );
	if (parallel_count > 3)  kprintf("[DEV] Parallel Port 4 at 0x%w\n", *((WORD*)0x40E) );
}


void StartupProcess_EntryPoint( void )
{
	exp_DumpKernelTable();
	STOP_PROCESSOR();


	kprintf("STARTUP!!\n");

//	// First look for the standard devices
//	StandardDeviceSearch();
////	net_InitDevice(NULL);
//
//	// Create the usb thread
//	ProcessTSS *usb = sch_CreateKernelThread( (void *)usb_EntryPoint );
//
//	// Now look for any PCI devices
////	pci_DeviceScan( TRUE );
//	pci_DeviceScan( FALSE );
//
//	// Determine the boot device and set that as the current directory
//	fs_SetCurrentDirectory("U:/");
//
//	// Create the console process
//	sch_CreateUserProcess( "CONSOLE.BIN" );
//
//	// Startup complete. End.
//	sch_EndProcess( sch_GetCurrentProcess() );
//

	// Shouldn't get here
	STOP_PROCESSOR();
}


void kernel_main()
{
	// Clear the BSS
	DWORD bssstart = (DWORD)&bss_start;
	DWORD bssend = (DWORD)&bss_end;
	memclr( (void*)bssstart, bssend - bssstart );

	// Setup the console
	con_InitDevice();
	kprintf("Welcome to Furos!\n\n");

	// Sort out some kernel memory
	mem_InitDevice();
	heap_InitDevice();

	// Then look for some basic devices we need
	irq_InitDevice();
	cpu_InitDevice();
//	apic_InitDevice();
	acpi_InitDevice();
	rtc_InitDevice();

	// Look for standard device that we are expecting
//	StandardDeviceSearch();

	// Start the thread scheduler
	sch_InitScheduler();

	// Create the kernel startup task. This is responsible for performing
	//  all the driver initialisation etc....
	ThreadTSS *startup = sch_CreateKernelThread( StartupProcess_EntryPoint );

	// Now sit in the main scheduling loop
	sch_MainLoop();

	// If we get here then the OS is no longer processing
	//  scheduling interrupts, so STOP.
	kprintf("\nSTOPPING\n\n");
	STOP_PROCESSOR();

//	pci_DeviceScan( TRUE );

}
