/*
 * fault.c
 *
 *  Created on: Dec 17, 2025
 *      Author: GRL
 */


#include "stm32h7xx.h"
#include <stdint.h>

// Put a breakpoint here; inspect stacked_* variables in debugger
volatile uint32_t stacked_r0, stacked_r1, stacked_r2, stacked_r3;
volatile uint32_t stacked_r12, stacked_lr, stacked_pc, stacked_psr;
volatile uint32_t scb_cfsr, scb_hfsr, scb_mmf, scb_bfar, scb_afsr;
volatile uint32_t exc_return_lr;
volatile uint32_t sp_at_fault;
volatile uint32_t sp_words[8];

//__attribute__((naked)) void HardFault_Handler(void) {
//
//  __asm volatile(
//    "mov %[ex], lr     \n"   // save EXC_RETURN
//    "tst lr, #4        \n"
//    "ite eq            \n"
//    "mrseq r0, msp     \n"
//    "mrsne r0, psp     \n"
//    "b hardfault_cc     \n"
//    : [ex] "=r"(exc_return_lr)
//  );
//}


void hardfault_cc(uint32_t *sp) {
  sp_at_fault = (uint32_t) sp;
  for (int i = 0; i < 8; i++) sp_words[i] = sp[i];

  stacked_r0  = sp[0];
  stacked_r1  = sp[1];
  stacked_r2  = sp[2];
  stacked_r3  = sp[3];
  stacked_r12 = sp[4];
  stacked_lr  = sp[5];
  stacked_pc  = sp[6];
  stacked_psr = sp[7];

  scb_cfsr = SCB->CFSR;
  scb_hfsr = SCB->HFSR;
  scb_mmf  = SCB->MMFAR;
  scb_bfar = SCB->BFAR;

  __BKPT(0);
  while (1) {}
}

