//
// Furos v2.0  -  Copyright Marlet Limited 2012
//
// File:			floppy.c
// Description:		Floppy Driver
// Author:			James Smith
// Created:			10-Oct-2012
//

#include "kernel.h"

KRESULT flp_InitDevice( WORD portaddr )
{
	kprintf( "[DEV] Floppy Port 1 at 0x%w\n", portaddr );

	return KRESULT_SUCCESS;
}
