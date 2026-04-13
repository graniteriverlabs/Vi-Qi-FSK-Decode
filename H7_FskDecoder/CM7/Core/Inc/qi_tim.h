/*
 * grl_tim.h
 *
 *  Created on: Feb 5, 2026
 *      Author: GRL
 */

#ifndef INC_QI_TIM_H_
#define INC_QI_TIM_H_
#include "main.h"

void msg_tim_init();
void msg_timer_start(TX_TIMER *TimerBase);
void msg_timer_stop(TX_TIMER *TimerBase);
void msg_timer_set_period(TX_TIMER *TimerBase, uint32_t period_ms);

#endif /* INC_QI_TIM_H_ */
