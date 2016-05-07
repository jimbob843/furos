//
// Furos v2.0  -  Copyright Marlet Limited 2012
//
// File:			memory.c
// Description:		Physical memory handling
// Author:			James Smith
// Created:			14-Sep-2012
//

#include "kernel.h"

//#define MEMORYREPORTS

// Total RAM available in KB
static DWORD gTotalRAM = 0;
static DWORD gAvailRAM = 0;

// Bitmap of physical pages in use
static BYTE *gPhysicalBitmap = NULL;

// Count of used memory
static DWORD gPagesUsed = 0;

// Spinlock to control access
static SPINLOCK gMemorySpinLock = 0;


//
// MemoryMapEntry
//
struct _MemoryMapEntry {
	QWORD BaseAddr;
	QWORD Length;
	DWORD AddressType;
	DWORD ExtendedAttributes;
};
typedef struct _MemoryMapEntry MemoryMapEntry;

// Memory types
#define ADDRESSTYPE_MEMORY 1
#define ADDRESSTYPE_RESERVED 2
#define ADDRESSTYPE_ACPIRECLAIM 3
#define ADDRESSTYPE_ACPINVS 4

// Page Table Entry bits
#define PAGETABLE_PRESENT      0x0001  // 0=Not-Present, 1=Present
#define PAGETABLE_WRITEABLE    0x0002  // 0=Read-Only, 1=Read/Write
#define PAGETABLE_SUPERVISOR   0x0004  // 0=User, 1=Supervisor
#define PAGETABLE_WRITETHROUGH 0x0008  // 0=Write-Back cache, 1=Write-Through cache
#define PAGETABLE_CACHEDISABLE 0x0010  // 0=Cacheable, 1=Not-Cacheable
#define PAGETABLE_ACCESSED     0x0020  // 0=Not-Accessed, 1=Accessed
#define PAGETABLE_DIRTY        0x0040  // 0=Not-Dirty, 1=Dirty
#define PAGETABLE_PAT          0x0080
#define PAGETABLE_GLOBAL       0x0100


// Address of master paging directory
//static DWORD *gMasterPagingDirectory = (DWORD*)0x00121000;
// Address of virtual paging directory (every page table)
static DWORD *gVirtualPagingDirectory = (DWORD*)0xFFC00000;

// Function declarations
static KRESULT mem_MapPhysicalPage( DWORD virtual_addr, DWORD physical_addr, DWORD memory_type );


//
// memint_PhysicalBitmapSet
//
static void mem_PhysicalBitmapSet( DWORD addr )
{
	// Each bit corresponds to one 4KB page. (>>12)
	//  and then (>>3) to find the correct byte in the bitmap
	DWORD bit = addr >> 12;
	gPhysicalBitmap[bit >> 3] |= (1 << (bit % 8));
}


//
// mem_PhysicalBitmapClear
//
static void mem_PhysicalBitmapClear( DWORD addr )
{
	// Each bit corresponds to one 4KB page. (>>12)
	//  and then (>>3) to find the correct byte in the bitmap
	DWORD bit = addr >> 12;
	gPhysicalBitmap[bit >> 3] &= ~(1 << (bit % 8));
}


//
// mem_PhysicalBitmapUpdate
//
static void mem_PhysicalBitmapUpdate( DWORD addr, BYTE value )
{
	if (value == 1)
	{
		// Set the bit
		mem_PhysicalBitmapSet( addr );
	}
	else
	{
		// Clear the bit
		mem_PhysicalBitmapClear( addr );
	}
}


//
// mem_PhysicalFindNextFreePage
//
static DWORD mem_PhysicalFindNextFreePage()
{
	// Searches the physical bitmap for a free page.
	DWORD newpage = -1;
	DWORD pages = gTotalRAM >> 2;			// Convert to 4KB pages
	DWORD bitmapsize = pages >> 3;			// 8 pages per bitmap byte
	BYTE *bitmap = gPhysicalBitmap;
	int i = 0;

	for (i = 0; (i < bitmapsize) && (newpage == -1); i++)
	{
		if (bitmap[i] == 0xFF)
		{
			// No pages available here
		}
		else
		{
			// There's a page here
			int j = 0;
			BYTE x = ~bitmap[i];  // Bit=1 gives free page
			BYTE mask = 1;
			for (j=0; (j<8) && (newpage == -1); j++)
			{
				if (x & mask)
				{
					// Found page
					newpage = (i << 15) + (j << 12);
				}
				else
				{
					// Not found
					mask <<= 1;
				}
			}
			// If newpage is still NULL then there's a problem, but 
			//  just go to the next bitmap byte.
		}
	}

	return newpage;
}


//
// mem_AllocPhysicalPage
//
static DWORD mem_AllocPhysicalPage( DWORD memory_type )
{
	// Allocates a virtual page for use
	DWORD newpage = mem_PhysicalFindNextFreePage();
	if (newpage != -1)
	{
		// Set the corresponding bit in the bitmap
		mem_PhysicalBitmapSet( newpage );
		gPagesUsed++;

		// Clear before use. Important for security between processes.
		// TODO: Create a temporary mapping, so the kernel has access?
		//       with a 1:1 mapping.
		//	MEMCLR( newpage, 4096 );
	}

	// Returns physical address of new 4k page
	return newpage;
}


//
// mem_InvalidatePage
//
void mem_InvalidatePage( DWORD physical_addr )
{
	INVALIDATE_PAGE( physical_addr );
}


//
// mem_GetMemoryTypeFlags
//  - returns the default set of bits required for the memory type
//
static DWORD mem_GetMemoryTypeFlags( DWORD memory_type )
{
	DWORD flags = 0;

	switch (memory_type)
	{
		case MEMORYTYPE_KERNEL:
			flags = PAGETABLE_PRESENT | PAGETABLE_WRITEABLE | PAGETABLE_SUPERVISOR;
			break;
		case MEMORYTYPE_DEVICE:
			flags = PAGETABLE_PRESENT | PAGETABLE_WRITEABLE | PAGETABLE_CACHEDISABLE;
			break;
		case MEMORYTYPE_USER:
			flags = PAGETABLE_PRESENT | PAGETABLE_WRITEABLE;
			break;
		case MEMORYTYPE_DMA:
			flags = PAGETABLE_PRESENT | PAGETABLE_WRITEABLE | PAGETABLE_CACHEDISABLE;
			break;
		case MEMORYTYPE_ROM:
			flags = PAGETABLE_PRESENT;
	}

	return flags;
}


//
// mem_GetPageTableEntry
//
static DWORD *mem_GetPageTableEntry( DWORD virtual_addr )
{
	// Looks for the page table entry for a particular virtual address.
	// Creates a new page table and adds it to the master directory
	// if necessary.
	// Returns a pointer to the correct directory entry.

	// 4MB (22 bits) per directory entry
//	DWORD *pagedirentry = gMasterPagingDirectory + (virtual_addr >> 22);
	DWORD *pagedirentry = (DWORD*)0xFFFFF000 + (virtual_addr >> 22);
	// 4KB (12 bits) per page.
	DWORD *pagetableentry = gVirtualPagingDirectory + (virtual_addr >> 12);

	// Check the page table is present
	if (!(*pagedirentry & PAGETABLE_PRESENT))
	{
		// Is not present, need to create the page table

		DWORD newpage_phys = mem_AllocPhysicalPage( MEMORYTYPE_USER );
		DWORD newpage_virt = ((DWORD)pagetableentry & 0xFFFFF000);

#ifdef MEMORYREPORTS
	kprintf("[MEM] PageDirEntry at %x = %x\n", pagedirentry, *pagedirentry );
	kprintf("[MEM] Creating new page table at physical %x\n", newpage_phys );
#endif
		if (newpage_phys == -1)
		{
			// No memory left!
			return NULL;
		}

		// Add the new page to the directory, and mark as present.
		*pagedirentry = newpage_phys | mem_GetMemoryTypeFlags( MEMORYTYPE_USER );

		// Don't do this when populating the page table for the top page
		if (newpage_virt != 0xFFFFF000) {
			// Map the new page table into the full page list at the top
			//  of virtual memory.
			mem_MapPhysicalPage( newpage_virt, newpage_phys, MEMORYTYPE_USER );

			// Clear the new page table
			memclr( (void*)newpage_virt, BYTES_PER_PAGE );
		}
	}

	return pagetableentry;
}



//
// mem_MapPhysicalPage
//
static KRESULT mem_MapPhysicalPage( DWORD virtual_addr, DWORD physical_addr, DWORD memory_type )
{
	KRESULT result = KRESULT_UNKNOWN;

	// Make sure the inputs are aligned to the 4KB boundaries
	virtual_addr &= 0xFFFFF000;
	physical_addr &= 0xFFFFF000;

#ifdef MEMORYREPORTS
	kprintf("[MEM] Mapping virtual %x to physical %x\n", virtual_addr, physical_addr );
#endif

	// Get the address of the Page Directory Entry for that group of pages
	DWORD *pagetableentry = mem_GetPageTableEntry( virtual_addr );

	if (pagetableentry == NULL)
	{
		result = KRESULT_NO_MEMORY;
	}
	else
	{
		// Check if the page is already present
		if (!(*pagetableentry & PAGETABLE_PRESENT))
		{
			// The page isn't mapped yet. Create a new entry.
			*pagetableentry = physical_addr | mem_GetMemoryTypeFlags( memory_type );

			// Make sure the cache is cleared properly????
			mem_InvalidatePage( virtual_addr );

			result = KRESULT_SUCCESS;
		}
		else
		{
			// The page is already present!
			kprintf("[MEM] %x already in use!\n", virtual_addr);
			result =  KRESULT_PAGE_IN_USE;
		}
	}

	return result;
}


//
// mem_AllocPage
//  - Allocates and new physical page and maps it to the supplied
//     virtual address. Returns a kernel result.
//
KRESULT mem_AllocPage( DWORD virtual_addr, DWORD memory_type )
{
	KRESULT result = KRESULT_UNKNOWN;

	// Prevent race conditions
	SPINLOCK_WAIT( &gMemorySpinLock );

	// Get the new physical page
	DWORD physical_addr = mem_AllocPhysicalPage( memory_type );
	if (physical_addr == -1)
	{
		// Oh dear. No more pages. Return failure.
		result = KRESULT_NO_MEMORY;
	}
	else
	{
		// Map the new page to the requested address
		result = mem_MapPhysicalPage( virtual_addr, physical_addr, memory_type );
	}

	// Release the spinlock
	SPINLOCK_SIGNAL( &gMemorySpinLock );

	return result;
}


//
// mem_ParseMemoryMap
//
static void mem_ParseMemoryMap( void )
{
	// NB: Currently only supports 32bit up to 4GB
	gTotalRAM = 0;

	// Get location of 128KB bitmap
	DWORD bitmapsize = 128 * 1024;
	KernelPointerTable *KPTR = &KPTR_PointerTableStart;
	gPhysicalBitmap = (BYTE*)KPTR->PhysicalPageBitmap;

	// Set all bits to reserved
	DWORD i = 0;
	DWORD *pBitmap = (DWORD*)gPhysicalBitmap;
	for (i=0; i<(bitmapsize>>2); i++)
	{
		*pBitmap = 0xFFFFFFFF;
		pBitmap++;
	}

	// Get the first entry from the fixed address the map was written
	//  to in the bootloader.
	BYTE *MemoryMapStart = (BYTE*)KPTR->MemoryMap;
	WORD *entrycount = (WORD*)MemoryMapStart;
	MemoryMapEntry *entry = (MemoryMapEntry *)(MemoryMapStart + 0x0004);

	for (i=0; i<*entrycount; i++)
	{
#ifdef MEMORYREPORTS
		kprintf("[MEM] %x:%x %x:%x %x\n", 
					(DWORD)(entry->BaseAddr >> 32),
					(DWORD)(entry->BaseAddr),
					(DWORD)(entry->Length >> 32),
					(DWORD)(entry->Length),
					entry->AddressType );
#endif

		if (entry->AddressType == ADDRESSTYPE_MEMORY)
		{
			// Usable memory, set to available
			QWORD addr = 0;
			QWORD startaddr = entry->BaseAddr;
			if (startaddr > 0xFFFFFFFF)
			{
				// Above 4GB, so ignore
			} 
			else
			{
				QWORD endaddr = startaddr + entry->Length;
				if (endaddr > 0xFFFFFFFF)
				{
					// Limit to 4GB
					endaddr = 0xFFFFFFFF;
				}
				for (addr=startaddr; addr<endaddr; addr+=BYTES_PER_PAGE)
				{
					mem_PhysicalBitmapClear( addr );
				}
				gAvailRAM += entry->Length >> 10;
			}
		}

		gTotalRAM += entry->Length >> 10;
		entry++;
	}

	// Add 512 to round up to the nearest MB
	kprintf( "[DEV] Memory: %dMB\n", (gTotalRAM+512)/1024 );

	// Mark the pages that are already allocated for the kernel as in use.
	DWORD kerneladdr = 0;
	for (kerneladdr = 0; kerneladdr < KPTR->KernelHeapStart; kerneladdr += BYTES_PER_PAGE)
	{
		mem_PhysicalBitmapSet( kerneladdr );
		gPagesUsed++;
	}

}


//
// mem_InitDevice
//
void mem_InitDevice() {

	//

	// Examine the memory map queried by the bootloader
	mem_ParseMemoryMap();

}
