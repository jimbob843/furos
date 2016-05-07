//
// Furos v2.0  -  Copyright Marlet Limited 2012
//
// File:			sch.c
// Description:		Scheduler
// Author:			James Smith
// Created:			10-Oct-2012
//

#include "kernel.h"

#define TSSREPORTS

// Process statuses
#define PROCSTATUS_RUNNING   1
#define PROCSTATUS_READY	 2	
#define PROCSTATUS_WAITING	 3
#define PROCSTATUS_IDLE		 4  // Used to identify the idle thread
#define PROCSTATUS_CLOSING   5
#define PROCSTATUS_TIMEDWAIT 6


static ThreadTSS *ThreadListHead;	// Head of linked list of threads
static ThreadTSS *ThreadListTail;	// Last item in list of threads
static ThreadTSS *IdleThread;
static ThreadTSS *CurrentThread;
static SPINLOCK gProcessListSpinlock = 0;

//
// Dump Thread List
//
void sch_DumpThreadList( void )
{
	KernelPointerTable *KPTR = &KPTR_PointerTableStart;
	ThreadTSS *Current = (ThreadTSS*)(KPTR->KernelTSSAddr);

	kprintf("==== THREAD LIST ====\n");
	kprintf("ADDR=%x TSS=%w STATUS=%d EIP=%x CR3=%x LINK=%x\n",
			(DWORD)Current,
			Current->TSSDescriptor,
			Current->ProcessStatus,
			Current->EIP,
			Current->CR3,
			Current->Link);

	Current = ThreadListHead;

	while (Current != NULL)
	{
		kprintf("ADDR=%x TSS=%w STATUS=%d EIP=%x CR3=%x LINK=%x\n",
			(DWORD)Current,
			Current->TSSDescriptor,
			Current->ProcessStatus,
			Current->EIP,
			Current->CR3,
			Current->Link);

		Current = Current->NextTSS;
	}
}


//
// Idle Process
//
void IdleProcess_EntryPoint( void )
{
	// The idle process. Just halt, and wait for interrupts
	IDLE_LOOP();
}


//
// sch_AddTaskToProcessList
//
void sch_AddTaskToProcessList( ThreadTSS *tss )
{
	SPINLOCK_WAIT( &gProcessListSpinlock );

	// Finish setting up the TSS
	tss->ProcessStatus = PROCSTATUS_READY;
	tss->PrevTSS = NULL;
	tss->NextTSS = NULL;

	// Turn off interrupts while we're changing the task list
	DISABLE_INTERRUPTS();

	// Install the new thread in the GDT
	tss->TSSDescriptor = (WORD) ADD_TASK( tss );

	// Add the thread to the thread list
	tss->PrevTSS = ThreadListTail;
	if (ThreadListTail != NULL) {
		ThreadListTail->NextTSS = tss;
	}
	ThreadListTail = tss;
	if (ThreadListHead == NULL) {
		// This is the first thread
		ThreadListHead = tss;
	}

#ifdef TSSREPORTS
	kprintf("[SCH] TSS Created. ADDR=%x TSS=%x ESP=%x\n",
				(DWORD)tss, (DWORD)tss->TSSDescriptor, (DWORD)tss->ESP );
#endif

	// Turn interrupts back on
	ENABLE_INTERRUPTS();

	SPINLOCK_SIGNAL( &gProcessListSpinlock );

}


//
// sch_CreateKernelThread
//
ThreadTSS *sch_CreateKernelThread( THREADENTRYFUNC EntryPoint )
{
	// Creates a new thread of execution in the main kernel process

	// Allocate some memory for the new TSS
	// We need to make sure that the TSS does not overlap a page boundary.
	ThreadTSS *tss = (ThreadTSS *)kmalloc_aligned(sizeof(ThreadTSS), 0x100);

	// Get the address of the kernel page directory
	KernelPointerTable *KPTR = &KPTR_PointerTableStart;
	DWORD PagingMasterDir = KPTR->PagingMasterTable;

	// Populate with values. Using kernel segments for code and data.
	tss->ES = 0x10;
	tss->CS = 0x08;
	tss->SS = 0x10;
	tss->DS = 0x10;
	tss->FS = 0x10;
	tss->GS = 0x10;
	tss->IOBaseOffset = 0x0098;		// Set the length of the TSS
	tss->EIP = (DWORD)EntryPoint;	// Thread entry point
	tss->EFLAGS = 0x00000200;		// Have IF=1 to enable interrupts
	tss->CR3 = PagingMasterDir;		// Set the kernel page master directory
	tss->LDT = 0;

	ThreadExt *ext = (ThreadExt*)kmalloc(sizeof(ThreadExt));
	tss->ThreadExtended = ext;

	// Allocate some stack space (DWORD aligned for performance)
	ext->StackSize = 4096;
	ext->StackBase = (DWORD)kmalloc_aligned(ext->StackSize, 4);
	if (ext->StackBase == 0) {
		// Out of memory
		SPINLOCK_SIGNAL( &gProcessListSpinlock );
		return NULL;
	}
	tss->ESP = ext->StackBase + ext->StackSize;		// Put ESP at the top

	// TODO: Put a return address onto the stack, so when the
	//  kernel entry function returns it'll closedown gracefully

	// Setup the thread's control data
	tss->Priority = 128;
	tss->WaitingEvent = 0;
	ext->WakeUpTime = 0;
//	ext->ParentProcess = sch_GetCurrentProcessData();
	ext->ParentProcess = NULL;

	// Add to the thread list
	sch_AddTaskToProcessList( tss );

	return tss;
}


//
// sch_InitScheduler
//
KRESULT sch_InitScheduler( void ) {
		
	// Initialises the process scheduler and sets it running.

	// Setup the process list
	ThreadListHead = NULL;
	ThreadListTail = NULL;

	// Create the idle process
	IdleThread = sch_CreateKernelThread( IdleProcess_EntryPoint );
	IdleThread->ProcessStatus = PROCSTATUS_READY;
	CurrentThread = ThreadListHead;

	// Set things running
	kprintf("[SCH] START TSS %w\n", CurrentThread->TSSDescriptor );
	CALL_TSS(CurrentThread->TSSDescriptor);

	return KRESULT_SUCCESS;
}


//
// sch_GetNextReadyThread
//
static ThreadTSS *sch_GetNextReadyThread( ThreadTSS *start )
{
	// Returns the next ready process. Assumes that at least one
	//  process will always be ready - the idle process.

	ThreadTSS *current = start;
	ThreadTSS *nextready = NULL;
	DWORD currenttick = rtc_GetGlobalTickCount();

	do {
		current = current->NextTSS;
		if (current == NULL) {
			current = ThreadListHead;		// Back to the head of the list
		}
		
		// Check for timed waiting process that could be ready now
		if (current->ProcessStatus == PROCSTATUS_TIMEDWAIT) {
			if (current->ThreadExtended->WakeUpTime < currenttick) {
				// Time to wake up
				current->ThreadExtended->WakeUpTime = 0;
				current->ProcessStatus = PROCSTATUS_READY;
			}
		}

		// Is this one ready?
		if (current->ProcessStatus == PROCSTATUS_READY) {
			nextready = current;
		}

	} while ((current != start) && (nextready == NULL));

	return nextready;
}


//
// sch_ScheduleInterrupt
//
void sch_ScheduleInterrupt( void )
{
	// The timer has told us to wake up.
	// Look at the process list, and switch tasks if necessary

	SPINLOCK_WAIT( &gProcessListSpinlock );

	// Put the current process back to READY.
	if (CurrentThread == NULL) {
		// No current process active
	} else {
		if (CurrentThread->ProcessStatus == PROCSTATUS_RUNNING) {
			// Stop us running to allow other tasks to run
			CurrentThread->ProcessStatus = PROCSTATUS_READY;
		}
	}

	ThreadTSS *next = sch_GetNextReadyThread( CurrentThread );
	if (next == NULL) {
		// Couldn't find another ready thread
		// Should never happen as the idle thread should always be ready
		if (CurrentThread != NULL) {
			CLEAR_BUSY_BIT(CurrentThread->TSSDescriptor);
		}
		CurrentThread = IdleThread;
//		next->ProcessStatus = PROCSTATUS_RUNNING;
		SET_BUSY_BIT(CurrentThread->TSSDescriptor);
		SET_SCHEDULER_BACKLINK(CurrentThread->TSSDescriptor);
	} else {
		if (next == CurrentThread) {
			// Still the same thread, no task switch necessary
		} else {
			// Switch to a new thread
			// Set the back link of the scheduler task to go
			//  "back" to the new task when we IRET
			// Also need to clear the busy bit of the current task
			if (CurrentThread != NULL) {
				CLEAR_BUSY_BIT(CurrentThread->TSSDescriptor);
			}
			CurrentThread = next;
			next->ProcessStatus = PROCSTATUS_RUNNING;
			SET_BUSY_BIT(next->TSSDescriptor);
			SET_SCHEDULER_BACKLINK(next->TSSDescriptor);
		}
	}

	SPINLOCK_SIGNAL( &gProcessListSpinlock );
}

//
// sch_AdvanceIndicator
//
static void sch_AdvanceIndicator( void )
{
	static BYTE c = '#';

	BYTE *screen = (BYTE*)0xB809E;
	*screen++ = c++;
	*screen++ = 0x07;
}


//
// sch_MainLoop
//
void sch_MainLoop( void )
{
	while (1)
	{
		sch_AdvanceIndicator();

//		flp_TimerInterrupt();
//		ohci_TimerInterrupt();
//		usb_TimerInterrupt();

		sch_ScheduleInterrupt();

		// Perform end of interrupt and iret
		END_OF_INT();
	}
}
