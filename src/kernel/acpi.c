/*
 * Furos v2.0   Copyright Marlet Limited 2012
 *
 * File:			acpi.c
 * Description:		Advanced Configuration and Power Interface
 * Author:			James Smith
 * Created:			20-Sep-2012
 *
 */
 
#include "kernel.h"

struct _RDSP_Descriptor {
	char Signature[8];
	BYTE Checksum;
	char OEMID[6];
	BYTE Revision;
	DWORD RsdtAddress;
};
typedef struct _RDSP_Descriptor RDSP_Descriptor;


struct _RDSP_Descriptor20 {
	char Signature[8];
	BYTE Checksum;
	char OEMID[6];
	BYTE Revision;
	DWORD RsdtAddress;
	DWORD Length;
	QWORD XsdtAddress;
	BYTE ExtendedChecksum;
	BYTE Reserved[3];
};
typedef struct _RDSP_Descriptor20 RDSP_Descriptor20;


struct _ACPI_SDTHeader {
  char Signature[4];
  DWORD Length;
  BYTE Revision;
  BYTE Checksum;
  char OEMID[6];
  char OEMTableID[8];
  DWORD OEMRevision;
  DWORD CreatorID;
  DWORD CreatorRevision;
};
typedef struct _ACPI_SDTHeader ACPI_SDTHeader;


// Extended BIOS Data Area
static DWORD *gEBDA = NULL;


#define SEARCH_RSD 0x20445352  // "RSD "
#define SEARCH_PTR 0x20525450  // "PTR "


static BYTE Checksum( BYTE *addr, DWORD count )
{
	BYTE result = 0;
	DWORD i = 0;

	for (i = 0; i < count; i++)
	{
		result += *addr;
		addr++;
	}

	return result;
}


static DWORD *RDSP_Search( void )
{
	// Look through memory for the RDSP table used for ACPI
	// Need to find "RSD PTR " either in first 1KB of EBDA (pointer to
	//   EBDA at 0x040E) or between 0x000E0000 to 0x000FFFFF

	DWORD *rdsp = NULL;

	// Get address of EBDA
	WORD *ebdasegaddr = (WORD*)0x040E;
	WORD ebdaseg = *ebdasegaddr;
	gEBDA = (DWORD*)(ebdaseg << 4);   // Calculate start address

	DWORD *current = gEBDA;
	DWORD i = 0;

	// Search in first 64 DWORDs of EBDA
	for (i = 0; i < 64; i++)
	{
		if (*current == SEARCH_RSD)  // Look for "RSD " 
		{
			// Found first part
			DWORD *nextdword = current;
			nextdword++;
			if (*nextdword == SEARCH_PTR)  // Look for "PTR "
			{
				// TODO: Checksum here
				// Found it
				rdsp = current;
			}
		}
		current += 2;
	}

	// Search from 0xE0000->0xFFFFF
	current = (DWORD*)0x000E0000;
	while (current < (DWORD*)0x00100000)
	{
		if (*current == SEARCH_RSD)  // Look for "RSD " 
		{
			// Found first part
			DWORD *nextdword = current;
			nextdword++;
			if (*nextdword == SEARCH_PTR)  // Look for "PTR "
			{
				// TODO: Checksum here
				// Found it
				rdsp = current;
			}
		}
		current += 2;
	}

	return rdsp;
}


void acpi_InitDevice( void )
{
	RDSP_Descriptor *RDSP = (RDSP_Descriptor*)RDSP_Search();
	if (RDSP != NULL)
	{
		DWORD rev = RDSP->Revision + 1;
		if (rev == 2)
		{
			// We have Extended ACPI 2.0 and above
			RDSP_Descriptor20 *RDSP20 = (RDSP_Descriptor20*)RDSP;
			DWORD XSDT = (DWORD)RDSP20->XsdtAddress;

			kprintf("[DEV] ACPI Rev=%d.0 RSDT=0x%x EBDA=0x%x XSDT=0x%x\n",
				        rev, RDSP->RsdtAddress, gEBDA, XSDT );
		}
		else
		{
			kprintf("[DEV] ACPI Rev=%d.0 RSDT=0x%x EBDA=0x%x Checksum=%b\n",
				        rev, RDSP->RsdtAddress, gEBDA, Checksum( (BYTE*)RDSP, sizeof(RDSP_Descriptor)) );
		}
	}

}
