;
; Furos v2.0  -  Copyright Marlet Limited 2012
;
; File:				startup.asm
; Description:		First part of the second stage. Start of the nitty
;					gritty of getting things up and running.
; Author:			James Smith
; Created:			18-Aug-2012
;


[BITS 32]			; Protected mode
SECTION .text		; Place code in .text section

;
; Declare symbols for export
;
global EntryPoint
global _KPTR_PointerTableStart
global KPTR_GDT_Length
global KPTR_GDT
global KPTR_KernelTSS

;
; Declare external symbols
;
extern _kernel_main
extern _irq_GenericIRQHandler
extern _exp_InvalidOpCodeHandler
extern _exp_InvalidTaskHandler
extern _exp_GPFHandler
extern _exp_DefaultHandler
extern _exp_PageFaultHandler
extern _sys_SystemCallHandler


;
; EntryPoint - beginning of kernel startup code
;
EntryPoint:
	
	; Make sure that the segment registers are setup before we do
	; anything else. Uses GDT setup in boot floppy code.
	; CS has already been set to CODE segment (0x08)
	mov eax, 0x10		; Set to DATA segment (0x10)
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	mov eax, [KPTR_KernelStack]
	mov esp, eax		; Set stack space: 0x00110000->0x00100000
						; DWORD aligned. 64KB.
	
	; Do paging first as all the following addresses depend on it
	call InitPaging
	call InitGDT
	call InitPIC
	call InitIDT
	call InitPIT
	call StartKernelTask
							
	sti						; Enabled interrupts
	call _kernel_main		; Call the OS

	; OS is now running, wait for an interrupt.
waitloop:
	hlt						; Stop here, but will continue on return from interrupt
	jmp waitloop			;  so keep looping


;
; InitPaging
;   Sets up the pagin directories and enables paging.
;   First page table is the first 4MB of memory used by the OS.
;
InitPaging:
	mov ebx, [KPTR_Paging_MasterTable]		; Physical address of master table
	mov cr3, ebx							; Set the master table address
	
	; Clear Master Table
	mov eax, 0x00000002		; Supervisor, R/W, Not-Present			
	mov ecx, 0x400			; 1024 entries, 4 bytes per entry = 0x400 DWORDs
	mov edi, ebx			; Table address
	rep stosd

	;
	; Add the first page table entry. Points to page table for
	;  first 4MB between 0x00000000 and 0x003FFFFF
	;
	mov ebx, [KPTR_Paging_MasterTable]			; Address of master table entry 0x000
	mov eax, [KPTR_Paging_FirstTable]			; Get the address of the first table
	or  eax, 0x3								; Set page table addr, Sup, R/W, Present
	mov dword [ebx], eax						; EAX=0x00122003 => EBX=0x00120000

	;	
	; Populate the first page table
	; Direct mapping between virtual and physical addresses.
	; 4MB from physical 0x00000000 to 0x00149000 (start of kernel heap)
	;
	mov eax, [KPTR_KernelHeapStart]			; Get end address for 1 to 1 mapping
	shr eax, 12								; Calculate number of 4KB pages
	mov ecx, eax							; Move page count to ecx
	xor eax, eax							; Set StartAddr = 0x00000000
	add eax, 0x3							; Set R/W + Present
	mov ebx, [KPTR_Paging_FirstTable]		; Get start address of page table	
.nextentry:
	mov dword [ebx], eax
	add eax, 4096					; Move to next address (each page is 4096 bytes)
	add ebx, 4						; Move to next table entry (each entry is 4 bytes)
	dec ecx							; Reduce counter
	jnz .nextentry

	;
	; Set the very top page directory entry to be the
	;  master directory itself. Creates a map of all page tables
	;  in virtual memory between 0xFFC00000 and 0xFFFFFFFF
	;  with master directory appearing at 0xFFFFF000
	;
	mov eax, [KPTR_Paging_MasterTable]	; Get address of master directory
	add eax, 0xFFC						; Goto last directory entry
	mov ebx, eax						; Address of last directory entry = 0x00121FFC
	mov eax, [KPTR_Paging_MasterTable]	; Get the address of the 4KB page to map
	or  eax, 0x3						; Set R/W + Present
	mov dword [ebx], eax				; Address of master directory page (val=0x00121003)
	
	;
	; Enable Paging
	;
	mov eax, cr0
	or eax, 0x80000000			; Set top bit of CR0 to enable paging
	mov cr0, eax
	
	ret
	
;
; Init GDT
;  Creates a new bigger GDT so we can fit all the tasks and stuff in it
;
InitGDT:
	; Init to all zeros
	xor eax, eax
	mov edi, [KPTR_GDT]		; Locate GDT at a fixed address
	mov ecx, 0x4000			; 64KB => 0x10000 bytes / 4 => number of dwords
	rep stosd				; Clear memory at new location

	; Copy GDT template to its new home
	mov esi, gdt_start		; Set source
	mov edi, [KPTR_GDT]		; Set dest
	mov ecx, 24				; 24 DWORDs
	rep movsd				; Move DS:ESI to ES:EDI for ECX dwords

	lgdt [KPTR_GDT_Length]		; Load the address of the new GDT
								; Pointer to a WORD:DWORD pair for length:address

	; NB. The segment registers will need reloading here if the DATA
	;     segment setup in the boot loader is not present in the new GDT!

	ret


;
; Init 8259 (PIC)
;  Maps IRQ0-7  to interrupts 0x20-0x27
;       IRQ8-15 to interrupts 0x28-0x2F
; Interrupts 0x00-0x1F are system interrupts, 0x00-0x14 are currently used exceptions
;  the higher ones are reserved.
;
InitPIC:
	mov al, 0x11		; ICW1 - Init command + ICW4 is required
	out 0x20, al		; PIC1 init
	mov al, 0x20		; ICW2 - Set vector table offset
	out 0x21, al		; Start IRQ0 at interrupt 0x20
	mov al, 0x04		; ICW3 - IRQ2 is connected to slave
	out 0x21, al		; IRQ2 connect to slave
	mov al, 0x01		; ICW4 - 8086 mode
	out 0x21, al		; 80x86, flags

	mov al, 0x11		; ICW1 - Init command + ICW4 is required
	out 0xA0, al		; PIC2 init
	mov al, 0x28		; ICW2 - Set vector table offset
	out 0xA1, al		; Start IRQ8 at interrupt 0x28
	mov al, 0x02		; ICW3 - Slave 2
	out 0xA1, al		; 
	mov al, 0x01		; ICW4 - 8086 mode
	out 0xA1, al		; 80x86, flags

	mov al, 0xFF		; All interrupts off while we setup
	out 0x21, al
	mov al, 0xFF
	out 0xA1, al

	ret


;
; InitPIT (8253/2854)
;	Sets up the timer to fire IRQ0 every 50ms or so.
;
InitPIT:
	mov al, 00110100b		; channel 0, lobyte/hibyte, rate generator
	out 0x43, al			; send to command write port
	
;	mov ax, 0xFFFF			; set rate to 65535 = 54.9254ms
	mov ax, 0x1000			; set rate to  4096 =  0.2441ms
	out 0x40, al			; send lowbyte to Channel 0 data port
	mov al, ah
	out 0x40, al			; send hibyte to Channel 0 data port
	
	ret
	
	
;
; InitIDT
;
InitIDT:
	; Clear IDT table
	xor eax, eax
	mov ecx, 0x200		; 256 entries, 8 bytes per entry = 0x200 DWORDs
	mov edi, [KPTR_IDT]
	rep stosd
	
	;
	; Now populate EXCEPTION entries
	;
	mov ebx, [KPTR_IDT]					; Set address of first IDT entry
	mov edx, IDT_Exception_Table		; Get address of jump table
	mov ecx, 20							; 20 exceptions to set
.nextEXP:
	mov eax, [edx]						; Load address of current jump pointer
	call SetIRQHandler					; Configure IDT entry
	add ebx, 8							; Move EBX 8 bytes (IDT entry)
	add edx, 4							; Move EDX 4 bytes (Jump pointer)
	loop .nextEXP						; Loop for ECX items
	
	;
	; Now populate INTERRUPT entries
	;
	mov ebx, [KPTR_IDT]					; Set address of first IDT entry
	add ebx, 0x100						; Goto first IRQ entry
	mov edx, IDT_Interrupt_Table		; Get address of jump table
	mov ecx, 16							; 16 interrupts to set
.nextINT:
	mov eax, [edx]						; Load address of current jump pointer
	call SetIRQHandler					; Configure IDT entry
	add ebx, 8							; Move EBX 8 bytes (IDT entry)
	add edx, 4							; Move EDX 4 bytes (Jump pointer)
	loop .nextINT						; Loop for ECX items
	
	;
	; IRQ0
	; Attach timer to scheduler task gate
	;
	mov ebx, [KPTR_IDT]			; Base addres of IDT
	add ebx, 0x100				; Address of IRQ0 IDT entry
	mov word [ebx+0], 0x0000	; Unused
	mov word ax, [KPTR_KernelTSSDesc]	; Get kernel TSS descriptor
	mov word [ebx+2], ax		; Store TSS Descriptor
	mov word [ebx+4], 0x8500	; Present, DPL=0, Task Gate
	mov word [ebx+6], 0x0000	; Unused

	; System call handler
	mov ebx, 0x00120200			; Address of System Call entry
	mov eax, SystemCallStub		; Load routine address
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax, 16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0xEE00	; Present, DPL=3, 32bits
	
	;
	; Set length/pointer values and load IDT
	;
	mov ebx, KPTR_IDT_Length		; Set length/pointer address
	mov word [ebx], 1024			; 1024 bytes long
	mov eax, [KPTR_IDT]
	mov [ebx+2], eax				; IDT base at 0x00120000
	lidt [KPTR_IDT_Length]

	ret


;
; SetIRQHandler
;  EAX - Address of handler function
;  EBX - Address of IDT entry to populate
;
SetIRQHandler:
	mov [ebx], ax				; Move bits 0-15 of eax to first byte of idt entry
	shr eax, 16
	mov [ebx+6], ax				; Move bits 16-31 of eax to fourth byte of idt entry
	mov word [ebx+2], 0x0008	; Kernel CODE segment
	mov word [ebx+4], 0x8E00	; Present, DPL=0, 32bits
	ret


;
; System Call Handler
;	
SystemCallStub:
	push edx
	push ecx
	push ebx
	push eax
	sti			; Need interrupts back on (required?)
	call _sys_SystemCallHandler
	pop ebx		; pop two values to ebx
	pop ebx		; so we leave eax alone
	pop ecx
	pop edx
	iret
		

;
;  Allocates the first TSS with all the right bits for the kernel
;  system process (process 0) and sets it running.
;  This identifies the currently running code with the TSS.
;  TSS + 8K IO Bitmap = 0x2068 bytes, plus a few extra to deal with overruns
;   when reading from the IO Bitmap.
;
; TODO: Do we need the IO Bitmap?
;
StartKernelTask:
	mov edi, [KPTR_KernelTSS]	; Locate TSS at a fixed address

	; Init to all zeros
	xor eax, eax
	mov ecx, 0x34				; 0x68 bytes / 2 => number of words
	rep stosw

	mov edi, [KPTR_KernelTSS]				; Get address of kernel TSS
	mov word [edi+0x66], 0x68				; Set offset of IO Bitmap from start of TSS
	mov eax, [KPTR_Paging_MasterTable]		; Get address of paging master directory
	mov dword [edi+0x1C], eax				; Set CR3
	
	; Load the task register with the kernel TSS selector from the new GDT.
	; This initialises the current task, but does not cause a task switch.
	mov word ax, [KPTR_KernelTSSDesc]		; TSS Selector (index 5 + TI(0), RPL(0))
	ltr ax	

	ret
	
	
;====================================================================
;
; INTERRUPT HANDLER FUNCTIONS
;
;====================================================================

;
; IRQ0 Handler (Normally overridden by task switch)
;
Interrupt0:
	pusha
	push 0
	jmp CallIRQHandler
	
;
; IRQ1 Handler
;
Interrupt1:
	pusha
	push 1
	jmp CallIRQHandler

;
; IRQ2 Handler
;
Interrupt2:
	pusha
	push 2
	jmp CallIRQHandler
	
;
; IRQ3 Handler
;
Interrupt3:
	pusha
	push 3
	jmp CallIRQHandler
	
;
; IRQ4 Handler
;
Interrupt4:
	pusha
	push 4
	jmp CallIRQHandler
	
;
; IRQ5 Handler
;
Interrupt5:
	pusha
	push 5
	jmp CallIRQHandler

;
; IRQ6 Handler
;
Interrupt6:
	pusha
	push 6
	jmp CallIRQHandler

;
; IRQ7 Handler
;
Interrupt7:
	pusha
	push 7
	jmp CallIRQHandler

;
; IRQ8 Handler
;
Interrupt8:
	pusha
	push 8
	jmp CallIRQHandler

;
; IRQ9 Handler
;
Interrupt9:
	pusha
	push 9
	jmp CallIRQHandler
	
	
;
; IRQ10 Handler
;
Interrupt10:
	pusha
	push 10
	jmp CallIRQHandler
	
	
;
; IRQ11 Handler
;
Interrupt11:
	pusha
	push 11
	jmp CallIRQHandler
	
	
;
; IRQ12 Handler
;
Interrupt12:
	pusha
	push 12
	jmp CallIRQHandler
	
	
;
; IRQ13 Handler
;
Interrupt13:
	pusha
	push 13
	jmp CallIRQHandler
	
	
;
; IRQ14 Handler
;
Interrupt14:
	pusha
	push 14
	jmp CallIRQHandler
	
	
;
; IRQ15 Handler
;
Interrupt15:
	pusha
	push 15
	jmp CallIRQHandler
	
	
;
; CallIRQHandler
;   One of the specific IRQ handler should be call before this one.
;
CallIRQHandler:
	call _irq_GenericIRQHandler
	pop eax

	; Send a non-specific EOI
	mov al, 0x20
	out 0x20, al
	mov al, 0x20
	out 0xA0, al

	popa
	iret



;
; Default Exception Handler
;
DefaultException:
	cli
	call _exp_DefaultHandler
	hlt

;
; Invalid Opcode Exception Handler
;
InvalidOpCodeException:
	cli
	call _exp_InvalidOpCodeHandler
	hlt

;
; Invalid Task Exception Handler
;
InvalidTaskException:
	cli
	call _exp_InvalidTaskHandler
	hlt
	
;
; GPF Exception Handler - Exception 13
;
GPFException:
	cli
	call _exp_GPFHandler
	hlt
	
	
;
; Page Fault Exception Handler - Exception 14
;
PageFaultException:
	cli
	push eax
	mov eax, cr2
	push eax
	call _exp_PageFaultHandler
	pop eax
	pop eax
	sti
	iret				; Return and try to execute again
	
		
;====================================================================
;
; DATA AREA
;
;====================================================================
	
;
; GDT
;   Temporary location of data the will be copied into the new
;    GDT when it is created
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

	dw 0xFFFF	; CODE selector, Base=0000, Len=4GB
	dw 0x0000	; Base
	dw 0xFA00	; Present, DPL=3, DT=App, CODE, Execute/Read, Non-conforming
	dw 0x00CF	; G=4K, D=32bit, Seg high nibble = 0xF

	dw 0xFFFF	; DATA selector, Base=0000, Len=4GB
	dw 0x0000	; Base
	dw 0xF200	; Present, DPL=3, DT=App, DATA, Read/Write, Expand-up
	dw 0x00CF	; G=4K, D=32bit, Seg high nibble = 0xF

	dw 0x0068	; SYSTEM TSS 0x0068 bytes long
	dw 0x3000	; Base address = 0x123000
	dw 0x8912	; Present, DPL=0, System=0 (True), X=32bit, Busy=0 (10001001) + 3rd byte of address
	dw 0x0000	; G=1byte, AVL=0, + upper nibble of size + more address (00000000) 
gdt_end:
	
;
; IDT Jump Table
;
IDT_Exception_Table:
	dd DefaultException			; Exception 0 - Divide Error  (offset 0x000 into IDT)
	dd DefaultException			; Exception 1 - Debug
	dd DefaultException			; Exception 2 - NMI
	dd DefaultException			; Exception 3 - Breakpoint (INT 3)
	dd DefaultException			; Exception 4 - Overflow (INTO)
	dd DefaultException			; Exception 5 - Bound Exceeded (BOUND)
	dd 0	; Exception 6 - Invalid Opcode
	dd DefaultException			; Exception 7 - Device Not Available
	dd DefaultException			; Exception 8 - Double Fault
	dd DefaultException			; Exception 9 - CoProcessor Segment Overrun
	dd InvalidTaskException		; Exception 10 - Invalid TSS
	dd DefaultException			; Exception 11 - Segment Not Present
	dd DefaultException			; Exception 12 - Stack Segment Fault
	dd GPFException				; Exception 13 - GPF
	dd PageFaultException		; Exception 14 - Page Fault
	dd DefaultException			; Exception 15 - Reserved
	dd DefaultException			; Exception 16 - Floating Point Error
	dd DefaultException			; Exception 17 - Alignment Check
	dd DefaultException			; Exception 18 - Machine Check
	dd DefaultException			; Exception 19 - SIMD Floating Point

IDT_Interrupt_Table:
	dd Interrupt0		; IRQ0 - PIT (offset 0x100 into IDT)
	dd Interrupt1		; IRQ1 - Keyboard
	dd Interrupt2		; IRQ2 - Cascade (=>IRQ9)
	dd Interrupt3		; IRQ3 - Serial (COM2)
	dd Interrupt4		; IRQ4 - Serial (COM1)
	dd Interrupt5		; IRQ5 - Parallel (LPT2)
	dd Interrupt6		; IRQ6 - Floppy
	dd Interrupt7		; IRQ7 - Parallel (LPT1)
	dd Interrupt8		; IRQ8 - RTC
	dd Interrupt9		; IRQ9
	dd Interrupt10		; IRQ10 - NE2000
	dd Interrupt11		; IRQ11 - USB
	dd Interrupt12		; IRQ12 - Mouse
	dd Interrupt13		; IRQ13 - FPU
	dd Interrupt14		; IRQ14 - ATA (Primary)
	dd Interrupt15		; IRQ15 - ATA (Secondary)


;
; MEMORY MAP (physical)
;
; 0010 0000  STACK (64k)
; 0011 0000  GDT 8192 entries (64k)
; 0012 0000  IDT 256 entries (4k)
; 0012 1000  Paging Master Directory (4k)
; 0012 2000  Paging First Table (4k) - maps bottom 4MB for kernel
; 0012 3000  Kernel TSS (12k)
; 0012 6000  Virtual Paging Directory (4k)
; 0012 7000  New Process working area (Page Tables)
; 0012 8000  New Process working area (Code Pages)
; 0012 9000  Physical Page Bitmap
; 0014 9000  Kernel Heap 
;

		
;
; Kernel Pointer Table
;
_KPTR_PointerTableStart:
KPTR_KernelStack:
	dd 0x00110000		; Defines 64KB kernel stack 0x00110000->0x00100000

KPTR_Padding1:
	dw 0x0000			; Keep things DWORD aligned

KPTR_GDT_Length:		
	dw gdt_end - gdt_start		; Length of GDT (max 8192 entries) (0x0030 = 48 bytes of GDT)

KPTR_GDT:
	dd 0x00110000		; GDT - 8192 entries (64KB)
						; NB: **MUST** be directly after KPTR_GDT_Length
						;  for LGDT instruction to work correctly

KPTR_Padding2:
	dw 0x0000			; Keep things DWORD aligned

KPTR_IDT_Length:
	dw 0x0000			; IDT Length	
	
KPTR_IDT:
	dd 0x00120000		; IDT - 256 entries (4KB)
	
KPTR_Paging_MasterTable:
	dd 0x00121000		; Paging master directory (4KB)
	
KPTR_Paging_FirstTable:
	dd 0x00122000		; Paging table for first master entry (4KB)
						; Will be 1 to 1 mapped for the kernel memory space
						;  up until the start of kernel heap
KPTR_KernelTSS:
	dd 0x00123000		; Kernel TSS 	
	
KPTR_KernelTSSDesc:
	dw 0x0028			; Kernel TSS Descriptor	

KPTR_Padding3:
	dw 0x0000			; Keep things DWORD aligned
						
KPTR_PhysicalPageBitmap:
	dd 0x00129000		; Start of bitmap of physical pages
	
KPTR_KernelHeapStart:
	dd 0x00149000		; Start of kernel heap (***MUST*** be 4KB aligned)

KPTR_Memory_Map:
	dd 0x00006000		; Address of memory map (populated in boot code)
