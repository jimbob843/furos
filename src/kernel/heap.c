//
// Furos v2.0  -  Copyright Marlet Limited 2012
//
// File:			heap.c
// Description:		Kernel Heap
// Author:			James Smith
// Created:			15-Sep-2012
//

#include "kernel.h"

//#define MEMORYREPORTS

struct _HEAPBLOCK {
	DWORD magic;
	DWORD size;
};
typedef struct _HEAPBLOCK HEAPBLOCK;

#define MAGIC_FREE 0xAABBCCDD
#define MAGIC_USED 0x11223344


static DWORD gKernelHeapStart = 0x00149000;
static DWORD gKernelHeapEnd = 0x00149000;  /* First invalid byte */
static DWORD gKernelLastPageEnd = 0x00149000;
static SPINLOCK gHeapSpinLock = 0;

//
// heap_InitDevice
//
KRESULT heap_InitDevice( void )
{
	// Get the start of available memory for the heap
	KernelPointerTable *KPTR = &KPTR_PointerTableStart;

	// Create the heap with zero length
	gKernelHeapStart = KPTR->KernelHeapStart;
	gKernelHeapEnd = gKernelHeapStart;
	gKernelLastPageEnd = gKernelHeapStart;

	return KRESULT_SUCCESS;
}

//
// heap_ExtendHeapPages
//
KRESULT heap_ExtendHeapPages( DWORD numpages )
{
	int i = 0;

	for (i=0; i<numpages; i++)
	{
		KRESULT result = mem_AllocPage( gKernelLastPageEnd, MEMORYTYPE_KERNEL );
		if (result != KRESULT_SUCCESS)
		{
			return result;
		}

		gKernelLastPageEnd += BYTES_PER_PAGE;
	}

#ifdef MEMORYREPORTS
	kprintf("[MEM] HEAP EXTEND %d pages, LastPageEnd=%x\n",
				numpages, gKernelLastPageEnd );
#endif

	// Return success
	return KRESULT_SUCCESS;
}


DWORD heap_ExtendHeap( DWORD numbytes ) {

	DWORD result = 0;
	gKernelHeapEnd += numbytes;

	while ((gKernelHeapEnd > gKernelLastPageEnd) && (result == 0)) {
		result = heap_ExtendHeapPages( 1 );
	}

	return result;
}


void heap_Free( void *addr ) {

	SPINLOCK_WAIT( &gHeapSpinLock );

	// Locate the block
//	HEAPBLOCK *hb = (HEAPBLOCK*)(addr - sizeof(HEAPBLOCK));

	// Mark as free
//	hb->magic = MAGIC_FREE;

	SPINLOCK_SIGNAL( &gHeapSpinLock );
}


void *heap_Alloc( DWORD numbytes ) {

	SPINLOCK_WAIT( &gHeapSpinLock );

	DWORD found_addr = 0;

	// Calculate the number of bytes we need
	DWORD bytes = numbytes + sizeof(HEAPBLOCK);

	// Search for the first free block that is big enough
	DWORD current_addr = gKernelHeapStart;

	while ((current_addr < gKernelHeapEnd) && (found_addr == 0)) {
		// Look at the current block
		HEAPBLOCK *hb = (HEAPBLOCK*)current_addr;

		if ((hb->magic == MAGIC_FREE) && (hb->size >= bytes)) {
			// This one will do
			found_addr = current_addr + sizeof(HEAPBLOCK);
			hb->magic = MAGIC_USED;

			if (hb->size > (bytes + 40)) {
				// Need to split this block
				HEAPBLOCK *newblock = (HEAPBLOCK*)(current_addr + bytes);
				newblock->magic = MAGIC_FREE;
				newblock->size = hb->size - bytes;
				hb->size = bytes;
			}
		} else {
			// Move to the next one
			current_addr += hb->size;
		}
	}

	if (found_addr == 0) {
		// Next block will have to be at the end of the heap
		current_addr = gKernelHeapEnd;

		// We've got to the end of the heap. Need more space.
		heap_ExtendHeap( bytes );

		HEAPBLOCK *hb = (HEAPBLOCK*)current_addr;
		found_addr = current_addr + sizeof(HEAPBLOCK);
		hb->magic = MAGIC_USED;
		hb->size = bytes;


	}

	SPINLOCK_SIGNAL( &gHeapSpinLock );

	return (void *)found_addr;
}



void *kmalloc_simple( DWORD bytes, DWORD alignment ) {
 
      // Need to allocate a DWORD aligned amount....
      if ((bytes % 4) > 0) {
            // Not whole number of DWORDs, so increase slightly
            bytes += 4 - (bytes % 4);
      }

	  // Handle alignment
	  if (alignment > 0) {
		  DWORD newaddr = gKernelHeapEnd + sizeof(HEAPBLOCK);
		  if ((newaddr % alignment) != 0) {
			  // Need to adjust
			  int alignmentbytes = alignment 
				                   - (newaddr % alignment)
								   - sizeof(HEAPBLOCK);
			  while (alignmentbytes <= 0) {
				  alignmentbytes += alignment;
			  }
			  heap_Alloc( alignmentbytes );
		  }
	  }

	  return heap_Alloc( bytes );
}


//
// kmalloc
//
void *kmalloc( DWORD bytes ) {
	void *x = kmalloc_simple( bytes, 4 );
#ifdef MEMORYREPORTS
	kprintf("[MEM] kmalloc(%d) = %x\n", bytes, (DWORD)x );
#endif
	return x;
}


//
// kmalloc_aligned
//
void *kmalloc_aligned( DWORD bytes, DWORD alignment ) {
	void *x = kmalloc_simple( bytes, alignment );
#ifdef MEMORYREPORTS
	kprintf("[MEM] kmalloc_aligned(%d,%d) = %x\n", bytes, alignment, (DWORD)x );
#endif
	return x;
}

//
// kfree
//
void kfree( void *addr )
{
	heap_Free( addr );
}
