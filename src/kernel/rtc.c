/*
 * Furos v2.0   Copyright Marlet Limited 2012
 *
 * File:			rtc.c
 * Description:		Real Time Clock and CMOS
 * Author:			James Smith
 * Created:			20-Sep-2012
 *
 */

#include "kernel.h"

// CMOS I/O PORTS
#define CMOS_PORT_ADDR    0x70
#define CMOS_PORT_DATA    0x71

// CMOS MEMORY ADDRESSES
#define CMOS_SECONDS      0x00
#define CMOS_SECONDSALARM 0x01
#define CMOS_MINUTES      0x02
#define CMOS_MINUTESALARM 0x03
#define CMOS_HOURS        0x04
#define CMOS_HOURSALARM   0x05
#define CMOS_DAYOFWEEK    0x06
#define CMOS_DAYOFMONTH   0x07
#define CMOS_MONTH        0x08
#define CMOS_YEAR         0x09
#define CMOS_RTCSTATUSA   0x0A
#define CMOS_RTCSTATUSB   0x0B
#define CMOS_RTCSTATUSC   0x0C
#define CMOS_RTCSTATUSD   0x0D
#define CMOS_DISKETTETYPE 0x10

// Bits in CMOS_RTCSTATUSC
#define RTC_INT_UPDATE    0x10
#define RTC_INT_ALARM     0x20
#define RTC_INT_PERIODIC  0x40
#define RTC_INT_IRQ8      0x80

// Top bit of CMOS address disables NMI
#define NMI_DISABLE       0x80


// Number of 1ms ticks since the RTC was enabled
static DWORD GlobalTickCount = 0;

//
// rtc_DisableNMI
//  - disables the Non-Maskable Interrupt
//
void rtc_DisableNMI( void )
{
	OUTPORT_BYTE( CMOS_PORT_ADDR, NMI_DISABLE );
}


//
// rtc_EnableNMI
//  - enables the Non-Maskable Interrupt
//
void rtc_EnabledNMI( void )
{
	OUTPORT_BYTE( CMOS_PORT_ADDR, 0x00 );
}


//
// rtc_ReadCMOS
//  - reads a byte values from the CMOS
//
BYTE rtc_ReadCMOS( BYTE cmosaddr )
{
	DISABLE_INTERRUPTS();

	OUTPORT_BYTE( CMOS_PORT_ADDR, cmosaddr | NMI_DISABLE );
	BYTE value = INPORT_BYTE( CMOS_PORT_DATA );
	OUTPORT_BYTE( CMOS_PORT_ADDR, 0x00 );  // Enable NMI

	ENABLE_INTERRUPTS();

	return value;
}


//
// rtc_WriteCMOS
//  - writes a byte values from the CMOS
//
void rtc_WriteCMOS( BYTE cmosaddr, BYTE value )
{
	DISABLE_INTERRUPTS();

	OUTPORT_BYTE( CMOS_PORT_ADDR, cmosaddr | NMI_DISABLE );
	OUTPORT_BYTE( CMOS_PORT_DATA, value );
	OUTPORT_BYTE( CMOS_PORT_ADDR, 0x00 );  // Enable NMI

	ENABLE_INTERRUPTS();
}


//
// rtc_IRQHandler
//  - RTC interrupt handler
//
void rtc_IRQHandler( void )
{
	// Check that the interrupt is from the RTC
	// This read also clears the register
	BYTE status = rtc_ReadCMOS( CMOS_RTCSTATUSC );

	if (status & RTC_INT_IRQ8)
	{
		// IRQ8 has fired
		if (status & RTC_INT_PERIODIC)
		{
			// Periodic interrupt has fired. Increment the global tick count.
			GlobalTickCount++;

			// Just for debugging
			if ((GlobalTickCount % 1000) == 0) {
//				kprintf("[%d]", (GlobalTickCount / 1000));
			}
		}
	}

}


//
// rtc_InitDevice
//  - RTC device initialisation
//
void rtc_InitDevice( void )
{
	// Attach a handler to the RTC interrupt on IRQ8
	irq_RegisterIRQHandler( 8, rtc_IRQHandler );

	// Enable the RTC periodic interrupt
	rtc_WriteCMOS( CMOS_RTCSTATUSA, 0x26 );  // Set no clock divider and 1ms period
	rtc_WriteCMOS( CMOS_RTCSTATUSB, RTC_INT_PERIODIC | 0x02 );  // Set periodic interrupt on + 24H clock

	// Turn on IRQ8
	ENABLE_IRQ( 8 );

	kprintf( "[DEV] RTC 1ms period\n" );
}


//
// rtc_GetGlobalTickCount
//
DWORD rtc_GetGlobalTickCount( void )
{
	return GlobalTickCount;
}


//
// rtc_SetGlobalTickCount
//
void rtc_SetGlobalTickCount( DWORD newtickcount )
{
	GlobalTickCount = newtickcount;
}
