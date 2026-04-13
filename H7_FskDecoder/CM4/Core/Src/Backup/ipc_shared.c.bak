/*
 * ipc_shared.c
 *
 *  Created on: Jan 22, 2026
 *      Author: GRL
 */


#include "ipc_shared.h"
#include "string.h"
volatile uint8_t cm4_reply_ready = 0;


/**
 * @brief Shared memory packet buffer for M7 to M4 inter-processor communication
 * 
 * @details This variable is placed in a dedicated shared RAM section with 32-byte
 *          alignment to enable communication between the M7 and M4 cores.
 *          The volatile qualifier ensures the compiler does not optimize access
 *          to this variable.
 * 
 * @note    - Located in ".shared_ram" section for inter-core visibility
 *          - 32-byte alignment optimizes cache line access
 *          - Initialized to zero at startup
 * 
 * @see     shm_packet_t
 */
__attribute__((section(".shared_ram"), aligned(32)))
volatile shm_packet_t SHM_M7_TO_M4;

/**
 * @brief Shared memory packet for inter-processor communication from M4 to M7
 * 
 * @details
 * - Located in shared RAM section for access by both Cortex-M4 and Cortex-M7 cores
 * - 32-byte aligned to optimize cache performance and ensure atomic access
 * - Volatile qualifier prevents compiler optimizations on accesses
 * - Initialized to zero at startup
 * 
 * @note This variable is shared between dual-core processors and must be 
 *       accessed with appropriate synchronization mechanisms to avoid data races
 */
__attribute__((section(".shared_ram"), aligned(32)))
volatile shm_packet_t SHM_M4_TO_M7;

volatile uint8_t packet_ready = 0;

void ipc_shm_init(void)
{
    memset((void*)&SHM_M7_TO_M4, 0, sizeof(SHM_M7_TO_M4));
    memset((void*)&SHM_M4_TO_M7, 0, sizeof(SHM_M4_TO_M7));
    __DMB();
}
/**
 * @brief Callback function invoked when a hardware semaphore is freed.
 * 
 * This function is called by the HAL when the semaphore specified by SemMask
 * is released. It checks if the CM7 to CM4 semaphore has been freed, and if so,
 * sets a flag indicating that a packet is ready for processing. The notification
 * is then re-armed to detect the next semaphore release.
 * 
 * @param SemMask Bitmask identifying which semaphore(s) have been freed.
 *                The mask is compared against the CM7_TO_CM4 semaphore ID.
 * 
 * @note This callback is typically registered with the HAL semaphore driver
 *       and is invoked in interrupt context. Keep processing minimal.
 * 
 * @see HAL_HSEM_ActivateNotification()
 * @see __HAL_HSEM_SEMID_TO_MASK()
 */
void HAL_HSEM_FreeCallback(uint32_t SemMask)
{
    if (SemMask & __HAL_HSEM_SEMID_TO_MASK(HSEM_CM7_TO_CM4))
    {
        packet_ready = 1;

        /* Re-arm if one-shot */
        HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(HSEM_CM7_TO_CM4));
    }
}


/**
 * @brief Attempts to take a hardware semaphore with a timeout.
 * 
 * @param sem_id The hardware semaphore ID to acquire.
 * @param timeout_ms The timeout duration in milliseconds.
 * 
 * @return 0 if the semaphore was successfully taken, -1 if timeout occurred.
 * 
 * @note This function uses HAL_GetTick() for timeout calculation and should be called
 *       in contexts where hardware semaphores are available (e.g., dual-core systems).
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
 * @brief Sends data from CM4 to CM7 via shared memory with hardware semaphore synchronization.
 * 
 * This function copies the provided buffer to shared memory and signals the CM7 core
 * using a hardware semaphore. The function includes timeout protection to prevent
 * blocking when the CM7 is not ready to receive data.
 * 
 * @param[in] buf       Pointer to the data buffer to send. Must not be NULL if len > 0.
 * @param[in] len       Length of the data in bytes. Must be greater than 0 and not exceed API_MAX_LEN.
 *                      If len is 0 or exceeds API_MAX_LEN, the function returns without sending.
 * 
 * @return void
 * 
 * @note If the hardware semaphore cannot be acquired within the timeout period,
 *       the message is dropped and the function returns without error.
 * @note A data memory barrier is issued before releasing the semaphore to ensure
 *       all writes to shared memory are complete.
 * 
 * @see hsem_take_with_timeout()
 * @see HAL_HSEM_Release()
 */
void cm4_send_to_cm7(const uint8_t *buf, uint16_t len)
{
    if (len == 0 || len > API_MAX_LEN) return;

    if (hsem_take_with_timeout(HSEM_CM4_TO_CM7, 10) != 0) {
        // busy, drop or retry later
        return;
    }

    SHM_M4_TO_M7.len = len;
    memcpy((void*)SHM_M4_TO_M7.data, buf, len);

    __DMB();  // make sure writes complete before doorbell
    HAL_HSEM_Release(HSEM_CM4_TO_CM7, 0);
}
