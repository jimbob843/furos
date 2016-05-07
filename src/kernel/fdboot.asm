;
; Furos v2.0  -  Copyright Marlet Limited 2012
;
; File:				fdboot.asm
; Description:		Floppy disk boot block
;					This is the main entry point to everything. This
;					code builds the first 512 bytes bootblock on the
;					boot floppy. It's main objective is to locate the
;					stage two boot code and load it into memory.
; Author:			James Smith
; Created:			18-Aug-2012
;


[BITS 16]		; Real mode
[ORG 0x7C00]	; Standard boot block load address

;
; MEMORY MAP
;
; 0000:0000 Interrupt Vector Table (IVT)
; 0000:0400 BIOS data area
; 0000:0500 Furos FAT storage area
; 0000:4500 FREE
; 0000:5000 Furos data area
; 0000:6000 Furos memory map
; 0000:7C00	Bootblock
; 0000:7E00 FREE
; 0000:8000 Furos boot stack (4KB)
; 0000:9000 Furos STAGE TWO  (604KB available)
; A000:0000 VGA Video RAM
; C000:0000 BIOS ROM
; F000:FFFF Free memory starting at 0x100000
; 

;
; Boot Parameter Block (BFB) - Needed for the disk to be DOS compatible.
;

	jmp 0x0000:mainstart		; Must be 3 bytes before vendor ident
								; Force CS to be 0x0000 just in case the BIOS
								; has loaded us to 0x07C0:0000 not 0x0000:7C00
	
					db 'MSWIN4.1'	; Vendor ident
SectorSize			dw 512			; SS - Sector size in bytes
SectorsPerCluster	db 1			; AU - Sectors per cluster
NumberOfRS			dw 1			; RS - Reserved sector count
NumberOfFATs		db 2			; NF - Number of FATs on this disk
EntriesPerDir		dw 224			; DS - Directory size (number of files)
					dw 2880			; TS - Total number of sectors
					db 0xF0			; MD - Media descriptor (1.44MB floppy)
SectorsPerFAT		dw 9			; FS - Sectors per FAT
SectorsPerTrack		dw 18			; ST - Sectors per track
NumHeads			dw 2			; NH - Number of disk heads
					dd 0			; HS - Number of hidden sectors
					dd 0			; LS - Large sector count
					db 0			; Drive number (0x00 = floppy, 0x80 = HD)
					db 0			; Reserved
					db 0x29			; BootSig (0x29 indicates that the following three fields are present)
					dd 0xFFFFFFFF	; VolumeID (date and time)
					db 'FUROS_BOOT ' ; Volume label (11 bytes)
					db 'FAT12   '	; SystemID (8 bytes)


KernelBin			db 'KERNEL  BIN'	; 8.3 filename of the second stage
NoSecondStageMsg	db 'No kernel.bin!',13,10,0
FloppyErrMsg		db 'FDC Err 0x',0

;
; Main Code Start
;
mainstart:
	xor ax, ax
	mov ds, ax		; Reset DS
	mov es, ax		; Reset ES
	mov ss, ax		; Reset SS
	mov sp, 0x9000	; Allocate 0x1000 bytes of stack @ 0000:9000
	
	mov [bootdrive], dl	; Store boot drive
	
;
; Stage Two Boot Load
;   - Load the FAT and FAT12 directory from disk to 0x00500 (sectors 1-33)
;   - Locate the KERNEL.BIN in the root dir
;   - Load the file at 0x09000. (Gives 604k of space from 0x09000->0xA0000)
;
; NB: To save time and space, this code assumes a normal 1.44MB floppy
;     with 512 bytes per sector.
;
LoadStageTwo:
	; Setup load directory, start sector, and number of sectors to load.
	mov bx, ax
	mov es, bx
	mov bh, 0x05		; Load data at 0x0500
	mov cx, 32			; Read 32 sectors
	inc ax				; AX=1, Start at sector 1 (LBA start at 0)
	
.nextsector:
	call LoadLBA		; Get one sector
	inc ax				; Move to next sector
	add bh, 0x2			; Move dest addr by number of bytes loaded (BX+0x200)
	loop .nextsector
	
	; All sectors loaded now, scan the directory table for the file.
	mov bh, 0x29		; Hard-coded start of FAT12 directory table. (BX=0x2900)
	mov si, KernelBin	; String to search for
	mov di, bx			; Start address of search
	mov cl, 224			; File entries per dir.
	

;
; StringCompare
;
; SI - string 1
; DI - string 2
; returns DX : 0 = no match, 1 = match
;
StringCompare:
	push cx
	mov cx, 11		; Assume length of the filename we are looking for
	xor dx, dx		; Init DX to not found
	cld				; Set direction to up
	push di
	push si
.compareloop2:
	cmpsb			; Compare [SI] with [DI]
	jnz .exitcompare	; ZF set if character matches
	loop .compareloop2	; do the next one
 
	inc dx			; Match found, set return = 1
.exitcompare:
	pop si
	pop di
	pop cx

	cmp dx, 1
	jz .foundentry
	
	; No entry found, move to next entry
	add di, 32			; 32 bytes per entry
	add bx, 32
	loop StringCompare
	
	; String not found in any of the entries, print message and stop
	mov si, NoSecondStageMsg
	call WriteStr
	
	cli
	hlt		; STOP
	
.foundentry:
	; BX currently points to the directory entry we want
	mov ax, [bx+26]		; Get the first cluster number
						; Assume SectorsPerCluster = 1
						
	; Now load the file into memory at GS:0x9000
	mov bx, 0x9000
loadnextsector:
	push ax
	add ax, 31			; First cluster is (32-1) sectors from the start of disk
	call LoadLBA
	
	pop ax
	
	
;
; GetSector - Finds the next cluster from the FAT
;  AX - current cluster no., returns next cluster number in AX
;
GetSector:
	push bx
	push cx
	push dx

	mov cx, ax		; save AX for later

	mov dx, 3		; 3 nibbles per FAT entry
	mul dx			; Calculate offset in nibbles
	shr ax, 1		; Convert to bytes  (/2)
	add ax, 0x0500	; Start of FAT
	mov bx, ax
	mov ax, [bx]	; Get both bytes  BC DA => DABC (even), CD AB => ABCD (odd)

	bt cx, 0		; Check to see if CX (input cluster num) is even
	jc .oddFAT
.evenFAT:
	and ax, 0x0FFF	; Mask off the bits we don't want => 0ABC
	jmp .sectorexit

.oddFAT:
	shr ax, 4		; Move the value (ABCD) down to the right place => 0ABC

.sectorexit:
	pop dx
	pop cx
	pop bx
; end GetSector

;
; Increment dest pointer and loop for next cluster
;
	push ax
	mov ax, es
	add ax, 0x20		; Move dest addr by 0x200 bytes by moving ES
	mov es, ax
	pop ax	

	cmp ax, 0x0FFF		; Check for end of FAT chain
	jnz loadnextsector



;
; Enable A20 using standard keyboard controller method
;   Modern boards already seen to have this set by default, but do it anyway.
;
EnableA20:
	mov dx, 0x64
	mov cx, KeyboardWait
	call cx
    
    mov al, 0xD0        ; "Read Output Port" command
	out dx, al			; out 0x64, al

	; Wait for the data to appear in the output buffer    
    call KeyboardWaitData
    
    in al, 0x60         ; Get the value
    mov bl, al          ; Save for later
    
	call cx				; KeyboardWait
    
    mov al, 0xD1        ; "Write Output Port" command
	out dx, al			; out 0x64, al
 
	call cx				; KeyboardWait
    
    mov al, bl
    or al, 2            ; Set bit1 to enable A20
    out 0x60, al

	call cx				; KeyboardWait


;
; Memory Map
;   Used 0xE820 BIOS call to get a list of memory areas
;   Writes entry count to 0x6000 and entries to 0x6004 onwards
;
MemoryMap:
	xor bx, bx			; Continuation (zero for first call)
	mov es, bx
	mov bp, bx			; Clear BP, Use BP to keep track of the entry count
	mov di, 0x6004		; Buffer start address
.memloop:
	mov eax, 0xE820		; Function
	mov ecx, 24			; 24 bytes per entry
	mov edx, 0x534D4150		; 'SMAP'
	int 0x15
	jc .memdone			; stop when carry is set
	add di, 24			; increment dest pointer
	inc bp				; increment entry counter
	cmp bx, 0
	jnz .memloop			; continue if bx != 0
.memdone:
	mov [0x6000], bp	; Write the entry count to 0x6000


;
; Stop the floppy motors
;
StopFloppyMotors:
	mov dx, 0x3F2			; Controller A
	mov al, 0x0C			; Motors=OFF, DMA/IRQ=ON, Controller=ON
	out dx, al
	mov dx, 0x372			; Controller B
	mov al, 0x0C			; Motors=OFF, DMA/IRQ=ON, Controller=ON
	out dx, al
	
	
;
; Enter Protected Mode
;
EnterPMode:
	cli						; Stop interrupts
	lgdt [gdt_pointer]		; Load GDT

	mov eax, cr0
	or al, 1				; Set bit0 of cr0
	mov cr0, eax

	jmp 0x08:0x9000			; Do far jmp to enable pmode
							; CODE segment (0x08)


;
; WriteStr
;
; [input] si = addr of string
;
WriteStr:
	mov ah,0x0E		; Function (teletype)
	mov bx,0x0007	; Page 0, White text

.nextchar:
	lodsb			; Loads [SI] into AL and increases SI by one
	or al, al		; check al = 0
	jz .return
	int 0x10		; BIOS print char
	jmp .nextchar

.return:
	ret
	

;
; LoadLBA
;   AX=LBA sector number
;   ES:BX=Dest addr
;
LoadLBA:
	push dx
	push cx
	push ax			; Save trashed registers
	push bx			; Save dest addr for later

	xor dx, dx
	mov bx, 18		; Sectors per track
	div bx			; ax/bx -> ax,dx
	inc dx
	mov cx, dx		; Put sector count into CL

	xor dx, dx
	mov bx, 2		; Number of Heads
	div bx			; AL=Cylinder, DL=Head, CL=Sector

 ; Now do the load
	mov ch, al		; Put the cylinder (track) into CH
	shl dx, 8		; Shift head into DH
	mov byte dl, [bootdrive]	; Get booting drive number
	mov ax, 0x0201  ; BIOS read, one sector
	pop bx			; Get the address to put the data that we saved earlier
	int 0x13		; Do it.

	; Carry flag set if there is an error. Code in AH.
	jc FloppyError
	
	pop ax
	pop cx
	pop dx
	ret
	
FloppyError:
	; Something went wrong. Report the error code from AH.
	push ax
	mov si, FloppyErrMsg
	call WriteStr
	pop ax
	
	; Print the error code
	rol ax, 4
	call PrintDigit
	rol ax, 4
	call PrintDigit
	
	cli
	hlt				; STOP

;
; Prints a single hexadecimal digit to the screen
;
PrintDigit:
	push ax
	and al, 0x0F		; Mask the nibble we want
	cmp al, 9
	jle .nothexdigit
	add al, 0x07		; Shift up to the letter A-F
.nothexdigit:
	add al, 0x30		; Convert to ASCII
	mov ah, 0x0E		; Function (teletype)
	mov bx, 0x0007		; Page 0, White text
	int 0x10			; Call BIOS
	pop ax
	ret
 
;
; KeyboardWait - wait for keyboard controller to be ready
;
KeyboardWait:
	in al, dx
	and al, 2			; Check bit1 of controller status
	jnz KeyboardWait	; Loop until 0
	ret


;
; KeyboardWaitData - wait for keyboard controller to be ready
;
KeyboardWaitData:
	in al, dx
	and al, 1				; Check bit0 of controller status
	jz KeyboardWaitData		; Loop until 1
	ret


;
; GDT (This is just a temporary one to switch to PMode)
;
gdt_start:
 dw 0x0000	; NULL selector
 dw 0x0000
 dw 0x0000
 dw 0x0000

 dw 0xFFFF	; CODE selector, Base=0000, Len=4GB
 dw 0x0000	; Base
 dw 0x9A00	; Present, DPL=0, DT=App, CODE, Execute/Read, Non-conforming
 dw 0x00CF	; G=4K, D=32bit, Seg high nibble = 0xF

 dw 0xFFFF	; DATA selector, Base=0000, Len=4GB
 dw 0x0000	; Base
 dw 0x9200	; Present, DPL=0, DT=App, DATA, Read/Write, Expand-up
 dw 0x00CF	; G=4K, D=32bit, Seg high nibble = 0xF

gdt_pointer:
 dw gdt_pointer - gdt_start			; Calculate length of GDT (WHY -1????)
 dd gdt_start


; Boot drive
bootdrive:
 db 0x00


;
; Bootblock end
;
times 510-($-$$) db 0	; Fill with zeros so that binary output is 512 bytes
db 0x55					; Boot loader signature
db 0xAA
