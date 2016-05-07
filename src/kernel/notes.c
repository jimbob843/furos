FPU Init
========
CR0-Emulation = 1    (bit2 ?)
CR0-NumericExp = 0   (bit5 ?)
CR0-MathPresent = 0  (bit1 ?)

struct sFloppyGeometries sFloppyGeometries[ 6 ] = {
	/* N/A*/	{ -1, -1, -1,   -1,  -1,   -1,   -1,   -1, -1 },
	/* 360kb */	{  2, 80,  9, 1440, 512,    0,    0, 0xF6,  2 },
	/* 1.2mb */	{  2, 80, 15, 2400, 512,    0,    0, 0xF6,  2 },
	/* 720kb */	{  2, 80,  9, 1440, 512,    0,    0, 0xF6,  2 },
	/* 1.44mb*/	{  2, 80, 18, 2880, 512, 0x1B, 0x6C, 0xF6,  2 },
	/* 2.88mb*/	{  2, 80, 36, 5760, 512,    0,    0, 0xF6,  2 }
};

struct sFloppyGeometries {
	sint32 nHeads;
	sint32 nTracks;
	sint32 nSectorsPerTrack;
	sint32 nSectors;
	sint32 nBlockSize;
	sint32 nGap3;

	/* format specific stuff */
	sint32 nFmtGap3;
	sint32 nFmtFill;
	sint32 nShiftedBlkSize;
};


     SECTION .sector1 progbits start=0x0000 vstart=0x1000
     SECTION .sector2 progbits start=0x0200 vstart=0x1200
     SECTION .sector3 progbits start=0x0400 vstart=0x1400
     SECTION .text progbits follows=.sector3 vfollows=.sector3
     SECTION .data progbits follows=.text vfollows=.text
     SECTION .common progbits follows=.data vfollows=.data



	 ;Define sections

     SECTION .header progbits start=0x00000000 vstart=0x00000000
     SECTION .text progbits follows=.header vstart=0x00000000
     SECTION .data progbits follows=.text vstart=0x00000000
     SECTION .bss nobits follows=.data vfollows=.data
     SECTION .stack nobits follows=.bss vfollows=.bss

;Set section start labels

     section .text
CODE_START:
     section .data
DATA_START:
     section .bss
BSS_START:
     section .stack
STACK_START:

;Header

     section .header
     db 'PROG' ; file type: program
     dd CODE_END-CODE_START ; code size
     dd DATA_END-DATA_START ; data size
     dd BSS_END-BSS_START ; uninitialized data size
     dd STACK_END-STACK_START ; stack size

     cpu 386
     bits 32

;Code and data goes here (can be in random order, just use "section whatever" where needed)

     section .text
     mov eax,[foo]
.l1: hlt
     jmp .l1

     section .data
foo: dd bar

     section .bss
bar: resd 1


;Set section end labels

     section .text
     align 4
CODE_END:

     section .data
     align 4
DATA_END:

     section .bss
     alignb 4
BSS_END:

     section .stack
     alignb 4
STACK_END:




	      %include "exe_file_format_macro.inc"

     EXE_START 1234           ;Create header ("1234" is stack size)

     section .text
     mov eax,[foo]
.l1: hlt
     jmp .l1

     section .data
foo: dd bar

     section .bss
bar: resd 1

     EXE_END                   ;Macro to set the end labels




	 CFLAGS   =-ffreestanding -masm=intel -std=c99 -O2 -fPIC -mno-red-zone \
          -mno-mmx -mno-sse -mno-sse2 -mno-sse3 -mno-3dnow -c -D_$(ARCH) $(INCLUDE) -Werror $(CWARN)

-mcmodel=large:
  mov rdx, offset64
  add rdi, [rdx]
  mov rax, position64
  jmp rax

-mcmodel=small
  add rdi, [offset32]
  jmp [position32]

-mcmodel=kernel (or DEFAULT REL in nasm)
  works like small, but use the fact that address is sign-extended

-fPIC
  in x86_64, generated code uses RIP relative addressing.

