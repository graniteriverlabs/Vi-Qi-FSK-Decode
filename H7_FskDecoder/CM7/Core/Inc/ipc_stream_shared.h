/*
 * ipc_stream_shared.h
 *
 *  Created on: Mar 10, 2026
 *      Author: kanna
 */

#ifndef INC_IPC_STREAM_SHARED_H_
#define INC_IPC_STREAM_SHARED_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "main.h"

/**
 * @file ipc_stream_shared.h
 * @brief Shared CM4 -> CM7 streaming ring buffer definitions and helpers.
 *
 * This header is included by both CM4 and CM7.
 *
 * Design model:
 * - CM4 is the producer and writes stream records into the ring.
 * - CM7 is the consumer and drains the ring for UDP transmission.
 * - The shared ring object itself is defined exactly once in a C file.
 * - Access helper functions are provided here as static inline functions,
 *   so both cores can use them without requiring a separately linked C file.
 */

/**
 * @brief Total payload capacity of the shared byte ring.
 *
 * The value is chosen so that the full ring structure, including metadata
 * and 32-byte alignment, fits inside the 64 KB shared streaming RAM region.
 */
#define IPC_STREAM_RING_SIZE        (65504U)

/**
 * @brief Threshold after which CM4 may notify CM7 that fresh data exists.
 *
 * The idea is to avoid notifying for every record. Instead, CM4 accumulates
 * a chunk of data and then signals CM7 once.
 */
#define IPC_STREAM_NOTIFY_CHUNK     (1024U)

/**
 * @brief Bit flags that may be carried inside a debug record.
 */
typedef enum
{
    IPC_STREAM_FLAG_TOGGLED          = (1U << 0),
    IPC_STREAM_FLAG_VALID_TRANSITION = (1U << 1),
    IPC_STREAM_FLAG_SIGN_POSITIVE    = (1U << 2)
} ipc_stream_flags_t;

/**
 * @brief Compact online debug record written by CM4.
 *
 * This record is intentionally small so that online streaming bandwidth stays
 * manageable. It is not meant to be a full offline raw capture format.
 *
 * Field sizing rationale:
 * - period:  most signal-related periods will fit in 16 bits after scaling
 * - delta:   signed delayed-difference value, clamped to int16 range
 * - ma:      moving average value, clamped to uint16 range
 * - flags:   bitfield for toggled / valid transition / sign
 * - state:   decoder state at the time of record generation
 *
 * Total size: 8 bytes
 */
typedef struct __attribute__((packed))
{
    uint16_t period;   /**< Scaled period value for online visibility */
    int16_t  delta;    /**< Signed delayed-difference value */
    uint16_t ma;       /**< Moving-average value */
    uint8_t  flags;    /**< Bitfield of @ref ipc_stream_flags_t */
    uint8_t  state;    /**< Decoder state value */
} ipc_stream_record_t;

/**
 * @brief Shared byte-stream ring buffer for CM4 -> CM7 transport.
 *
 * This is a single-producer / single-consumer ring:
 * - CM4 updates @ref wr
 * - CM7 updates @ref rd
 *
 * Counters:
 * - dropped: incremented when CM4 cannot write due to insufficient space
 * - high_water: highest observed ring occupancy
 *
 * @note This structure must be placed in memory visible to both cores.
 * @note On CM7, the shared region should ideally be configured non-cacheable.
 */
typedef struct __attribute__((aligned(32)))
{
    volatile uint32_t wr;          /**< Producer write index */
    volatile uint32_t rd;          /**< Consumer read index */
    volatile uint32_t dropped;     /**< Number of dropped writes */
    volatile uint32_t high_water;  /**< Maximum observed occupancy */
    volatile uint8_t  data[IPC_STREAM_RING_SIZE]; /**< Byte storage */
} ipc_stream_ring_t;

/**
 * @brief Shared CM4 -> CM7 stream ring instance.
 *
 * This object must be defined exactly once in a source file and placed into
 * a linker section mapped to shared RAM.
 */
extern volatile ipc_stream_ring_t g_ipc_stream_cm4_to_cm7;

/**
 * @brief Compute ring occupancy from write/read indices.
 *
 * @param[in] wr Producer write index.
 * @param[in] rd Consumer read index.
 *
 * @return Number of bytes currently stored in the ring.
 */
static inline uint32_t ipc_stream_ring_used_internal(uint32_t wr, uint32_t rd)
{
    if (wr >= rd)
    {
        return (wr - rd);
    }

    return (IPC_STREAM_RING_SIZE - rd + wr);
}

/**
 * @brief Initialize the shared stream ring.
 *
 * This helper clears all metadata and payload bytes.
 *
 * @note This should normally be called only by the owner core responsible
 *       for initial shared-memory setup.
 */
static inline void ipc_stream_init(void)
{
    g_ipc_stream_cm4_to_cm7.wr         = 0U;
    g_ipc_stream_cm4_to_cm7.rd         = 0U;
    g_ipc_stream_cm4_to_cm7.dropped    = 0U;
    g_ipc_stream_cm4_to_cm7.high_water = 0U;

    for (uint32_t i = 0U; i < IPC_STREAM_RING_SIZE; i++)
    {
        g_ipc_stream_cm4_to_cm7.data[i] = 0U;
    }

    __DMB();
}

/**
 * @brief Return number of bytes currently stored in the ring.
 *
 * @return Number of bytes available for CM7 to consume.
 */
static inline uint32_t ipc_stream_used(void)
{
    return ipc_stream_ring_used_internal(g_ipc_stream_cm4_to_cm7.wr,
                                         g_ipc_stream_cm4_to_cm7.rd);
}

/**
 * @brief Return number of free bytes currently available in the ring.
 *
 * One byte is intentionally left unused to distinguish:
 * - empty: wr == rd
 * - full:  next_wr == rd
 *
 * @return Number of free bytes available for CM4 to produce.
 */
static inline uint32_t ipc_stream_free(void)
{
    return (IPC_STREAM_RING_SIZE - 1U - ipc_stream_used());
}

/**
 * @brief Write a block of bytes into the shared stream ring.
 *
 * This function is intended for CM4 producer usage.
 *
 * @param[in] buf Pointer to source data.
 * @param[in] len Number of bytes to write.
 *
 * @retval 0  Write successful.
 * @retval -1 Invalid arguments or insufficient free space.
 *
 * @note If insufficient space exists, the function drops the write and
 *       increments the ring's dropped counter.
 */
static inline int ipc_stream_write(const uint8_t *buf, uint32_t len)
{
    uint32_t wr;
    uint32_t used;

    if ((buf == NULL) || (len == 0U) || (len >= IPC_STREAM_RING_SIZE))
    {
        return -1;
    }

    if (ipc_stream_free() < len)
    {
        g_ipc_stream_cm4_to_cm7.dropped++;
        return -1;
    }

    wr = g_ipc_stream_cm4_to_cm7.wr;

    for (uint32_t i = 0U; i < len; i++)
    {
        g_ipc_stream_cm4_to_cm7.data[wr] = buf[i];

        wr++;
        if (wr >= IPC_STREAM_RING_SIZE)
        {
            wr = 0U;
        }
    }

    /**
     * Ensure payload bytes are visible before publishing the new write index.
     */
    __DMB();

    g_ipc_stream_cm4_to_cm7.wr = wr;

    used = ipc_stream_used();
    if (used > g_ipc_stream_cm4_to_cm7.high_water)
    {
        g_ipc_stream_cm4_to_cm7.high_water = used;
    }

    return 0;
}

/**
 * @brief Read a block of bytes from the shared stream ring.
 *
 * This function is intended for CM7 consumer usage.
 *
 * @param[out] buf Destination buffer.
 * @param[in]  len Maximum number of bytes to read.
 *
 * @return Number of bytes actually read.
 */
static inline uint32_t ipc_stream_read(uint8_t *buf, uint32_t len)
{
    uint32_t rd;
    uint32_t available;
    uint32_t read_len;

    if ((buf == NULL) || (len == 0U))
    {
        return 0U;
    }

    /**
     * Ensure CM7 observes the latest producer-visible ring contents before
     * reading indices and payload.
     */
    __DMB();

    available = ipc_stream_used();
    read_len  = (len < available) ? len : available;
    rd        = g_ipc_stream_cm4_to_cm7.rd;

    for (uint32_t i = 0U; i < read_len; i++)
    {
        buf[i] = g_ipc_stream_cm4_to_cm7.data[rd];

        rd++;
        if (rd >= IPC_STREAM_RING_SIZE)
        {
            rd = 0U;
        }
    }

    /**
     * Publish updated read index after data has been copied out.
     */
    __DMB();

    g_ipc_stream_cm4_to_cm7.rd = rd;

    return read_len;
}

#ifdef __cplusplus
}
#endif

#endif /* INC_IPC_STREAM_SHARED_H_ */
