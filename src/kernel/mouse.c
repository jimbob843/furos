/*
 * Furos v2.0   Copyright Marlet Limited 2012
 *
 * File:			mouse.c
 * Description:		Internal PS/2 mouse driver
 * Author:			James Smith
 * Created:			10-Oct-2012
 *
 */

#include "kernel.h"

#define MOUSE_SCALEX 1
#define MOUSE_SCALEY 1
#define MOUSE_SCALEZ 1
#define MOUSE_BUTTONS 8
#define MAX_BYTECOUNT 4		// Largest possible packet size
#define MAX_XCOORD 799
#define MAX_YCOORD 599

#define PORT_COMMAND 0x64	// Write
#define PORT_STATUS  0x64	// Read
#define PORT_DATA    0x60   // Read/Write

static BYTE MouseDeviceID = 0;
static int MouseDeviceByteCount = 3;

static int bytecounter = 0;
static char inputbuffer[MAX_BYTECOUNT];
static int rawxcoord = 40 * MOUSE_SCALEX;
static int rawycoord = 12 * MOUSE_SCALEY;
static int rawzcoord =  0 * MOUSE_SCALEZ;
static char rawbuttons[MOUSE_BUTTONS];
static int xcoord = 40;
static int ycoord = 12;
static int zcoord =  0;

static void KeyboardWait( void );
static void KeyboardWaitData( void );
void ENABLE_IRQ( DWORD irq );
void DISABLE_IRQ( DWORD irq );




void SendKeyboardCommandWithData( BYTE command, BYTE data ) {
	KeyboardWait();
	OUTPORT_BYTE( 0x64, command );

	KeyboardWait();
	OUTPORT_BYTE( 0x60, data );
}

BYTE SendKeyboardCommandWithResult( BYTE command ) {
	KeyboardWait();
	OUTPORT_BYTE( 0x64, command );

	KeyboardWaitData();
	return INPORT_BYTE( 0x60 );
}

BYTE SendMouseCommand( BYTE command ) {
	BYTE result = 0x00;

	KeyboardWait();
	OUTPORT_BYTE( 0x64, 0xD4 );		// "Write Mouse Device" command

	KeyboardWait();
	OUTPORT_BYTE( 0x60, command );

	KeyboardWaitData();
	result = INPORT_BYTE( 0x60 );	// Should be 0xFA

	return result;
}


void SendMouseCommandWithData( BYTE command, BYTE data ) {
	SendMouseCommand( command );
	SendMouseCommand( data );
}


BYTE SendMouseCommandWithResult( BYTE command ) {

	SendMouseCommand( command );

	// Wait for the response from the mouse
	KeyboardWaitData();
	return INPORT_BYTE( 0x60 );
}


void mse_IRQHandler( void ) {

	inputbuffer[bytecounter++] = INPORT_BYTE( 0x60 );

	if (bytecounter == MouseDeviceByteCount) {
		int oldxcoord = xcoord;
		int oldycoord = ycoord;

		// Get the values from the packet
		int xdelta = inputbuffer[1];
		int ydelta = inputbuffer[2];

		// Accelerate the mouse pointer
		if (abs(xdelta) > 5) {
			xdelta *= 3;
		} else {
			if (abs(xdelta) > 3) {
				xdelta *= 2;
			}
		}
		if (abs(ydelta) > 5) {
			ydelta *= 3;
		} else {
			if (abs(ydelta) > 3) {
				ydelta *= 2;
			}
		}

		// Check the mouse wheel
		if (MouseDeviceID == 0x03) {
			rawzcoord -= (int)inputbuffer[3];
		}
		if (MouseDeviceID == 0x04) {
			rawzcoord -= (int)(inputbuffer[3] & 0x0F);
		}
		// And the buttons
		rawbuttons[0] = inputbuffer[0] & 0x01;
		rawbuttons[1] = inputbuffer[0] & 0x02;
		rawbuttons[2] = inputbuffer[0] & 0x04;
		if (MouseDeviceID == 0x04) {
			rawbuttons[3] = inputbuffer[3] & 0x10;
			rawbuttons[4] = inputbuffer[3] & 0x20;
		}

		// Update the mouse position
		rawxcoord += (int)xdelta;
		rawycoord -= (int)ydelta;
		// Clamp the position inside the screen
		if (rawxcoord > (MAX_XCOORD*MOUSE_SCALEX))  rawxcoord = (MAX_XCOORD*MOUSE_SCALEX);
		if (rawxcoord < 0)                          rawxcoord = 0;
		if (rawycoord > (MAX_YCOORD*MOUSE_SCALEY))  rawycoord = (MAX_YCOORD*MOUSE_SCALEY);
		if (rawycoord < 0)                          rawycoord = 0;
		xcoord = rawxcoord / MOUSE_SCALEX;
		ycoord = rawycoord / MOUSE_SCALEY;

		// Draw the mouse pointer
		if ((xcoord != oldxcoord) || (ycoord != oldycoord)) {
//			gfx_SetCursorPosition( xcoord, ycoord );
		}

		// Draw the mouse wheel indicator
		if ((MouseDeviceID == 0x03) || (MouseDeviceID == 0x04)) {
//			BYTE *z = (BYTE*)(0xB8030);
//			*z = 0x30 + (BYTE)rawzcoord;
		}

		// Reset for the start of a new packet
		bytecounter = 0;

#if 0
		// Output the packet
		kprintf(">X:%b Y:%b", inputbuffer[1], inputbuffer[2] );
		if (MouseDeviceID == 0x03) {
			kprintf(" Z:%b", inputbuffer[3] );
		}
		if (inputbuffer[0] & 0x01) {
			kprintf("L");
		}
		if (inputbuffer[0] & 0x02) {
			kprintf("R");
		}
		if (inputbuffer[0] & 0x04) {
			kprintf("M");
		}
		if (MouseDeviceID == 0x04) {
			if (inputbuffer[3] & 0x10) {
				kprintf("B4");
			}
			if (inputbuffer[3] & 0x20) {
				kprintf("B5");
			}
		}
		kprintf("<");
#endif

	}
}



KRESULT mse_InitDevice( void ) {

	int i = 0;

	// Turn off IRQ12 while we init
	DISABLE_IRQ( 12 );

	// Also stop the keyboard interrupts,
	//  otherwise we can get stuck if an IRQ1 fires!
	DISABLE_IRQ( 1 );

	// Enable Mouse Interrupt on IRQ12
	BYTE commandbyte = SendKeyboardCommandWithResult( 0x20 );  // "Read Command Byte"
	commandbyte |= 0x02;	        // Set bit 1    - Enable INT12
	commandbyte &= 0xDF;	        // Clear bit 5  - Enable Mouse
	SendKeyboardCommandWithData( 0x60, commandbyte );  // "Write Command Byte"

	// Initialise the Mouse
	BYTE result = SendMouseCommand( 0xF4 );		// "Mouse Init" command
	if (result == 0xFE) {
		// Mouse did not respond
		kprintf("[DEV] No mouse detected\n");
		// Need to re-enable the keyboard IRQ
		ENABLE_IRQ( 1 );
		return KRESULT_SUCCESS;
	}

	// Attempt to enter "scrolling mode" for a Microsoft Intellimouse
	SendMouseCommandWithData( 0xF3, 0xC8 );		// 200
	SendMouseCommandWithData( 0xF3, 0x64 );		// 100
	SendMouseCommandWithData( 0xF3, 0x50 );		// 80

	// Attempt to enter "scrolling+buttons mode" for a Microsoft Intellimouse
	SendMouseCommandWithData( 0xF3, 0xC8 );		// 200
	SendMouseCommandWithData( 0xF3, 0xC8 );		// 200
	SendMouseCommandWithData( 0xF3, 0x50 );		// 80

	// Get the ID of the mouse
	MouseDeviceID = SendMouseCommandWithResult( 0xF2 );

	if ((MouseDeviceID == 0x03) || (MouseDeviceID == 0x04)) {
		MouseDeviceByteCount = 4;
	} else {
		MouseDeviceByteCount = 3;
	}

	// Reset some of the mouse state
	bytecounter = 0;
	for (i=0; i<MOUSE_BUTTONS; i++) {
		rawbuttons[i] = 0;
	}


	// Tell the PIC that this IRQ is ready
	irq_RegisterIRQHandler( 12, mse_IRQHandler );
	ENABLE_IRQ( 12 );
	ENABLE_IRQ( 1 );	// And the keyboard

	kprintf("[DEV] PS/2 Mouse. DeviceID=0x%b\n", MouseDeviceID );

	return KRESULT_SUCCESS;
}



static void KeyboardWait( void ) {
	BYTE status = 0x02;

	// Wait for bit 2 to become 0
	while (status & 0x02) {
		status = INPORT_BYTE( 0x64 );
	}
}


static void KeyboardWaitData( void ) {
	BYTE status = 0x00;

	// Wait for bit 1 to become 1
	while (!(status & 0x01)) {
		status = INPORT_BYTE( 0x64 );
	}
}
