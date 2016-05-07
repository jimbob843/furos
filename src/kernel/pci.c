/*
 * Furos v2.0   Copyright Marlet Limited 2012
 *
 * File:			pci.c
 * Description:		PCI Interface
 * Author:			James Smith
 * Created:			09-Oct-2012
 *
 */
 
#include "kernel.h"

#define PCIREPORTS

#define PORT_PCI_CONFIG_ADDR 0xCF8  // DWORD port (0xCF8-0xCFB)
#define PORT_PCI_CONFIG_DATA 0xCFC  // DWORD port (0xCFC-0xCFF)

#define NUM_PCI_BUS       8  // ????
#define NUM_PCI_DEVICE   32  // TODO: Double check. Was 64!
#define NUM_PCI_FUNCTION  8

#define VENDOR_REALTEK  0x10EC
#define  DEVICE_RTL8029 0x8029
#define VENDOR_MATROX   0x102B
#define  DEVICE_G200    0x0521
#define VENDOR_NVIDIA   0x10DE
#define  DEVICE_TNT2    0x002D
#define VENDOR_CREATIVE 0x1102
#define  DEVICE_SBLIVE  0x0002

#define PCI_CLASS_LEGACY        0x00
#define PCI_CLASS_STORAGE       0x01
#define PCI_CLASS_NETWORK       0x02
#define PCI_CLASS_DISPLAY       0x03
#define PCI_CLASS_MULTIMEDIA    0x04
#define PCI_CLASS_MEMORY        0x05
#define PCI_CLASS_BRIDGE        0x06
#define PCI_CLASS_COMMS         0x07
#define PCI_CLASS_SYSTEM        0x08
#define PCI_CLASS_INPUT         0x09
#define PCI_CLASS_DOCKING       0x0A
#define PCI_CLASS_PROCESSOR     0x0B
#define PCI_CLASS_SERIALBUS     0x0C
#define PCI_CLASS_WIRELESS      0x0D
#define PCI_CLASS_INTELLIGENTIO 0x0E
#define PCI_CLASS_SATELLITE     0x0F
#define PCI_CLASS_MISC          0xFF



static PCIDeviceInfo *MakePCIDeviceInfo( BYTE bus, BYTE device, BYTE function );
static void WritePCIDeviceInfo( PCIDeviceInfo *info );

	
static DWORD GetConfigDWORD( BYTE bus, BYTE device, BYTE function, BYTE addr ) {
	// bus (0-255), device (0-31), function (0-7), addr (0-63)
	// Reads a DWORD from the PCI Configuration Space

	// Create the configuration address
	DWORD configaddr = 0x80000000;	// bit 31 must be 1 for config. (0 means PCI IO transaction)

	// Put the values into the config addr
	configaddr |= (bus      & 0xFF) << 16;
	configaddr |= (device   & 0x1F) << 11;
	configaddr |= (function & 0x07) <<  8;

	// addr must be DWORD aligned, so bottom two bits are zeros.
	configaddr |= (addr     & 0xFC);

	// Get the current config addr (Do we need this?)
	DWORD origaddr = INPORT_DWORD( PORT_PCI_CONFIG_ADDR );

	// Perform out config transaction
	OUTPORT_DWORD( PORT_PCI_CONFIG_ADDR, configaddr );		// Send address
	DWORD value = INPORT_DWORD( PORT_PCI_CONFIG_DATA );		// Read value

	// Put back the original config addr
	OUTPORT_DWORD( PORT_PCI_CONFIG_ADDR, origaddr );

	return value;
}

void pci_WriteConfigWORD( PCIDeviceInfo *info, BYTE addr, WORD value ) {

	// Create the configuration address
	DWORD configaddr = 0x80000000;	// bit 31 must be 1 for config. (0 means PCI IO transaction)

	// Put the values into the config addr
	configaddr |= (info->bus      & 0xFF) << 16;
	configaddr |= (info->device   & 0x1F) << 11;
	configaddr |= (info->function & 0x07) <<  8;

	// addr must be WORD aligned, so bottom two bits are zeros.
	configaddr |= (addr     & 0xFE);

	// Get the current config addr (Do we need this?)
	DWORD origaddr = INPORT_DWORD( PORT_PCI_CONFIG_ADDR );

	// Perform out config transaction
	OUTPORT_DWORD( PORT_PCI_CONFIG_ADDR, configaddr );		// Send address
	OUTPORT_WORD( PORT_PCI_CONFIG_DATA, value );			// Write value

	// Put back the original config addr
	OUTPORT_DWORD( PORT_PCI_CONFIG_ADDR, origaddr );
}

WORD pci_ReadConfigWORD( PCIDeviceInfo *info, BYTE addr ) {

	// Create the configuration address
	DWORD configaddr = 0x80000000;	// bit 31 must be 1 for config. (0 means PCI IO transaction)

	// Put the values into the config addr
	configaddr |= (info->bus      & 0xFF) << 16;
	configaddr |= (info->device   & 0x1F) << 11;
	configaddr |= (info->function & 0x07) <<  8;

	// addr must be WORD aligned, so bottom two bits are zeros.
	configaddr |= (addr     & 0xFE);

	// Get the current config addr (Do we need this?)
	DWORD origaddr = INPORT_DWORD( PORT_PCI_CONFIG_ADDR );

	// Perform in config transaction
	OUTPORT_DWORD( PORT_PCI_CONFIG_ADDR, configaddr );		// Send address
	WORD value = INPORT_WORD( PORT_PCI_CONFIG_DATA );		// Read value

	// Put back the original config addr
	OUTPORT_DWORD( PORT_PCI_CONFIG_ADDR, origaddr );

	return value;
}


static void SetConfigDWORD( BYTE bus, BYTE device, BYTE function, BYTE addr, DWORD value ) {
	// bus (0-255), device (0-31), function (0-7), addr (0-63)
	// Writes a DWORD from the PCI Configuration Space

	// Create the configuration address
	DWORD configaddr = 0x80000000;	// bit 31 must be 1 for config. (0 means PCI IO transaction)

	// Put the values into the config addr
	configaddr |= (bus      & 0xFF) << 16;
	configaddr |= (device   & 0x1F) << 11;
	configaddr |= (function & 0x07) <<  8;

	// addr must be DWORD aligned, so bottom two bits are zeros.
	configaddr |= (addr     & 0xFC);

	// Get the current config addr (Do we need this?)
	DWORD origaddr = INPORT_DWORD( PORT_PCI_CONFIG_ADDR );

	// Perform out config transaction
	OUTPORT_DWORD( PORT_PCI_CONFIG_ADDR, configaddr );		// Send address
	OUTPORT_DWORD( PORT_PCI_CONFIG_DATA, value );			// Write value

	// Put back the original config addr
	OUTPORT_DWORD( PORT_PCI_CONFIG_ADDR, origaddr );
}


static void LoadDeviceDriver( PCIDeviceInfo *info ) {

	// Load the drivers for known devices
	if ((info->vendorid == VENDOR_REALTEK) && (info->deviceid == DEVICE_RTL8029)) {
//		net_InitDevice( info );
	}
	if ((info->vendorid == VENDOR_MATROX) && (info->deviceid == DEVICE_G200)) {
//		mga_InitDevice( info );
	}
	if ((info->vendorid == VENDOR_NVIDIA) && (info->deviceid == DEVICE_TNT2)) {
//		nv_InitDevice( info );
	}

	// IDE Support
	if ((info->class_code == PCI_CLASS_STORAGE) &&		// Mass-Storage
		(info->subclass_code == 0x01)) {	// IDE
//			ide_InitDevice( info );
	}

	// USB Support
	if ((info->class_code == PCI_CLASS_SERIALBUS) &&       // Serial Bus
		(info->subclass_code == 0x03)) {    // USB

		// Load appropriate generic USB host controller driver
		if (info->interface_code == 0x00) {   // UHCI
//			uhci_InitDevice( info );
		}
		if (info->interface_code == 0x10) {   // OHCI
//			ohci_InitDevice( info );
		}
		if (info->interface_code == 0x20) {   // EHCI
//			ehci_InitDevice( info );
		}
	}

}


void pci_DeviceScan( BOOL report_to_console ) {

	// Interrogate the PCI configuration space to determine what
	//  buses/devices are available. The config space is a special
	//  memory area that is accessed through the ports.
	
	BYTE bus = 0;
	BYTE device = 0;
	BYTE function = 0;

	for (bus=0; bus<NUM_PCI_BUS; bus++) {
		for (device=0; device<NUM_PCI_DEVICE; device++) {

			DWORD x = GetConfigDWORD( bus, device, 0, 0x00 );
			DWORD vendorid = x & 0xFFFF;

			if ((vendorid == 0x0000) || (vendorid == 0xFFFF)) {
				// Not a device
			} else {
				BYTE functioncount = 1;		// Default to single function device
				DWORD y = GetConfigDWORD( bus, device, 0, 0x0C );
				BYTE headertype = (y & 0x00FF0000) >> 16;
				if (headertype & 0x80) {
					// Is multi-function device
					functioncount = NUM_PCI_FUNCTION;
				}

				for (function=0; function<functioncount; function++) {
					DWORD z = GetConfigDWORD( bus, device, function, 0x00 );
					DWORD vendorid = z & 0xFFFF;
					if ((vendorid == 0x0000) || (vendorid == 0xFFFF)) {
						// No device
					} else {
						// We've found a device. Create an info struct for it.
						PCIDeviceInfo *info = MakePCIDeviceInfo( bus, device, function );
						if (report_to_console == TRUE) {
							WritePCIDeviceInfo( info );
						} else {
							LoadDeviceDriver( info );
						}

					}
				}
			}
		}
	}
}


static PCIDeviceInfo *MakePCIDeviceInfo( BYTE bus, BYTE device, BYTE function ) {
	// Populates a new PCIDeviceInfo struct from the config space.
	PCIDeviceInfo *info = (PCIDeviceInfo*)kmalloc( sizeof(PCIDeviceInfo) );

	info->bus = bus;
	info->device = device;
	info->function = function;

	// Get vendor and device IDs
	DWORD deviceID = GetConfigDWORD( bus, device, function, 0x00 );
	info->vendorid = (deviceID & 0x0000FFFF);
	info->deviceid = (deviceID & 0xFFFF0000) >> 16;

	// Get the device class
	DWORD classinfo = GetConfigDWORD( bus, device, function, 0x08 );
	info->class_code = (classinfo & 0xFF000000) >> 24;
	info->subclass_code = (classinfo & 0x00FF0000) >> 16;
	info->interface_code = (classinfo & 0x0000FF00) >> 8;

	// Get the connected interrupt
	DWORD irqinfo = GetConfigDWORD( bus, device, function, 0x3C );
	info->irq = irqinfo & 0x000000FF;

	// Check the Base Address Registers for required resources.
	// TODO: Do we need to save the PCI COMMAND register and use its
	//       memory access bit? (USB Specific?)
	DWORD addr = 0;
	DWORD bar = 0;
	DWORD barsize = 0;
	DWORD memsize = 0;
	DWORD i = 0;

	for (i=0; i<5; i++) {
		// Calculate the config address of the current BAR
		addr = 0x10 + (i*4);

		// Get the current value
		bar = GetConfigDWORD( bus, device, function, addr );

		// Ask the BAR how much space is required
		SetConfigDWORD( bus, device, function, addr, 0xFFFFFFFF );
		barsize = GetConfigDWORD( bus, device, function, addr );

		// Put the current value back
		SetConfigDWORD( bus, device, function, addr, bar );

		if (barsize == 0) {
			// Not implemented
			bar = 0;
			memsize = 0;
		} else {
			// Calculate the size of the memory/port area
			if (bar & 0x01) {
				// I/O Space
				barsize = barsize & 0xFFFFFFFC;
			} else {
				barsize = barsize & 0xFFFFFFF0;
			}
			memsize = (~barsize) + 1;

			if (bar & 0x01) {
				// I/O Space
				info->ports[i] = bar & 0xFFFC;   // Mask off bottom bits
				info->ports_size[i] = memsize;
			} else {
				// Memory Space
				info->memory[i] = bar & 0xFFFFFFF0;   // Mask off bottom bits
				info->memory_size[i] = memsize;
				info->memory_prefetch[i] = (bar & 0x8) >> 3;
			}
		}
	}

	return info;
}


static void WriteMemorySize( DWORD x ) {
	if (x < 1024) {
		kprintf("%dB", x);
	} else {
		if (x < 1024*1024) {
			kprintf("%dKB", x>>10);
		} else {
			kprintf("%dMB", x>>20);
		}
	}	
}


static void WritePCIDeviceInfo( PCIDeviceInfo *info ) {
	// Writes the details for a PCI device to the console

	kprintf("[PCI] b=%b D=%b F=%b ", info->bus, info->device, info->function );
	kprintf("Vendor=%x ", info->vendorid + (info->deviceid << 16) );
	kprintf("Class=%x ", (info->class_code << 24) 
			+ (info->subclass_code << 16) + (info->interface_code << 8) );
	kprintf("Int=%b ", info->irq );

	switch (info->class_code) {
		case 0x00:
			kprintf( " Pre PCI rev 2" );
			break;
		case 0x01:
			switch (info->subclass_code)
			{
			case 0x01:
				kprintf( " IDE Controller" );
				break;
			case 0x05:
				kprintf( " ATA Controller" );
				break;
			case 0x06:
				kprintf( " SATA Controller" );
				break;
			default:
				kprintf( " Mass Storage" );
				break;
			}
			break;
		case 0x02:
			kprintf( " Network" );
			break;
		case 0x03:
			kprintf( " Video" );
			break;
		case 0x04:
			kprintf( " Multimedia" );
			break;
		case 0x06:
			switch (info->subclass_code)
			{
			case 0x00:
				kprintf( " Host-PCI Bridge" );
				break;
			case 0x01:
				kprintf( " ISA Bridge" );
				break;
			case 0x02:
				kprintf( " EISA Bridge" );
				break;
			case 0x03:
				kprintf( " MCA Bridge" );
				break;
			case 0x04:
				kprintf( " PCI-PCI Bridge" );
				break;
			case 0x80:
				kprintf( " Other Bridge" );
				break;
			default:
				kprintf( " Bridge" );
				break;
			}
			break;
		case 0x07:
			kprintf( " Simple Comms" );
			break;
		case 0x08:
			kprintf( " Base System" );
			break;
		case 0x09:
			kprintf( " Input" );
			break;
		case 0x0A:
			kprintf( " Docking Station" );
			break;
		case 0x0B:
			kprintf( " Processor" );
			break;
		case 0x0C:
			switch (info->subclass_code)
			{
			case 0x03:
				switch (info->interface_code)
				{
				case 0x00:
					kprintf( " USB Hub (UHCI)" );
					break;
				case 0x01:
					kprintf( " USB Hub (OHCI)" );
					break;
				case 0x02:
					kprintf( " USB Hub (EHCI)" );
					break;
				default:
					kprintf( " USB Hub" );
					break;
				}
				break;
			default:
				kprintf( " Serial Bus" );
				break;
			}
			break;
		case 0x0D:
			kprintf( " Wireless" );
			break;
		case 0x0E:
			kprintf( " Intelligent IO" );
			break;
		case 0x0F:
			kprintf( " Satellite Comms" );
			break;
		default:
			kprintf( " Unknown" );
	}

	DWORD i = 0;
	kprintf( "\n[PCI]   " );

	for (i=0; i<5; i++) {
		if (info->memory_size[i]) {
			kprintf("M:%x ", info->memory[i] );
			WriteMemorySize( info->memory_size[i] );
			kprintf(" ");
			if (info->memory_prefetch[i] == 1) {
				kprintf("PF ");
			}
		}
		if (info->ports_size[i]) {
			kprintf("P:%x,%d ", info->ports[i], i );
			WriteMemorySize( info->ports_size[i] );
			kprintf(" ");
		}
	}
	kprintf("\n");
}

