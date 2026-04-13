/*
 * fsk_stream_test.h
 *
 * @brief CM4-side synthetic test stream generator for verifying the
 *        CM4->CM7->UDP pipeline.
 *  Created on: Mar 20, 2026
 *      Author: kanna
 */

#ifndef INC_FSK_STREAM_TEST_H_
#define INC_FSK_STREAM_TEST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief Synthetic 8-byte test record used for end-to-end validation.
 *
 * This record is intentionally the same size as the normal online FSK
 * stream record so that the packetization path remains unchanged.
 */
typedef struct __attribute__((packed))
{
    uint32_t counter; /**< Monotonic test counter */
    uint16_t pattern; /**< Fixed validation pattern, e.g. 0xA55A */
    uint8_t  source;  /**< Source identifier */
    uint8_t  flags;   /**< Test flags */
} fsk_test_record_t;

/**
 * @brief Initialize the synthetic stream test generator.
 */
void fsk_stream_test_init(void);

/**
 * @brief Emit one synthetic test record into the CM4->CM7 shared ring.
 */
void fsk_stream_test_emit(void);

#ifdef __cplusplus
}
#endif


#endif /* INC_FSK_STREAM_TEST_H_ */
