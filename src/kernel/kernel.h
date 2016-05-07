//
// Furos v2.0  -  Copyright Marlet Limited 2012
//
// File:			kernel.h
// Description:		Main kernel headers
// Author:			James Smith
// Created:			18-Aug-2012
//

#ifndef __KERNEL_H__
#define __KERNEL_H__

//
// TYPES
//
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef unsigned long long QWORD;

#define NULL 0

typedef int BOOL;
#define TRUE 1
#define FALSE 0

typedef DWORD HANDLE;
typedef DWORD SPINLOCK;
extern void SPINLOCK_WAIT( SPINLOCK* );
extern void SPINLOCK_SIGNAL( SPINLOCK* );

// IRQ Handler Function 
typedef void (*IRQFUNCPTR)(void);


//
// Variable Argument Defines
//
#define va_start(v,l) __builtin_va_start(v,l)
#define va_arg(v,l)   __builtin_va_arg(v,l)
#define va_end(v)     __builtin_va_end(v)
#define va_copy(d,s)  __builtin_va_copy(d,s)
typedef __builtin_va_list va_list;


//
// Kernel Types
//
typedef DWORD KRESULT;		// Kernel operation result

#define KRESULT_SUCCESS     0
#define KRESULT_NO_DEVICE   1
#define KRESULT_NO_MEMORY   2
#define KRESULT_PAGE_IN_USE 3
#define KRESULT_UNKNOWN     4
#define KRESULT_BAD_HANDLE  5


//
// Kernel Memory Types
//
#define MEMORYTYPE_KERNEL  1
#define MEMORYTYPE_DEVICE  2
#define MEMORYTYPE_USER    3
#define MEMORYTYPE_DMA     4
#define MEMORYTYPE_ROM     5


//
// Kernel Library Functions
//
void memcpy( void *src, void *dest, int bytes );
void memclr( void *dest, int bytes );


//
// Kernel Pointer Table (*** MUST MATCH startup.asm ***)
//
struct _KernelPointerTable
{
	DWORD KernelStack;
	WORD  Padding1;
	WORD  GDTLength;
	DWORD GDTAddr;
	WORD  Padding2;
	WORD  IDTLength;
	DWORD IDTAddr;
	DWORD PagingMasterTable;
	DWORD PagingFirstTable;
	DWORD KernelTSSAddr;
	WORD  KernelTSSDesc;
	WORD  Padding3;
	DWORD PhysicalPageBitmap;
	DWORD KernelHeapStart;
	DWORD MemoryMap;
};
typedef struct _KernelPointerTable KernelPointerTable;

// Import pointer to kernel pointer table
extern KernelPointerTable KPTR_PointerTableStart;

#define BYTES_PER_PAGE 4096


//
// Event
//
struct _Event {
	BOOL EventStatus;
};
typedef struct _Event Event;

//
// KERNEL OBJECT
//
struct _KERNELOBJECT {
	DWORD ObjectType;
	HANDLE ObjectHandle;
	DWORD ProcessID;
	struct _KERNELOBJECT *next;
};
typedef struct _KERNELOBJECT KERNELOBJECT;

//
// EVENT OBJECT
//
struct _EVENTOBJECT {
	KERNELOBJECT KObject;
	DWORD Value;
};
typedef struct _EVENTOBJECT EVENTOBJECT;


//
// Object Types
//
#define OBJECT_EVENT  1
#define OBJECT_DEVICE 2
#define OBJECT_FILE   3

//
// ProcessData
//
struct _ProcessData {
	DWORD ProcessID;
	BYTE *CurrentDirectory;
};
typedef struct _ProcessData ProcessData;


//
// ThreadExt
//
struct _ThreadExt {
	DWORD StackBase;
	DWORD StackSize;
	DWORD WakeUpTime;
	ProcessData *ParentProcess;
};
typedef struct _ThreadExt ThreadExt;


//
// ThreadTSS
//
struct _ThreadTSS {
	WORD  Link;		WORD  Empty1;
	DWORD ESP0;
	WORD  SS0;		WORD  Empty2;
	DWORD ESP1;
	WORD  SS1;		WORD  Empty3;
	DWORD ESP2;
	WORD  SS2;		WORD  Empty4;
	DWORD CR3;
	DWORD EIP;
	DWORD EFLAGS;
	DWORD EAX;
	DWORD ECX;
	DWORD EDX;
	DWORD EBX;
	DWORD ESP;
	DWORD EBP;
	DWORD ESI;
	DWORD EDI;
	WORD  ES;		WORD  Empty5;
	WORD  CS;		WORD  Empty6;
	WORD  SS;		WORD  Empty7;
	WORD  DS;		WORD  Empty8;
	WORD  FS;		WORD  Empty9;
	WORD  GS;		WORD  Empty10;
	WORD  LDT;		WORD  Empty11;
	WORD  DebugTrapBit;
	WORD  IOBaseOffset;			// Must be 0x0068+0x0030 for this struct.

	// OS Specific
	WORD ProcessStatus;
	WORD Priority;
	WORD TSSDescriptor;
	WORD Unused;
	struct _ThreadTSS *PrevTSS;
	struct _ThreadTSS *NextTSS;
	HANDLE WaitingEvent;
	DWORD OLDWakeUpTime;
	struct _ThreadExt *ThreadExtended;
};
typedef struct _ThreadTSS ThreadTSS;

//
// Thread Entry Function Pointer
//
typedef void (THREADENTRYFUNC)( void );


//
// define funcs
//
#define abs(x) (((x)<0)?(-(x)):(x))

#include "dev.h"


#endif /* __KERNEL_H__ */
