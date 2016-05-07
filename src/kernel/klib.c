//
// Furos v2.0  -  Copyright Marlet Limited 2012
//
// File:			klib.c
// Description:		Kernel library functions
// Author:			James Smith
// Created:			18-Aug-2012
//

#include "kernel.h"


//
// strcpy
//
int strcpy( char *s1, char *s2 )
{
	while (*s1++ = *s2++) ;
}

//
// strcmp
//
int strcmp( char *s1, char *s2 )
{
 
	while (*s1 == *s2++) {
		if (*s1++ == 0) {
			return 0;
		}
	}
	
	return (*s1 - *(s2-1));
}

//
// strlen
//
int strlen( char *s )
{
	int count = 0;
	while (*s != 0)
	{
		count++;
		s++;
	}
	return count;
}

//
// memcpy
//
void memcpy( void *src, void *dest, int bytes )
{
	MEMCPY( src, dest, bytes );
}

//
// memclr
//
void memclr( void *dest, int bytes )
{
	MEMCLR( dest, bytes );
}

//
// memcmp
//
int memcmp( BYTE *s1, BYTE *s2, DWORD byte_count )
{
	const unsigned char *us1 = (const unsigned char *) s1;
	const unsigned char *us2 = (const unsigned char *) s2;

	while (byte_count != 0) {
	 byte_count--;

	 if (*us1 != *us2)
		 return (*us1 < *us2) ? -1 : +1;

	 us1++;
	 us2++;
	}

	return 0;
}


//
// kprintf
//
void kprintf( BYTE *fmt, ... )
{
	va_list ap;
	va_start(ap,fmt);

	while (*fmt)
	{
		if (*fmt == '%')
		{
			fmt++;
			switch (*fmt)
			{
				case 'd':
					con_WriteDWORD_Dec( 0, va_arg(ap, DWORD) );
					break;
				case 'x':
					con_WriteDWORD( 0, va_arg(ap, DWORD) );
					break;
				case 'w':
					con_WriteWORD( 0, (WORD)(va_arg(ap, DWORD)&0xFFFF) );
					break;
				case 's':
					con_WriteString( 0, va_arg(ap, BYTE*) );
					break;
				case 'c':
					con_WriteChar( 0, (BYTE)(va_arg(ap, DWORD) & 0xFF) );
					break;
				case 'b':
					con_WriteBYTE( 0, (BYTE)(va_arg(ap, DWORD) & 0xFF) );
					break;
				default:
					con_WriteChar( 0, *fmt );
					break;
			}
		}
		else
		{
			con_WriteChar( 0, *fmt );
		}
		fmt++;
	}
}
