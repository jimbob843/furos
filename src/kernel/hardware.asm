;
; Furos v2.0  -  Copyright Marlet Limited 2012
;
; File:				hardware.asm
; Description:		Assembly hardware access functions
; Author:			James Smith
; Created:			21-Aug-2012
;


[BITS 32]			; Protected mode
SECTION .text		; Place code in .text section

;
; Declare symbols for export
;
global _INPORT_BYTE
global _OUTPORT_BYTE
global _INPORT_WORD
global _OUTPORT_WORD
global _INPORT_DWORD
global _OUTPORT_DWORD

global _CPUID_AVAIL
global _EXEC_CPUID
global _READ_MSR
global _WRITE_MSR

global _ENABLE_IRQ
global _DISABLE_IRQ

global _IDLE_LOOP
global _CALL_TSS
global _ADD_TASK
global _END_OF_INT
global _CLEAR_BUSY_BIT
global _SET_BUSY_BIT
global _SET_SCHEDULER_BACKLINK

extern KPTR_GDT_Length
extern KPTR_GDT
extern KPTR_KernelTSS

;
; INPORT_BYTE
;  [input]  ESP+4 = port
;  [output] EAX   = value
;
_INPORT_BYTE:
	push edx
	xor eax, eax			; Clear EAX
	xor edx, edx			; Clear EDX

	mov edx, [esp+8]		; Get port from input parameter
	in al, dx				; Get value from port and put in AL

	pop edx
	ret						; Result returned in EAX

;
; OUTPORT_BYTE
;  [input] ESP+4 = port
;  [input] ESP+8 = value
;
_OUTPORT_BYTE:
	push eax
	push edx
	
	mov edx, [esp+12]		; Get the port
	mov eax, [esp+16]		; Get the value
	out dx, al				; Send the value to the port
	
	pop edx
	pop eax
	ret
	
;
; INPORT_WORD
;  [input]  ESP+4 = port
;  [output] EAX   = value
;
_INPORT_WORD:
	push edx
	xor eax, eax			; Clear EAX
	xor edx, edx			; Clear EDX

	mov edx, [esp+8]		; Get port from input parameter
	in ax, dx				; Get value from port and put in AX

	pop edx
	ret						; Result returned in EAX

;
; OUTPORT_WORD
;  [input] ESP+4 = port
;  [input] ESP+8 = value
;
_OUTPORT_WORD:
	push eax
	push edx
	
	mov edx, [esp+12]		; Get the port
	mov eax, [esp+16]		; Get the value
	out dx, ax				; Send the value to the port
	
	pop edx
	pop eax
	ret
	
	
;
; INPORT_DWORD
;  [input]  ESP+4 = port
;  [output] EAX   = value
;
_INPORT_DWORD:
	push edx
	xor eax, eax			; Clear EAX
	xor edx, edx			; Clear EDX

	mov edx, [esp+8]		; Get port from input parameter
	in eax, dx				; Get value from port and put in EAX

	pop edx
	ret						; Result returned in EAX

;
; OUTPORT_DWORD
;  [input] ESP+4 = port
;  [input] ESP+8 = value
;
_OUTPORT_DWORD:
	push eax
	push edx
	
	mov edx, [esp+12]		; Get the port
	mov eax, [esp+16]		; Get the value
	out dx, eax				; Send the value to the port
	
	pop edx
	pop eax
	ret
	

;
; CPUID_AVAIL
;  returns 1 if CPUID available
;
_CPUID_AVAIL:
	push ecx
	pushfd
	pop eax		; Load eflags into eax

	mov ecx, eax		; Save eflags for later
	xor eax, 0x200000	; Flip CPUID bit
	push eax
	popfd				; Load eflags from stack

	pushfd
	pop eax				; Load eflags back into eax

	xor eax, ecx		; mask changed bits
	shr eax, 21			; move bit 21 to bit 0
	and eax, 1			; and mask others

	pop ecx
	ret

;
; EXEC_CPUID
; [input] ESP+4  = CPUID command
;         ESP+8  = int *eax
;         ESP+12 = int *ebx
;         ESP+16 = int *ecx
;         ESP+20 = int *edx
;
_EXEC_CPUID:
	push eax
	push ebx
	push ecx
	push edx
	push esi
	
	mov eax, [esp+24]	; Get CPUID command
	cpuid
	
	mov esi, [esp+28]	; Get address of eax output
	mov [esi], eax
	mov esi, [esp+32]	; Get address of ebx output
	mov [esi], ebx
	mov esi, [esp+36]	; Get address of ecx output
	mov [esi], ecx
	mov esi, [esp+40]	; Get address of edx output
	mov [esi], edx
	
	pop esi
	pop edx
	pop ecx
	pop ebx
	pop eax
	
	ret
	
;
; READ_MSR
; [input] ESP+4  = MSR register
;         ESP+8  = int *edx
;         ESP+12 = int *eax
;
_READ_MSR:
	push eax
	push ecx
	push edx
	push esi
	
	mov ecx, [esp+20]	; Get MSR register

	rdmsr				; READ MSR
	
	mov esi, [esp+24]	; Get address of edx output
	mov [esi], edx
	mov esi, [esp+28]	; Get address of eax output
	mov [esi], eax
	
	pop esi
	pop edx
	pop ecx
	pop eax
	ret
	
;
; WRITE_MSR
; [input] ESP+4  = MSR register
;         ESP+8  = int edx
;         ESP+12 = int eax
;
_WRITE_MSR:
	push eax
	push ecx
	push edx
	
	mov ecx, [esp+16]	; Get MSR register
	mov edx, [esp+20]	; Get hiword
	mov eax, [esp+24]   ; Get loword

	wrmsr				; WRITE MSR
	
	pop edx
	pop ecx
	pop eax
	ret
	
	

;====================================================================
;
; INTERRUPT FUNCTIONS
;
;====================================================================

;
; Enable a specific interrupt at the PIC
;  [input] ESP+4 = irq
;
_ENABLE_IRQ:
	cli
	push eax
	push ebx
	mov ebx, [esp+12]	; Get the irq number (0-15)
	
	; Get the current mask setting
	in al, 0xA1
	shl eax, 8
	in al, 0x21
	
	btr eax, ebx		; Clear the bit in the mask
	
	; Put the mask back
	out 0x21, al
	shr eax, 8
	out 0xA1, al
	
	pop ebx
	pop eax
	sti
	ret

;
; Disable a specific interrupt at the PIC
;  [input] ESP+4 = irq
;
_DISABLE_IRQ:
	cli
	push eax
	push ebx
	
	xor eax, eax
	mov ebx, [esp+12]	; Get the irq number (0-15)
	
	; Get the current mask setting
	in al, 0xA1			; Get high byte
	shl eax, 8
	in al, 0x21			; Get low byte
	
	bts eax, ebx		; Set the bit in the mask
	
	; Put the mask back
	out 0x21, al		; Set low byte
	shr eax, 8
	out 0xA1, al		; Set high byte
	
	pop ebx
	pop eax
	sti
	ret


;====================================================================
;
; TASK SCHEDULING FUNCTIONS
;
;====================================================================

idlecount:
	db 0

;
; IDLE_LOOP
;
_IDLE_LOOP:
	mov al, [idlecount]
	inc al
	mov [idlecount], al
	mov byte [0xb8000], al 
	mov byte [0xb8001], 0x07
	hlt
	jmp _IDLE_LOOP
	
	
;
; Switch to a task by TSS selector
;  just performs a far call to the selector placed on the stack
;
_CALL_TSS:
	push eax
	mov eax, [esp+8]
	mov word [taskptr_selector], ax
	pop eax

	cli
	in al, 0x21
	and al, 0xFE		; Enable IRQ0
	out 0x21, al
	sti
	
	jmp far [taskptr_offset]
	ret
	

;
; Add a new task to the GDT
;  ESP+4  = Address of new TSS
;
_ADD_TASK:
	mov eax, [esp+4]	; Get the TSS base address
	push ebx
	push ecx
	push edx
	
	xor ebx, ebx
	xor ecx, ecx
	xor edx, edx
	
	; Update the GDT values
	mov word bx, [KPTR_GDT_Length]	; Get length of GDT = new descriptor
	mov dx, bx					; Save for return value
	add ebx, [KPTR_GDT]			; Add the start address
	
	mov cx, dx					; Now work out the new length
	add ecx, 0x08				; Add length of new entry
	mov word [KPTR_GDT_Length], cx		; Write back the new value
	
	; Initialise the contents of the new entry
	mov word [ebx+0], 0x0098	; 0x0098 bytes long  (0x30 of OS specific)
	mov word [ebx+2], ax		; Bottom two bytes of address
	shr eax, 16					; Move to next bytes of address
	mov byte [ebx+4], al		; byte 3 of address
	mov byte [ebx+5], 0x89		; Present, DPL=0, System=0 (True), X=32bit, Busy=0 (11101001)
	mov byte [ebx+6], 0x00		; G=1byte, AVL=0
	mov byte [ebx+7], ah		; byte 4 (MSB) of address
	
	lgdt [KPTR_GDT_Length]		; Update the GDT registers
	
	mov eax, edx				; Set the return value (TSS desc)
	pop edx
	pop ecx
	pop ebx
	
	ret	


;
; Used at the end of the scheduler
;
_END_OF_INT:
	cli				; 
	push eax
	mov al, 0x60	; generate end of interrupt (IRQ0 Specific)
	out 0x20, al
	pop eax

	iret            ; Peform the task switch
					
	cli				; Prevent further interrupts interfering with the scheduler
	ret				; Return to the main loop when we get back here!

	
;
; Raises an interrupt to switch to the scheduler
;
_RAISE_INT0:
	int 0x20
	ret

;
; Clears the busy bit of a process
;  ESP+4 = TSS Descriptor of old task
;
_CLEAR_BUSY_BIT:
	push eax
	push ebx
	mov eax, [esp+12]				; Move the TSS descriptor into AX
	mov ebx, [KPTR_GDT]				; Get address of GDT
	add ebx, eax					; Calculate start of TSS entry
	mov byte al, [ebx+5]
	and al, 11111101b				; Clear the busy bit (bit1)
	mov byte [ebx+5], al
	pop ebx
	pop eax
	ret
	
;
; Sets the busy bit of a process
;  ESP+4 = TSS Descriptor of task
;
_SET_BUSY_BIT:
	push eax
	push ebx
	mov eax, [esp+12]				; Move the TSS descriptor into AX
	mov ebx, [KPTR_GDT]				; Get address of GDT
	add ebx, eax					; Calculate start of TSS entry
	mov byte al, [ebx+5]
	or al, 00000010b				; Set the busy bit (bit1)
	mov byte [ebx+5], al
	pop ebx
	pop eax
	ret

	
;
; Set the backlink of the scheduler task, so we perform a task switch
;  when we IRET
;  ESP+4 = TSS Descriptor of new task
_SET_SCHEDULER_BACKLINK:
	push eax
	push ebx
	mov eax, [esp+12]
	mov ebx, [KPTR_KernelTSS]
	mov word [ebx], ax		; Fixed Kernel TSS location
	pop eax
	ret


	
;
; Task pointer for switching tasks
;
taskptr_offset:
	dd 0	; offset
taskptr_selector:
	dw 0	; selector

	
