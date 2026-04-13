/*
 * @file ipc_stream_shared.c
 * @brief Shared ring buffer storage definition for CM4 -> CM7 streaming.
 *
 *  Created on: Mar 10, 2026
 *      Author: kanna
 */

#include "ipc_stream_shared.h"

/**
 * @brief Shared ring instance.
 *
 * This object must live in a memory region visible to both CM4 and CM7.
 * The linker section name must match the linker scripts for both cores.
 *
 * @note This file should be compiled only once in the dual-core system.
 *       In your current architecture, compile this file only in the CM4 project.
 */
__attribute__((section(".shared_stream_ram"), aligned(32)))
volatile ipc_stream_ring_t g_ipc_stream_cm4_to_cm7;
