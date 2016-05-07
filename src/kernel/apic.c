//
// Furos v2.0  -  Copyright Marlet Limited 2012
//
// File:			apic.c
// Description:		APIC Device
// Author:			James Smith
// Created:			20-Sep-2012
//

#include "kernel.h"

static DWORD volatile *APICBase = NULL;

#define IA32_APIC_BASE    0x1B   // MSR Address
#define IA32_APIC_ENABLE 0x800   // Enable Flag

#define APIC_APICID	     0x020
#define APIC_APICVER	 0x030
#define APIC_TASKPRIOR	 0x080
#define APIC_EOI         0x0B0
#define APIC_LDR	     0x0D0
#define APIC_DFR	     0x0E0
#define APIC_SPURIOUS	 0x0F0
#define APIC_ESR	     0x280
#define APIC_ICRL	     0x300
#define APIC_ICRH	     0x310
#define APIC_LVT_TMR	 0x320
#define APIC_LVT_PERF	 0x340
#define APIC_LVT_LINT0	 0x350
#define APIC_LVT_LINT1	 0x360
#define APIC_LVT_ERR	 0x370
#define APIC_TMRINITCNT	 0x380
#define APIC_TMRCURRCNT	 0x390
#define APIC_TMRDIV	     0x3E0
#define APIC_LAST	     0x38F
#define APIC_DISABLE	 0x10000
#define APIC_SW_ENABLE	 0x100
#define APIC_CPUFOCUS	 0x200
#define APIC_NMI	 (4<<8)
#define TMR_PERIODIC	 0x20000
#define TMR_BASEDIV	(1<<20)


//isr_dummytmr:	mov			dword [apic+APIC_EOI], 0
//		iret
//isr_spurious:	iret
//		;function to set a specific interrupt gate in IDT
//		;al=interrupt
//		;ebx=isr entry point
//writegate:	...
//		ret
//


void apic_SpuriousIRQHandler( void )
{
	// IRQ7
}


void apic_TimerIRQHandler( void )
{
	// IRQ0

	// Acknowledge the interrupt
	APICBase[APIC_EOI] = 0;
}


KRESULT apic_InitDevice( void )
{
	// TODO: First check that the APIC exists
	//  (Don't we need to do this for each processor?

	// Read the APIC address
	DWORD eax = 0;
	DWORD edx = 0;
	READ_MSR( IA32_APIC_BASE, &edx, &eax );
	APICBase = (DWORD volatile *)eax;

	// TODO: Map memory to allow access to APIC memory IO range

	// Write the APIC address back with the enable flag set
	WRITE_MSR( IA32_APIC_BASE, eax | IA32_APIC_ENABLE );

    // Set the Spurious Interrupt Vector Register bit 8 to start receiving interrupts
	APICBase[APIC_SPURIOUS] = APICBase[APIC_SPURIOUS] | APIC_SW_ENABLE;
	// NB: APIC_SPURIOUS holds IRQ of spurious interrupt handler

	return KRESULT_SUCCESS;
}


DWORD apic_ReadIoApic( void *ioapicaddr, DWORD reg )
{
   DWORD volatile *ioapic = (DWORD volatile *)ioapicaddr;
   ioapic[0] = (reg & 0xff);
   return ioapic[4];
}
 
void apic_WriteIoApic( void *ioapicaddr, DWORD reg, DWORD value )
{
   DWORD volatile *ioapic = (DWORD volatile *)ioapicaddr;
   ioapic[0] = (reg & 0xff);
   ioapic[4] = value;
}
