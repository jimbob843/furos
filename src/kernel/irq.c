//
// Furos v2.0  -  Copyright Marlet Limited 2012
//
// File:			irq.c
// Description:		Interrupt handling
// Author:			James Smith
// Created:			18-Aug-2012
//

#include "kernel.h"

//#define IRQREPORTS

// Set largest available IRQ number
#define MAX_IRQ 15

// Handler Entry
struct _IRQ_HANDLER {
	IRQFUNCPTR HandlerFunc;
	struct _IRQ_HANDLER *NextHandler;
};
typedef struct _IRQ_HANDLER IRQ_HANDLER;


// Separate handler list for each IRQ
static IRQ_HANDLER *HandlerList[MAX_IRQ+1];


//
// irq_InitDevice
//
void irq_InitDevice( void )
{
	// Initialise all of the handler lists
	DWORD i = 0;
	for (i=0; i<MAX_IRQ; i++)
	{
		HandlerList[i] = NULL;
	}

	// Allow interrupts to cascade through the PIC
	ENABLE_IRQ( 2 );
}


//
// irq_RegisterIRQHandler
//
void irq_RegisterIRQHandler( BYTE irq, IRQFUNCPTR func )
{
	if (irq > MAX_IRQ)
	{
		// irq too large
		return;
	}

	DISABLE_INTERRUPTS();

	// Create a new handler entry and add it to the appropriate list
	IRQ_HANDLER *newhandler = (IRQ_HANDLER*)kmalloc(sizeof(IRQ_HANDLER));
	newhandler->HandlerFunc = func;
	newhandler->NextHandler = HandlerList[irq];
	HandlerList[irq] = newhandler;

#ifdef IRQREPORTS
	kprintf("[IRQ] Registered IRQ%d with func %x\n", irq, (DWORD)func );
#endif

	ENABLE_INTERRUPTS();
}


//
// irq_UnregisterIRQHandler
//
void irq_UnregisterIRQHandler( BYTE irq, IRQFUNCPTR func )
{
	if (irq > MAX_IRQ)
	{
		// irq too large
		return;
	}

	DISABLE_INTERRUPTS();

	IRQ_HANDLER *current = HandlerList[irq];
	IRQ_HANDLER *previous = NULL;

	while (current != NULL)
	{
		if (current->HandlerFunc == func)
		{
			// Remove this one
			IRQ_HANDLER *nextcurrent = current->NextHandler;
			if (previous == NULL)
			{
				HandlerList[irq] = nextcurrent;
			}
			else
			{
				previous->NextHandler = nextcurrent;
			}

			// Free up the memory
			kfree(current);

			// Move to the next one, previous doesn't change
			current = nextcurrent;
		}
		else
		{
			// Shuffle along the list
			previous = current;
			current = current->NextHandler;
		}

	}

	ENABLE_INTERRUPTS();
}


//
// irq_GenericIRQHandler
//
void irq_GenericIRQHandler( BYTE irq )
{
	if (irq > MAX_IRQ)
	{
		return;
	}

	// Find the correct IRQ handler list
	IRQ_HANDLER *current = HandlerList[irq];
	if (current == NULL)
	{
		kprintf("[IRQ] NO HANDLER FOR IRQ%d\n", irq );
		return;
	}

	// Execute all of the handlers registered for that IRQ
	while (current != NULL)
	{
#ifdef IRQREPORTS
		if ((irq != 8) && (irq != 6)) {
			kprintf("[IRQ] IRQ%d Calling Func %x\n", irq, (DWORD)current->HandlerFunc );
		}
#endif
		current->HandlerFunc();
		current = current->NextHandler;
	}
}
