//
// Furos v2.0  -  Copyright Marlet Limited 2012
//
// File:			exp.c
// Description:		Exception handling
// Author:			James Smith
// Created:			18-Aug-2012
//

#include "kernel.h"


//
// Dump GDT
//
void exp_DumpGDT( void )
{
	KernelPointerTable *KPTR = &KPTR_PointerTableStart;
	DWORD GDT_Addr = KPTR->GDTAddr;
	WORD GDT_Length = KPTR->GDTLength;
	WORD *GDT_End = (WORD*)(GDT_Addr + GDT_Length);
	WORD *GDT_Ptr = (WORD*)GDT_Addr;
	int i = 0;
	WORD Desc = 0;

	kprintf("\n==== GDT DUMP ====\n");

	while (GDT_Ptr < GDT_End)
	{
		kprintf("GDT\t%d %w %w %w %w %w\n",
				i++,
				Desc,
				*GDT_Ptr++,
				*GDT_Ptr++,
				*GDT_Ptr++,
				*GDT_Ptr++ );

		Desc += 8;
	}
}

//
// Dump Kernel Pointer Table
//
void exp_DumpKernelTable( void )
{
	int i = 0;

	DWORD *KPTR = (DWORD*)&KPTR_PointerTableStart;
	kprintf("\n==== KERNEL TABLE ====\n");

	for (i=0; i<3; i++)
	{
		kprintf("%x %x %x %x %x %x\n",
			KPTR[0 + i*6],
			KPTR[1 + i*6],
			KPTR[2 + i*6],
			KPTR[3 + i*6],
			KPTR[4 + i*6],
			KPTR[5 + i*6] );
	}
}


void exp_GPFHandler( DWORD errorcode, DWORD addr, DWORD cs, DWORD eflags )
{
	kprintf("\nGENERAL FAULT ADDR=0x%x CS=0x%x ERROR=0x%x FLAGS=0x%x\n",
					addr, cs, errorcode, eflags );
	STOP_PROCESSOR();
}

void exp_PageFaultHandler( DWORD faultaddr, DWORD eax, DWORD errorcode, DWORD ip )
{
//	kprintf("\nPAGE FAULT ADDR=0x%x EAX=0x%x ERROR=0x%x EIP=0x%x\n",
//					faultaddr, eax, errorcode, ip );
	STOP_PROCESSOR();
}

void exp_DefaultHandler( DWORD errorcode, DWORD addr, DWORD cs, DWORD eflags )
{
	kprintf("\nDEFAULT FAULT ERROR=0x%x EIP=0x%x CS=0x%w FLAGS=0x%x\n",
					errorcode, addr, cs, eflags );
	kprintf("\n");
	sch_DumpThreadList();
//	exp_DumpGDT();
	exp_DumpKernelTable();
	STOP_PROCESSOR();
}


void exp_InvalidOpCodeHandler( DWORD errorcode, DWORD addr, DWORD cs, DWORD eflags )
{
	kprintf("\nINVALID OPCODE ERROR=0x%x EIP=0x%x CS=0x%x FLAGS=0x%x\n",
					errorcode, addr, cs, eflags );

	kprintf("\n");
	sch_DumpThreadList();
	exp_DumpGDT();
	exp_DumpKernelTable();

	STOP_PROCESSOR();
}

void exp_InvalidTaskHandler( DWORD errorcode, DWORD addr, DWORD cs, DWORD eflags )
{
	kprintf("\nINVALID TSS FAULT EIP=0x%x CS=0x%w TSS=0x%w FLAGS=0x%x\n",
					addr, cs, errorcode, eflags );

	kprintf("\n");
	sch_DumpThreadList();
	exp_DumpGDT();
	exp_DumpKernelTable();

	STOP_PROCESSOR();
}

