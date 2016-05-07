/*
 * Furos v2.0   Copyright Marlet Limited 2012
 *
 * File:			cpu.c
 * Description:		General Device Handling Routines
 * Author:			James Smith
 * Created:			20-Sep-2012
 *
 */
 
#include "kernel.h"

// CPUID commands
#define CPUID_GET_VENDOR_STRING 0x0
#define CPUID_GET_FEATURES      0x1
#define CPUID_GET_TLB_INFO      0x2      
#define CPUID_GET_SERIAL_NUMBER 0x3
#define CPUID_GET_CACHE_PARAMS  0x4
#define CPUID_GET_MONITOR_INFO  0x5
#define CPUID_EXT_MAX           0x80000000
#define CPUID_EXT_GET_FEATURES  0x80000001

// MSR registers
#define IA32_APIC_BASE    0x01B
#define IA32_MTRRCAP      0x0FE
#define IA32_SYSENTER_CS  0x174
#define IA32_SYSENTER_ESP 0x175
#define IA32_SYSENTER_EIP 0x176

// MSR flags
#define IA32_APIC_BASE_MSR_ENABLE 0x800


#define LAPIC_DEFAULT_BASE 0xFEE00000
#define IOAPIC_DEFAULT_BASE 0xFEC00000

#define LAPIC_ID_REGISTER  0x0020
#define LAPIC_VER_REGISTER 0x0030
#define LAPIC_TASK_PRI_REG 0x0080
#define LAPIC_ARB_PRI_REG  0x0090
#define LAPIC_PROC_PRI_REG 0x00A0
#define LAPIC_ISR_0        0x0100
#define LAPIC_ISR_1        0x0110
#define LAPIC_ISR_2        0x0120
#define LAPIC_ISR_3        0x0130
#define LAPIC_ISR_4        0x0140
#define LAPIC_ISR_5        0x0150
#define LAPIC_ISR_6        0x0160
#define LAPIC_ISR_7        0x0170
#define LAPIC_TMR_0        0x0180
#define LAPIC_IRR_0        0x0200
#define LAPIC_ERR_STATUS   0x0280

#define PORT_IMCR_ADDR 0x22
#define PORT_IMCR_DATA 0x23
#define IMCR_ADDR 0x70
#define IMCR_OFF 0x00
#define IMCR_ON  0x01


struct _MPConfigTable
{
	DWORD Signature;
	WORD  BaseTableLength;
	BYTE  SpecRev;
	BYTE  Checksum;
	BYTE  OEMID[8];
	BYTE  ProductID[12];
	DWORD OEMTablePtr;
	WORD  OEMTableSize;
	WORD  EntryCount;
	DWORD LAPICAddr;
	WORD  ExtTableLength;
	BYTE  ExtTableChecksum;
};
typedef struct _MPConfigTable MPConfigTable;


static DWORD *MP_Search( void )
{
	// Look through memory for the MP table used for Multi Processor support
	// Need to find "_MP_" either in first 1KB of EBDA (pointer to
	//   EBDA at 0x040E) or between 0x000F0000 to 0x000FFFFF

	DWORD *mp = NULL;

	// Get address of EBDA
	WORD *ebdasegaddr = (WORD*)0x040E;
	WORD ebdaseg = *ebdasegaddr;
	DWORD *ebda = (DWORD*)(ebdaseg << 4);   // Calculate start address
	DWORD *current = ebda;
	DWORD i = 0;
//	kprintf("[DEV] EBDA=0x%x\n", ebda);

	for (i = 0; i < 64; i++) {
		if (*current == 0x5F504D5F) {
			// Found first part
//			kprintf("[DEV] Found _MP_ at 0x%x\n", (DWORD)current);
			mp = current;
		}
		current += 2;
	}

	current = (DWORD*)0x000F0000;
	while (current < (DWORD*)0x00100000) {
		if (*current == 0x5F504D5F) {
			// Found first part
//			kprintf("[DEV] Found _MP_ at 0x%x\n", (DWORD)current);
			mp = current;
		}
		current += 2;
	}

	return mp;
}


void cpu_InitDevice( void )
{
	// Query the CPU for features and devices

	if (CPUID_AVAIL() == 1)
	{
		DWORD eax = 0;
		DWORD ebx = 0;
		DWORD ecx = 0;
		DWORD edx = 0;

		// CPUID is available
		EXEC_CPUID( CPUID_GET_FEATURES, &eax, &ebx, &ecx, &edx );

		DWORD family = (eax >> 8) & 0x7;
		DWORD model = (eax >> 4) & 0x7;
		kprintf( "[DEV] CPU Family=%d Model=%d", family, model );

		if (edx & (1<<0))  kprintf(" FPU");
		if (edx & (1<<5))  kprintf(" MSR");
		if (edx & (1<<6))  kprintf(" PAE");
		if (edx & (1<<9))  kprintf(" APIC");
		if (edx & (1<<11))  kprintf(" SEP");
		if (edx & (1<<12))  kprintf(" MTRR");
		if (edx & (1<<22))  kprintf(" ACPI");
		if (edx & (1<<23))  kprintf(" MMX");
		if (edx & (1<<25))  kprintf(" SSE");
		if (edx & (1<<28))  kprintf(" HTT");

		BYTE lapicID = (BYTE)(ebx >> 24);

		// Find the largest extended leaf that is supported
		EXEC_CPUID( CPUID_EXT_MAX, &eax, &ebx, &ecx, &edx );

		if (eax >= CPUID_EXT_GET_FEATURES)
		{
			EXEC_CPUID( CPUID_EXT_GET_FEATURES, &eax, &ebx, &ecx, &edx );
			if (edx & (1<<29))  kprintf(" IA64");
		}
		kprintf("\n");


		eax = 0;
		edx = 0;
		READ_MSR( IA32_APIC_BASE, &edx, &eax );

		kprintf("[DEV] APIC=0x%x ID=%d\n", (eax & 0xFFFFF000), lapicID );

		DWORD *mpptrstruct = MP_Search();
		DWORD *mpconfig = (DWORD*)mpptrstruct[1];
		kprintf("[DEV] MP=0x%x 0x%x\n", (DWORD)mpconfig, (DWORD)(*mpconfig) );

		MPConfigTable *mp = (MPConfigTable*)mpconfig;
		BYTE OEM[9];
		memcpy( mp->OEMID, OEM, 8 );
		OEM[8] = 0;
		BYTE Product[13];
		memcpy( mp->ProductID, Product, 12 );
		Product[12] = 0;
		kprintf("[DEV] OEM=%s Product=%s Entries=%d ExtEntries=%d\n",
				OEM, Product, mp->EntryCount, mp->ExtTableLength );

		DWORD EntriesLeft = mp->EntryCount;
		BYTE *config = (BYTE*)((DWORD)mp + sizeof(MPConfigTable));
		while (EntriesLeft > 0)
		{
			BYTE EntryType = *config;
			DWORD EntryLength = 0;
			BYTE *EntryName;
			switch (EntryType)
			{
				case 0: // PROCESSOR
					EntryLength = 20;
					EntryName = "CPU";
					DWORD *sig = (DWORD*)&config[4];
					BOOL IsEnabled = (config[3] & 0x01 ? TRUE : FALSE);
					BOOL IsBSP = (config[3] & 0x02 ? TRUE : FALSE);
					kprintf("[MP ] %s APIC=%d V=%d SIG=%x %s\n",
								EntryName, config[1],
								config[2], *sig,
								(IsBSP == TRUE ? "BSP" : "AP"));
					break;
				case 1:	// BUS
					EntryLength = 8;
					EntryName = "BUS";
					BYTE TypeStr[7];
					memcpy( &config[2], TypeStr, 6 );
					TypeStr[7] = 0;
//					kprintf("[MP ] %s BUS=%d TYPE=%s\n", EntryName, config[1],
//								TypeStr);
					break;
				case 2:	// IO APIC
					EntryLength = 8;
					EntryName = "I/O APIC";
					DWORD *addr = (DWORD*)&config[4];
					kprintf("[MP ] %s BUS=%d ADDR=%x\n", EntryName, config[4],
								*addr);
					break;
				case 3:	// IO INTERRUPT
					EntryLength = 8;
					EntryName = "I/O INT";
//					kprintf("[MP ] %s BUS=%d IRQ=%d TYPE=%d\n", EntryName, config[4],
//								config[5], config[1]);
					break;
				case 4:	// LOCAL INTERRUPT
					EntryLength = 8;
					EntryName = "LOCAL INT";
//					kprintf("[MP ] %s BUS=%d\n", EntryName, config[4] );
					break;
				default:
					EntryLength = 8;
					EntryName = "Unknown";
			}
			config = (BYTE*)((DWORD)config + EntryLength);
			EntriesLeft--;
		}

	}
}
