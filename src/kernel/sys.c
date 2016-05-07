//
// Furos v2.0  -  Copyright Marlet Limited 2012
//
// File:			sys.c
// Description:		System Call handling
// Author:			James Smith
// Created:			10-Oct-2012
//

#include "kernel.h"

void sys_SystemCallHandler( DWORD eax, DWORD ebx, DWORD ecx, DWORD edx )
{
	switch (eax) {
	default:
		kprintf( "SYSCALL: eax=%x ebx=%x\n", eax, ebx );
	}
}
