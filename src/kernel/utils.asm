;
; Furos v2.0  -  Copyright Marlet Limited 2012
;
; File:				utils.asm
; Description:		Assembly utility functions
; Author:			James Smith
; Created:			18-Aug-2012
;


[BITS 32]			; Protected mode
SECTION .text		; Place code in .text section

;
; Declare symbols for export
;
global _STOP_PROCESSOR
global _HALT_PROCESSOR

global _DISABLE_INTERRUPTS
global _ENABLE_INTERRUPTS

global _INVALIDATE_PAGE
global _MEMCPY
global _MEMCLR

global _SPINLOCK_WAIT
global _SPINLOCK_SIGNAL


;
; STOP_PROCESSOR
;
_STOP_PROCESSOR:
	cli
	hlt
	
;
; HALT_PROCESSOR
;
_HALT_PROCESSOR:
	hlt
	ret
	
;
; DISABLE_INTERRUPTS
;
_DISABLE_INTERRUPTS:
	cli
	ret
	
;
; ENABLE_INTERRUPTS
;
_ENABLE_INTERRUPTS:
	sti
	ret

;
; INVALIDATE_PAGE
;
_INVALIDATE_PAGE:
	push eax
	mov eax, [esp+8]
	invlpg [eax]
	pop eax
	ret
	
;
; MEMCPY
;  [input] ESP+4  = src
;  [input] ESP+8  = dest
;  [input] ESP+12 = bytes
;
_MEMCPY:
	push esi
	push edi
	push ecx
	
	; Copy the main block using DWORDs
	mov esi, [esp+16]		; Load source address
	mov edi, [esp+20]		; Load destination address
	mov ecx, [esp+24]		; Load number of bytes to copy
	shr ecx, 2				; Convert to whole DWORDS
	rep movsd
	
	; Copy any odd bytes lefts using BYTEs
	mov ecx, [esp+24]		; Get the number of bytes again
	and ecx, 0x03			; Mask off bottom 2 bits
	rep movsb
	
	pop ecx
	pop edi
	pop esi
	ret
	
;
; MEMCLR
;  [input] ESP+4  = addr
;  [input] ESP+8  = bytes
;
_MEMCLR:
	push eax
	push edi
	push ecx
	
	; Set the main block using DWORDs
	mov edi, [esp+16]		; Load destination address
	mov ecx, [esp+20]		; Load number of bytes to copy
	xor eax, eax			; Set EAX = 0
	shr ecx, 2				; Convert to whole DWORDS
	rep stosd
	
	; Copy any odd bytes lefts using BYTEs
	mov ecx, [esp+20]		; Get the number of bytes again
	and ecx, 0x03			; Mask off bottom 2 bits
	rep movsb
	
	pop ecx
	pop edi
	pop eax
	ret


;====================================================================
;
; SPINLOCK FUNCTIONS
;
;====================================================================

;
; SPINLOCK_WAIT
;  [input] ESP+4  = addr
;
; Wait for the spinlock to be clear and then sets it
;
_SPINLOCK_WAIT:
	push eax
	mov eax, [esp+8]
.spinwait:
	bts dword [eax], 0
	jc .waittoclear		; Carry set to previous value. Check wasn't already 1
	pop eax
	ret					; We got the lock, return.

.waittoclear:
	; Spinlock is set by another thread. Fire the scheduler.
	int 0x20			; Signal a task switch
	jmp .spinwait


;
; SPINLOCK_SIGNAL
;  [input] ESP+4  = addr
;
; Signals that we've finished with the spinlock
;	
_SPINLOCK_SIGNAL:
	push eax
	mov eax, [esp+8]
	mov dword [eax], 0		; Just clear. Doesn't need to be atomic.
	pop eax
	ret
	

