//
// Furos v2.0  -  Copyright Marlet Limited 2012
//
// File:			console.c
// Description:		Multi-screen text console
// Author:			James Smith
// Created:			18-Aug-2012
//

#include "kernel.h"

#define MAX_CONSOLES 8
#define TEXTVIDEO_BASEADDR (BYTE *)0xB8000
#define SCREEN_WIDTH  80
#define SCREEN_HEIGHT 25

#define PORT_BOCHS_DEBUG_CONSOLE 0xE9
static BOOL BochsE9Hack = FALSE;

#define COL_BLACK        0x0
#define COL_BLUE         0x1
#define COL_GREEN        0x2
#define COL_CYAN         0x3
#define COL_RED          0x4
#define COL_MAGENTA      0x5
#define COL_BROWN        0x6
#define COL_LIGHTGREY    0x7
#define COL_DARKGREY     0x8
#define COL_LIGHTBLUE    0x9
#define COL_LIGHTGREEN   0xA
#define COL_LIGHTCYAN    0xB
#define COL_LIGHTRED     0xC
#define COL_LIGHTMAGENTA 0xD
#define COL_YELLOW       0xE
#define COL_WHITE        0xF

struct _Console
{
	DWORD ConsoleID;
	WORD *Buffer;
	WORD PositionX;
	WORD PositionY;
	WORD ColourMode;
};
typedef struct _Console Console;

static Console ConsoleArray[MAX_CONSOLES];
static Console *CurrentConsole = NULL;

static void con_Reset( Console *con );


KRESULT con_InitDevice( void )
{
	// Check for E9 debug support
	if (INPORT_BYTE(PORT_BOCHS_DEBUG_CONSOLE) == PORT_BOCHS_DEBUG_CONSOLE)
	{
		// Bochs E9 debug output hack is available
		BochsE9Hack = TRUE;
	}

	CurrentConsole = &ConsoleArray[0];

	CurrentConsole->ConsoleID = 0;
	con_Reset( CurrentConsole );

	return KRESULT_SUCCESS;
}


static void con_UpdateCursor( Console *con )
{
	if (con == CurrentConsole)
	{
		// Tell the VGA adapter where to put the cursor
		DWORD cursorpos = (con->PositionY * SCREEN_WIDTH) + con->PositionX;
		OUTPORT_BYTE( 0x3D4, 0x0F );
		OUTPORT_BYTE( 0x3D5, (cursorpos & 0x00FF) );
		OUTPORT_BYTE( 0x3D4, 0x0E );
		OUTPORT_BYTE( 0x3D5, (cursorpos & 0xFF00) >> 8 );
	}
}


static void con_SetCursorPosition( Console *con, BYTE x, BYTE y )
{
	con->PositionX = x;
	con->PositionY = y;
	con_UpdateCursor( con );
}

static void con_SetColourMode( Console *con, BYTE foreColour, BYTE backColour )
{
	// Pre-shifted for efficiency later
	con->ColourMode = (foreColour | (backColour << 4)) << 8;
}


static void con_Reset( Console *con )
{
	DWORD screensize = (SCREEN_WIDTH * SCREEN_HEIGHT * 2);

	// Clear the console buffer
	if (con->Buffer != NULL)
	{
		memclr( (BYTE*)con->Buffer, screensize );
	}

	if (con == CurrentConsole)
	{
		// Also clear the screen
		memclr( TEXTVIDEO_BASEADDR, screensize );
	}

	// Reset the cursor
	con_SetCursorPosition( con, 0, 0 );

	// Reset the colour
	con_SetColourMode( con, COL_WHITE, COL_BLACK );
}


static void con_ScrollUp( Console *con, int numlines )
{
	/* Scrolls the page up numlines by copying memory */
	DWORD bytestoscroll = (numlines * SCREEN_WIDTH * 2);
	DWORD bytesperpage = (SCREEN_HEIGHT * SCREEN_WIDTH * 2);
	DWORD bytestocopy = bytesperpage - bytestoscroll;

	//
	// Perform for the console buffer
	//
	if (con->Buffer != NULL)
	{
		BYTE *dest_addr = (BYTE*)con->Buffer;
		BYTE *src_addr = (BYTE*)con->Buffer + bytestoscroll;

		// Copy the old text up the screen by numlines
		memcpy( src_addr, dest_addr, bytestocopy );
		dest_addr += bytestocopy;

		// Clear the new lines at the bottom
		memclr( dest_addr, bytestoscroll );
	}

	//
	// Now repeat for the screen if this console is currently visible
	//
	if (con == CurrentConsole)
	{
		BYTE *dest_addr = TEXTVIDEO_BASEADDR;
		BYTE *src_addr = TEXTVIDEO_BASEADDR + bytestoscroll;

		// Copy the old text up the screen by numlines
		memcpy( src_addr, dest_addr, bytestocopy );
		dest_addr += bytestocopy;

		// Clear the new lines at the bottom
		memclr( dest_addr, bytestoscroll );
	}

//	con_UpdateCursor();
}

static void con_DecCursorPosition( Console *con )
{
	con->PositionX--;

	if (con->PositionX < 0)
	{
		/* we've hit the side, so wrap around */
		con->PositionX = 0;
		if ((con->PositionY-1) == 0)
		{
			/* We're at the top of the screen so just stop */
		}
		else
		{
			con->PositionY--;
		}
	}

	con_UpdateCursor( con );
}


static void con_IncCursorPosition( Console *con, char c )
{
	/* Moves the cursor based on the character being printed (c) */
	if (c == 10)	/* NL */
	{
		if ((con->PositionY+1) == SCREEN_HEIGHT)
		{
			/* We're at the bottom of the screen so scroll up */
			con_ScrollUp( con, 1 );
		}
		else
		{
			con->PositionY++;
		}
		con->PositionX = 0;
		con_UpdateCursor( con );

		if (BochsE9Hack == TRUE)
		{
			OUTPORT_BYTE( PORT_BOCHS_DEBUG_CONSOLE, c );
		}

		return;
	}

	if (c == 13)	/* CR */
	{
		return;
	}

	/* else */
	con->PositionX++;
	if (con->PositionX >= SCREEN_WIDTH)
	{
		/* we've hit the side, so wrap around */
		con->PositionX = 0;
		if ((con->PositionY+1) == SCREEN_HEIGHT)
		{
			/* We're at the bottom of the screen so scroll up */
			con_ScrollUp( con, 1 );
		}
		else
		{
			con->PositionY++;
		}
	}

	con_UpdateCursor( con );
}


static void con_PutChar( Console *con, char c )
{
	DWORD offset = (con->PositionX) + (con->PositionY * SCREEN_WIDTH);
	WORD value = con->ColourMode | c;

	// Update the console buffer
	if (con->Buffer != NULL)
	{
		WORD *screen_addr = con->Buffer + offset;
		*screen_addr = value;
	}

	// Also update the screen
	if (con == CurrentConsole)
	{
		WORD *screen_addr = (WORD*)TEXTVIDEO_BASEADDR + offset;
		*screen_addr = value;
	}
}


static con_IntWriteChar( Console *con, char c )
{
	if (c == 8)
	{
		// DELETE
		con_DecCursorPosition( con );
	}
	else
	{
		if (c >= 0x20)
		{
			con_PutChar( con, c );
			if (BochsE9Hack == TRUE)
			{
				OUTPORT_BYTE( PORT_BOCHS_DEBUG_CONSOLE, c );
			}
		}
		con_IncCursorPosition( con, c );
	}

	con_PutChar( con, ' ' );
}


KRESULT con_WriteString( DWORD consoleID, char *s )
{
	// Find the correct console
	if (consoleID >= MAX_CONSOLES)
	{
		return KRESULT_NO_DEVICE;
	}

	// Write the string to the console's buffer
	char c = 0;
	while (c = *s++)
	{
		con_IntWriteChar( &ConsoleArray[consoleID], c );
	}
}


KRESULT con_WriteChar( DWORD consoleID, char c )
{
	// Find the correct console
	if (consoleID >= MAX_CONSOLES)
	{
		return KRESULT_NO_DEVICE;
	}

	// Write the string to the console's buffer
	con_IntWriteChar( &ConsoleArray[consoleID], c );
}


KRESULT con_StreamWriteHexDigit( DWORD consoleID, char c )
{
	/* Input: 0-F, prints char */
	char x = '0';

	if ((c < 0) || (c > 0xF))
	{
		/* Bad char */
		x = '0';
	} else
	{
		if ((c >= 0) && (c <= 9))
		{
			x = '0' + c;
		} else {
			x = '0' + c + 7;
		}
	}

	return con_WriteChar( consoleID, x );
}


KRESULT con_WriteBYTE( DWORD consoleID, BYTE x )
{
	con_StreamWriteHexDigit( consoleID, (0xF & (x >> 4)) );
	return con_StreamWriteHexDigit( consoleID, (0xF & (x >> 0)) );
}


KRESULT con_WriteWORD( DWORD consoleID, WORD x )
{
	KRESULT result = KRESULT_SUCCESS;
	int i = 0;
	int shift = 16;		// 16 bits in total

	// Print each nibble of the WORD, starting at the top
	for (i=0; i<4; i++)
	{
		shift -= 4;		// Move to next nibble
		result = con_StreamWriteHexDigit( consoleID, (0xF & (x >> shift)) );
	}

	return result;
}


KRESULT con_WriteDWORD( DWORD consoleID, DWORD x )
{
	KRESULT result = KRESULT_SUCCESS;
	int i = 0;
	int shift = 32;		// 32 bits in total

	// Print each nibble of the DWORD, starting at the top
	for (i=0; i<8; i++)
	{
		shift -= 4;		// Move to next nibble
		result = con_StreamWriteHexDigit( consoleID, (0xF & (x >> shift)) );
	}

	return result;
}


KRESULT con_WriteDWORD_Dec( DWORD consoleID, DWORD x )
{
	KRESULT result = KRESULT_SUCCESS;
	BYTE revdigits[12];
	DWORD current = x;
	int i = 0;
	int j = 0;

	if (x == 0)
	{
		return con_StreamWriteHexDigit( consoleID, 0 );
	}

	while (current>0)
	{
		revdigits[i] = current % 10;
		current /= 10;
		i++;
	}

	for (j=i; j>0; j--)
	{
		result = con_StreamWriteHexDigit( consoleID, revdigits[j-1] );
	}

	return result;
}

