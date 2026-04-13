/*
 * ipc_shared.c
 *
 *  Created on: Jan 22, 2026
 *      Author: GRL
 */


#include "ipc_shared.h"

volatile uint8_t cm4_reply_ready = 0;

/**
 * @brief Shared memory packet from M7 core to M4 core
 * 
 * This volatile variable is placed in the shared RAM section with 32-byte alignment
 * to facilitate inter-processor communication (IPC) between the M7 and M4 cores.
 * Initialized to zero on startup.
 * 
 * @note Must maintain 32-byte alignment for optimal DMA or cache performance
 * @note Access should be protected with appropriate synchronization mechanisms
 */

__attribute__((section(".shared_ram"), aligned(32)))
volatile shm_packet_t SHM_M7_TO_M4;

/**
 * @brief Shared memory packet from M4 core to M7 core
 * 
 * This volatile variable is placed in the shared RAM section with 32-byte alignment
 * to facilitate inter-processor communication (IPC) between the M4 and M7 cores.
 * Initialized to zero on startup.
 * 
 * @note Must maintain 32-byte alignment for optimal DMA or cache performance
 * @note Access should be protected with appropriate synchronization mechanisms
 */
__attribute__((section(".shared_ram"), aligned(32)))
volatile shm_packet_t SHM_M4_TO_M7;

void ipc_shm_init(void)
{
    memset((void*)&SHM_M7_TO_M4, 0, sizeof(SHM_M7_TO_M4));
    memset((void*)&SHM_M4_TO_M7, 0, sizeof(SHM_M4_TO_M7));
    __DMB();
}

/**
 * @brief Callback function invoked when a hardware semaphore is freed.
 * 
 * This function is called by the HAL when a semaphore matching the provided mask
 * is released. It handles inter-processor communication (IPC) between CM4 and CM7 cores.
 * 
 * @param[in] SemMask Bitmask identifying which semaphore(s) were freed.
 *                    Used to determine if this is a CM4-to-CM7 semaphore event.
 * 
 * @return void
 * 
 * @details
 * - Sets the cm4_reply_ready flag when the CM4_TO_CM7 semaphore is released
 * - Re-arms the semaphore notification for subsequent events
 * - Signals the ApiEventFlags event group to wake waiting threads via ThreadX RTOS
 * 
 * @note This is a HAL callback and should not be called directly by user code.
 *       Requires ThreadX RTOS and hardware semaphore (HSEM) peripheral initialization.
 * 
 * @see HAL_HSEM_ActivateNotification()
 * @see tx_event_flags_set()
 */
void HAL_HSEM_FreeCallback(uint32_t SemMask)
{
    if (SemMask & __HAL_HSEM_SEMID_TO_MASK(HSEM_CM4_TO_CM7))
    {
        cm4_reply_ready = 1;

        /* Re-arm if your HAL behaves one-shot */
        HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(HSEM_CM4_TO_CM7));

        /* If you still want to wake your thread: */
        tx_event_flags_set(&ApiEventFlags, CM4_INTR, TX_OR);
    }
}

/**
 * @brief Attempts to acquire a hardware semaphore with a timeout mechanism.
 * 
 * This function tries to take ownership of a hardware semaphore within a specified
 * time limit. It continuously attempts to acquire the semaphore until either the
 * semaphore is successfully taken or the timeout period expires.
 * 
 * @param sem_id The hardware semaphore ID to acquire (0-31 for STM32H755)
 * @param timeout_ms The timeout period in milliseconds. If the semaphore cannot be
 *                   acquired within this time, the function returns with an error.
 * 
 * @return 0 if the hardware semaphore was successfully acquired
 * @return -1 if the timeout period expired before acquiring the semaphore
 * 
 * @note This function uses a polling mechanism and will block until either the
 *       semaphore is acquired or timeout occurs. Consider the impact on system
 *       responsiveness for long timeout values.
 * @note HAL_GetTick() must be initialized before calling this function.
 * 
 * @see HAL_HSEM_FastTake()
 * @see HAL_GetTick()
 */
int hsem_take_with_timeout(uint32_t sem_id, uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    while (HAL_HSEM_FastTake(sem_id) != HAL_OK) {
        if ((HAL_GetTick() - t0) > timeout_ms) return -1;
    }
    return 0;
}

/**
 * @brief Sends data from CM7 to CM4 core via shared memory with synchronization.
 * 
 * This function transfers a buffer of data to the CM4 core using a hardware semaphore
 * for mutual exclusion and a shared memory region. The function ensures cache coherency
 * by cleaning the data cache before signaling the CM4 via doorbell interrupt.
 * 
 * @param buf       Pointer to the data buffer to send. Must not be NULL.
 * @param len       Length of data in bytes. Must be greater than 0 and not exceed API_MAX_LEN.
 * 
 * @return void
 * 
 * @note This function performs the following operations:
 *       1. Validates input length
 *       2. Acquires hardware semaphore (HSEM_CM7_TO_CM4) with 10ms timeout
 *       3. Copies data to shared memory region SHM_M7_TO_M4
 *       4. Performs memory barriers and cache cleaning for coherency
 *       5. Releases semaphore to signal CM4 core
 * 
 * @warning If the hardware semaphore cannot be acquired within the timeout period,
 *          the function returns without sending data. Calling code should handle
 *          potential message loss.
 * 
 * @see hsem_take_with_timeout()
 * @see HAL_HSEM_Release()
 * @see SCB_CleanDCache_by_Addr()
 */
void cm7_send_to_cm4(const uint8_t *buf, uint16_t len)
{
    if (len == 0 || len > API_MAX_LEN) return;

    if (hsem_take_with_timeout(HSEM_CM7_TO_CM4, 10) != 0) {
        printf("HSEM take timeout CM7->CM4\n");
        return;
    }

    SHM_M7_TO_M4.len = len;
    memcpy((void*)SHM_M7_TO_M4.data, buf, len);

    __DMB();
    SCB_CleanDCache_by_Addr((uint32_t*)&SHM_M7_TO_M4,
                            cache_align_size(sizeof(SHM_M7_TO_M4)));
    __DMB();

    HAL_HSEM_Release(HSEM_CM7_TO_CM4, 0);   // doorbell CM4
}
