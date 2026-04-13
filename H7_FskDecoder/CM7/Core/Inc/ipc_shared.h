/*
 * ipc_shared.h
 *
 *  Created on: Jan 13, 2026
 *      Author: pavan
 */

#ifndef INC_IPC_SHARED_H_
#define INC_IPC_SHARED_H_

#include <stdint.h>
#pragma once
#include <stdint.h>
#include "stm32h7xx_hal.h"
#include "app_netxduo.h"
#include <cm7_qi_app.h>

#define HSEM_CM7_TO_CM4   1
#define HSEM_CM4_TO_CM7   2

/**
 * @brief Calculates the cache-line aligned size for a given size value.
 * 
 * This function takes a size value and rounds it up to the next multiple of
 * the cache line size (32 bytes). This is useful for memory allocation and
 * buffer management to ensure proper cache alignment.
 * 
 * @param sz The original size in bytes to be aligned.
 * @return The cache-line aligned size (rounded up to the nearest multiple of CACHELINE).
 * 
 * @note The alignment is done using bitwise operations:
 *       - Add (CACHELINE - 1) to round up
 *       - Use bitwise AND with ~(CACHELINE - 1) to clear lower bits
 * 
 * @example
 *       uint32_t aligned = cache_align_size(100);  // Returns 128
 *       uint32_t aligned = cache_align_size(32);   // Returns 32
 */
#define CACHELINE 32U
static inline uint32_t cache_align_size(uint32_t sz) {
    return (sz + (CACHELINE - 1U)) & ~(CACHELINE - 1U);
}

/**
 * @struct shm_packet_t
 * @brief Shared memory packet structure for inter-processor communication (IPC).
 * 
 * This structure defines the format of packets exchanged between processors
 * via shared memory. It is packed and aligned to 32-byte boundaries for
 * optimal DMA and cache performance.
 * 
 * @param len Length of valid data in the packet (in bytes).
 *            This field indicates how many bytes of the data array contain
 *            valid payload information.
 * @param data Payload buffer containing the packet data.
 *             Size is limited to API_MAX_LEN bytes.
 * 
 * @note Both fields are declared volatile to prevent compiler optimizations
 *       that could interfere with inter-processor communication.
 * @note The structure uses __attribute__((packed)) to eliminate padding and
 *       __attribute__((aligned(32))) to ensure 32-byte alignment for shared
 *       memory and DMA operations.
 */
typedef struct __attribute__((aligned(32)))
{
    volatile uint32_t len;
    volatile uint8_t  data[API_MAX_LEN];
} shm_packet_t;

/* Two mailboxes in shared RAM */
__attribute__((section(".shared_ram"), aligned(32)))
extern volatile shm_packet_t SHM_M7_TO_M4;

__attribute__((section(".shared_ram"), aligned(32)))
extern volatile shm_packet_t SHM_M4_TO_M7;

void ipc_shm_init(void);
void cm7_send_to_cm4(const uint8_t *buf, uint16_t len);
int hsem_take_with_timeout(uint32_t sem_id, uint32_t timeout_ms);

#endif /* INC_IPC_SHARED_H_ */
