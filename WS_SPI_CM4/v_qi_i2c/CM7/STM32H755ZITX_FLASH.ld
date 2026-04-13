/*
******************************************************************************
**
**  File        : LinkerScript.ld
**
**  Author      : STM32CubeIDE
**
**  Abstract    : Linker script for STM32H7 series
**                      1024Kbytes FLASH
**                      800Kbytes RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used.
**
**  Target      : STMicroelectronics STM32
**
**  Distribution: The file is distributed as is without any warranty
**                of any kind.
**
*****************************************************************************
** @attention
**
** Copyright (c) 2025 STMicroelectronics.
** All rights reserved.
**
** This software is licensed under terms that can be found in the LICENSE file
** in the root directory of this software component.
** If no LICENSE file comes with this software, it is provided AS-IS.
**
*****************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(RAM_D1) + LENGTH(RAM_D1); /* end of "RAM_D1" Ram type memory */

_Min_Heap_Size = 0x200; /* required amount of heap  */
_Min_Stack_Size = 0x400; /* required amount of stack */

/* Memories definition */
MEMORY
{
  RAM_D1 (xrw)   : ORIGIN = 0x24000000, LENGTH =  512K
  FLASH   (rx)   : ORIGIN = 0x08000000, LENGTH = 640K    /* Memory is divided. Actual start is 0x08000000 and actual length is 2048K */
  DTCMRAM (xrw)  : ORIGIN = 0x20000000, LENGTH = 128K

    /* Reserve last 8KB of D2 for IPC */
	/*RAM_D2 (xrw)      : ORIGIN = 0x30000000, LENGTH = 240K*/
	/*RAM_CM4 (xrw)     : ORIGIN = 0x3003C000, LENGTH = 32K*/   /* CM7 shall not place anything here */
	
	RAM_D2 (xrw)      : ORIGIN = 0x30000000, LENGTH = (288K - 4K)
	RAM_D2_IPC (xrw)  : ORIGIN = 0x30047000, LENGTH = 4K
	
	/*RAM_D2_IPC (xrw)  : ORIGIN = 0x30044000, LENGTH = 16K*/
  
  /*RAM_D3 (xrw)   : ORIGIN = 0x38000000, LENGTH = 64K*/ /*dedicated this section for CM4*/
  ITCMRAM (xrw)  : ORIGIN = 0x00000000, LENGTH = 64K
}

/* Sections */
SECTIONS
{
  /* The startup code into "FLASH" Rom type memory */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >FLASH

  /* The program code and other data into "FLASH" Rom type memory */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH

  /* Constant data into "FLASH" Rom type memory */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >FLASH

  .ARM.extab (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
  } >FLASH
  .ARM (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } >FLASH

  .preinit_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .init_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .fini_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >FLASH

  /* Used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections into "RAM" Ram type memory */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    *(.RamFunc)        /* .RamFunc sections */
    *(.RamFunc*)       /* .RamFunc* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >RAM_D1 AT> FLASH

  /* Uninitialized data section into "RAM" Ram type memory */
  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss section */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM_D1

  /* User_heap_stack section, used to check that there is enough "RAM" Ram  type memory left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM_D1
  
	/* Descriptors: small, NOLOAD, D2 SRAM (e.g., SRAM3 @ 0x30040000, 32 KB) */
	.RxDescripSection (NOLOAD) :
	{
	  . = ALIGN(32);
	  *(.RxDescripSection)
	  . = ALIGN(32);
	} >RAM_D2
	
	.TxDescripSection (NOLOAD) :
	{
	  . = ALIGN(32);
	  *(.TxDescripSection)
	  . = ALIGN(32);
	} >RAM_D2
	
	/* NetX packet pool: larger buffer region in D2 SRAM */
	.NetXPoolSection (NOLOAD) :
	{
	  . = ALIGN(32);
	  *(.NetXPoolSection)
	  . = ALIGN(32);
	} >RAM_D2
	/* UART4 DMA RX buffer (D2 SRAM, NOLOAD, 32-byte aligned) */
  	.Uart4DmaSection (NOLOAD) :
  	{
      . = ALIGN(32);
      *(.Uart4DmaSection)
      . = ALIGN(32);
  	} >RAM_D2

	/* I2C 1 DMA RX buffer (D2 SRAM, NOLOAD, 32-byte aligned) */
  	.i2c1DmaSection (NOLOAD) :
  	{
      . = ALIGN(32);
      *(.i2c1DmaSection)
      . = ALIGN(32);
  	} >RAM_D2
  	
	/* Shared CM7 <-> CM4 packet buffer */
	.shared_ram (NOLOAD) :
	{
	  . = ALIGN(32);
	  *(.shared_ram)
	  . = ALIGN(32);
	} > RAM_D2_IPC
	
  /* Remove information from the compiler libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}

/* ---- IPC safety checks (CM7) ---- */

/* 1) Shared section must fit in IPC window */
ASSERT(SIZEOF(.shared_ram) <= LENGTH(RAM_D2_IPC),
       "ERROR: .shared_ram larger than RAM_D2_IPC region");

/* 2) NetX/ETH pool section must fit in RAM_D2 (if you use it) */
ASSERT(SIZEOF(.NetXPoolSection) <= LENGTH(RAM_D2),
       "ERROR: .NetXPoolSection overflow RAM_D2");

/* 3) ETH descriptor sections must fit in RAM_D2 (if present) */
ASSERT(SIZEOF(.RxDescripSection) <= LENGTH(RAM_D2),
       "ERROR: .RxDescripSection overflow RAM_D2");
ASSERT(SIZEOF(.TxDescripSection) <= LENGTH(RAM_D2),
       "ERROR: .TxDescripSection overflow RAM_D2");

/* 4) Guard: RAM_D2 must end before IPC begins */
ASSERT((ORIGIN(RAM_D2) + LENGTH(RAM_D2)) <= ORIGIN(RAM_D2_IPC),
       "ERROR: RAM_D2 overlaps RAM_D2_IPC (check ORIGIN/LENGTH math)");

ASSERT(SIZEOF(.Uart4DmaSection) <= LENGTH(RAM_D2),
       "ERROR: .Uart4DmaSection overflow RAM_D2");
