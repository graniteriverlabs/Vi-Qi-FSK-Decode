/*
 * ipc_shared.h
 *
 *  Created on: Jan 13, 2026
 *      Author: pavan
 */

#ifndef INC_IPC_SHARED_H_
#define INC_IPC_SHARED_H_

#include <stdint.h>
#include <stdint.h>
#include "stm32h7xx_hal.h"
//#include "app_netxduo.h"
//#include "cm7_grl_app.h"
#define API_MAX_LEN	256

#define HSEM_CM7_TO_CM4   1
#define HSEM_CM4_TO_CM7   2

/**
 * @brief Calculate the cache-aligned size for a given byte count.
 * 
 * This function rounds up the provided size to the next cache line boundary
 * (32 bytes). It ensures that allocated memory is properly aligned for optimal
 * cache performance.
 * 
 * @param sz The original size in bytes to be aligned.
 * @return The cache-aligned size, rounded up to the nearest multiple of CACHELINE (32).
 * 
 * @note This is an inline function for performance-critical code paths.
 * @note The result will always be >= sz and a multiple of 32.
 * 
 * @example
 * uint32_t aligned = cache_align_size(100); // Returns 128
 * uint32_t aligned = cache_align_size(32);  // Returns 32
 */
#define CACHELINE 32U
static inline uint32_t cache_align_size(uint32_t sz) {
    return (sz + (CACHELINE - 1U)) & ~(CACHELINE - 1U);
}

/**
 * @struct shm_packet_t
 * @brief Shared memory packet structure for inter-processor communication.
 *
 * This structure defines the layout of a packet used for IPC (Inter-Processor Communication)
 * with specific alignment and packing requirements for shared memory operations.
 *
 * @details
 * - Packed attribute ensures no padding between members
 * - 32-byte alignment ensures proper cache line alignment for shared memory access
 * - All members are volatile to prevent compiler optimizations on shared memory accesses
 *
 * @var shm_packet_t::len
 *      Length of the data payload in bytes. Indicates how many bytes in the data array
 *      contain valid packet information.
 *
 * @var shm_packet_t::data
 *      Payload buffer containing the packet data. Size is limited by API_MAX_LEN constant.
 *      Only the first @c len bytes contain valid data.
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
void cm4_send_to_cm7(const uint8_t *buf, uint16_t len);
int hsem_take_with_timeout(uint32_t sem_id, uint32_t timeout_ms);

#endif /* INC_IPC_SHARED_H_ */
