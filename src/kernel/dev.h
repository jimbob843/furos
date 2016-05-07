//
// Furos v2.0  -  Copyright Marlet Limited 2012
//
// File:			dev.h
// Description:		Device manager headers
// Author:			James Smith
// Created:			10-Oct-2012
//

#ifndef __DEV_H__
#define __DEV_H__

//
// PCI Device Info
//

struct _PCIDeviceInfo {
	WORD vendorid;
	WORD deviceid;
	BYTE bus;
	BYTE device;
	BYTE function;
	BYTE class_code;
	BYTE subclass_code;
	BYTE interface_code;
	BYTE irq;
	DWORD memory[5];
	DWORD memory_size[5];
	BYTE memory_prefetch[5];
	DWORD ports[5];
	DWORD ports_size[5];
};
typedef struct _PCIDeviceInfo PCIDeviceInfo;


#define MAX_PATH 256

//
// FILE
//
struct _FILE {
//	FAT12DirectoryEntry *direntry;
	DWORD fileposition;
	BYTE *buffer;
	int bufferposition;
	WORD currentcluster;
};
typedef struct _FILE FILE;


//
// DEVICE
//
typedef KRESULT (INITDEVICEFUNC)( void );
typedef KRESULT (RESETDEVICEFUNC)( void );
typedef KRESULT (OPENDEVICEFUNC)( BYTE*, DWORD, FILE** );
typedef KRESULT (CLOSEDEVICEFUNC)( void );
typedef KRESULT (READBLOCKFUNC)( BYTE, BYTE, BYTE, BYTE, BYTE* );
typedef KRESULT (WRITEBLOCKFUNC)( BYTE, BYTE, BYTE, BYTE, BYTE* );

struct _DEVICE {
	INITDEVICEFUNC *InitDevice;
	RESETDEVICEFUNC *ResetDevice;
	OPENDEVICEFUNC *OpenDevice;
	CLOSEDEVICEFUNC *CloseDevice;
	READBLOCKFUNC *ReadBlock;
	WRITEBLOCKFUNC *WriteBlock;
	struct _DEVICE *NextDevice;
};
typedef struct _DEVICE DEVICE;

#endif /* __DEV_H__ */
