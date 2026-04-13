/*
 * grl_tim.c
 *
 *  Created on: Feb 5, 2026
 *      Author: GRL
 */
#include "tx_api.h"
#include "stdint.h"
#include "qi_tim.h"

static TX_TIMER GRL_MSG_TIM_1;
volatile uint8_t streamer_stop;

UINT msg_timer_cfg(TX_TIMER *TimerBase, uint8_t aTim_IDx, uint32_t period_ms);
/* Convert milliseconds → ThreadX ticks (rounded up, min 1 tick)
 * Works for any TX_TIMER_TICKS_PER_SECOND value.
 */
static ULONG ms_to_ticks(uint32_t ms)
{
    if (ms == 0u) ms = 1u;

    // round up: ticks = ceil(ms * TPS / 1000)
    uint64_t ticks = ((uint64_t)ms * (uint64_t)TX_TIMER_TICKS_PER_SECOND + 999ull) / 1000ull;
    return (ticks == 0u) ? 1u : (ULONG)ticks;
}

void msg_tim_init(){
	msg_timer_cfg(&GRL_MSG_TIM_1, 1, 2);
	msg_timer_start(&GRL_MSG_TIM_1);
}

/**
 * @brief Callback function for message timer 1
 * @details Fetches rectangle V-I data and restarts the timer if streaming is active.
 *          This function is called periodically by the message timer.
 * @param arg Unused argument (required by timer callback signature)
 * @return void
 * @note This callback should not be called directly; it is invoked by the timer mechanism
 */
static void msg_timer1_cb(ULONG arg)
{
    (void)arg;

    if(!streamer_stop)
    	fetch_rect_v_i(6);

	msg_timer_start(&GRL_MSG_TIM_1);

}

UINT msg_timer_cfg(TX_TIMER *TimerBase, uint8_t aTim_IDx, uint32_t period_ms){

	ULONG ticks = ms_to_ticks(period_ms);
	switch(aTim_IDx){
	case 1:
	   return tx_timer_create(TimerBase,
	                    (CHAR *)"GRL-MSG-TIM-1",
	                    msg_timer1_cb,
	                    0,
	                    ticks,          /* initial expiration */
	                    ticks,          /* reschedule period */
						TX_NO_ACTIVATE);

	    break;

	default:
	        return -1;
	}

}

/* Change timer period at runtime */
void msg_timer_set_period(TX_TIMER *TimerBase, uint32_t period_ms)
{
	if (TimerBase == NULL) return TX_PTR_ERROR;

    ULONG ticks = ms_to_ticks(period_ms);

    tx_timer_change(TimerBase,
                    ticks,      /* new initial */
                    ticks);     /* new reschedule */
}

void msg_timer_stop(TX_TIMER *TimerBase)
{
    if (TimerBase == NULL) return TX_PTR_ERROR;
    tx_timer_deactivate(TimerBase);
}

/**
 * @brief Activates a ThreadX timer for message transmission.
 * 
 * @param TimerBase Pointer to the TX_TIMER structure to be activated.
 *                  Must not be NULL.
 * 
 * @return TX_PTR_ERROR if TimerBase is NULL, otherwise the return value
 *         from tx_timer_activate().
 * 
 * @note This function validates the input pointer before attempting to
 *       activate the timer.
 */
void msg_timer_start(TX_TIMER *TimerBase)
{
    if (TimerBase == NULL) return TX_PTR_ERROR;
    tx_timer_activate(TimerBase);
}
