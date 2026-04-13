/**
 * @file fsk_stream_test.c
 * @brief CM4-side synthetic test stream generator.
 */

#include "fsk_stream_test.h"
#include "ipc_stream_shared.h"

/**
 * @brief Monotonic test record counter.
 */
static uint32_t g_fsk_test_counter = 0U;

void fsk_stream_test_init(void)
{
    g_fsk_test_counter = 0U;
}

void fsk_stream_test_emit(void)
{
    fsk_test_record_t rec;

    rec.counter = g_fsk_test_counter++;
    rec.pattern = 0xA55AU;
    rec.source  = 0xC4U;
    rec.flags   = 0x5AU;

    (void)ipc_stream_write((const uint8_t *)&rec, sizeof(rec));
}
