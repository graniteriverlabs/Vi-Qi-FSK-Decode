#include "qi.h"
#include <string.h>

/* Private defines -----------------------------------------------------------*/
#define QI_MAX_RAW_BITS 1024

/* Private types -------------------------------------------------------------*/
typedef struct {
  uint32_t raw_samples[QI_MAX_RAW_BITS / 32];
  uint8_t b_toggle; // Current state: 0 or 1
} qi_physical_t;

static qi_physical_t qi_phy;

/* Private function prototypes -----------------------------------------------*/
static int qi_write_preamble(uint8_t);
static int qi_write_bit(int bit_off, uint8_t bit_value);
static int qi_encode_byte(int bit_off, uint8_t data);
static int qi_write_last_edge(int bit_off);

/* Public Functions ----------------------------------------------------------*/

int Qi_BuildPacket(Qi_Packet_t *packet, uint8_t header, uint8_t *payload,
                   uint8_t payload_len) {
  if (packet == NULL)
    return -1;
  if (payload_len > QI_MAX_PAYLOAD_BYTES)
    return -1;
  if (payload_len > 0 && payload == NULL)
    return -1;

  packet->header = header;
  packet->payload_len = payload_len;

  if (payload_len > 0) {
    memcpy(packet->payload, payload, payload_len);
  }

  // Calculate checksum (XOR of all bytes)
  packet->checksum = header;
  for (int i = 0; i < payload_len; i++) {
    packet->checksum ^= payload[i];
  }

  return 0;
}

int Qi_EncodeToSPI(Qi_Packet_t *packet, uint8_t *spi_buffer,
                   uint16_t buffer_size) {
  if (packet == NULL || spi_buffer == NULL)
    return -1;

  // Clear raw samples
  memset(qi_phy.raw_samples, 0, sizeof(qi_phy.raw_samples));
  qi_phy.b_toggle = 0; // Start with 0

  int bit_off = 0;

  // 1. Preamble
  bit_off = qi_write_preamble(QI_SS_PREAMBLE_BITS);//Change this as per operating phase as preamble bit count changes based on phase(PING/ID/PT/NEG phases)

  // 2. Header
  bit_off = qi_encode_byte(bit_off, packet->header);

  // 3. Payload
  for (int i = 0; i < packet->payload_len; i++) {
    bit_off = qi_encode_byte(bit_off, packet->payload[i]);
  }

  // 4. Checksum
  bit_off = qi_encode_byte(bit_off, packet->checksum);

  // 5. Final edge
  bit_off = qi_write_last_edge(bit_off);

  // Convert to bytes
  int n_bytes = (bit_off + 7) / 8;
  if (n_bytes > buffer_size)
    return -1;

  memset(spi_buffer, 0, buffer_size);
  memcpy(spi_buffer, &qi_phy.raw_samples[0], n_bytes);

  return n_bytes;
}

/* Convenience Functions -----------------------------------------------------*/

int Qi_CreateSignalStrength(Qi_Packet_t *packet, uint8_t strength) {
  return Qi_BuildPacket(packet, QI_MSG_SIGNAL_STRENGTH, &strength, 1);
}

int Qi_CreateEndPowerTransfer(Qi_Packet_t *packet, uint8_t reason) {
  return Qi_BuildPacket(packet, QI_MSG_END_POWER_TRANSFER, &reason, 1);
}

int Qi_CreateControlError(Qi_Packet_t *packet, uint8_t error) {
  return Qi_BuildPacket(packet, QI_MSG_CONTROL_ERROR, &error, 1);
}

int Qi_CreateReceivedPower(Qi_Packet_t *packet, uint8_t power) {
  return Qi_BuildPacket(packet, QI_MSG_RECEIVED_POWER, &power, 1);
}

int Qi_CreateChargeStatus(Qi_Packet_t *packet, uint8_t status) {
  return Qi_BuildPacket(packet, QI_MSG_CHARGE_STATUS, &status, 1);
}

int Qi_CreateConfiguration(Qi_Packet_t *packet, uint8_t *config_data,
                           uint8_t len) {
  if (len < 1 || len > 6)
    return -1;
  return Qi_BuildPacket(packet, QI_MSG_CONFIGURATION, config_data, len);
}

int Qi_CreateIdentification(Qi_Packet_t *packet, uint8_t *ident_data,
                            uint8_t len) {
  if (len < 4 || len > 7)
    return -1;
  return Qi_BuildPacket(packet, QI_MSG_IDENTIFICATION, ident_data, len);
}

/* Private Functions - BMC Encoding ------------------------------------------*/
static int qi_write_preamble(uint8_t aPreamble_bit_cnt) {
  // Qi preamble is 25 bits of value '1'.
  // Each bit -> 2 half-bits, so total = 50 half-bits.
  int bit_off = 0;

  for (int i = 0; i < aPreamble_bit_cnt; i++) {
    bit_off = qi_write_bit(bit_off, 1);
  }

  return bit_off; // 50 half-bits
}
#if 0
static int qi_write_preamble(void) {
  // Preamble: 25 bits of alternating '10' pattern (Manchester '1')
  // In BMC: '1' = transition in middle = 10 pattern
  uint32_t *msg = qi_phy.raw_samples;

  // 25 bits of '1' = 50 transitions alternating 10101010...
  // Binary: 0xAAAAAAAA = 10101010 10101010 10101010 10101010 (32 bits)
//  msg[0] = 0xAAAAAAAA; // First 32 bits (16 Qi '1's)
  msg[0] = 0x66666666;   // produces 0,1,1,0,0,1,1,0... (2 kHz)

  // Need 9 more Qi '1's = 18 more transitions
  // 0x2AA = 0000 0010 1010 1010 (18 bits starting from bit 32)
//  msg[1] = 0x000002AA;
  msg[1] = 0x00026666;   // 0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1

//  qi_phy.b_toggle = 0; // Preamble ends with 0
  qi_phy.b_toggle = 1;   // 25 toggles from 0 = 1
  return 50;           // 25 Qi bits × 2 BMC transitions each
}

static int qi_write_bit(int bit_off, uint8_t bit_value) {
  uint32_t *msg = qi_phy.raw_samples;
  int word_idx = bit_off / 32;
  int bit_idx = bit_off % 32;

  if (bit_value) {
    // Qi '1' = Manchester: transition in middle
    // Two half-bits with opposite levels

    // First half-bit (current state)
    if (bit_idx < 32) {
      msg[word_idx] |= ((uint32_t)qi_phy.b_toggle << bit_idx);
      if (bit_idx == 31) {
        msg[word_idx + 1] = 0;
      }
    }
    bit_off++;

    // Toggle
    qi_phy.b_toggle ^= 1;

    // Second half-bit (toggled state)
    word_idx = bit_off / 32;
    bit_idx = bit_off % 32;
    if (bit_idx < 32) {
      msg[word_idx] |= ((uint32_t)qi_phy.b_toggle << bit_idx);
    }
    bit_off++;

  } else {
    // Qi '0' = No transition: two half-bits at same level
    for (int i = 0; i < 2; i++) {
      word_idx = bit_off / 32;
      bit_idx = bit_off % 32;

      if (bit_idx < 32) {
        msg[word_idx] |= ((uint32_t)qi_phy.b_toggle << bit_idx);
      }
      bit_off++;
    }

    // Toggle after full bit
    qi_phy.b_toggle ^= 1;
  }

  return bit_off;
}
#endif
static int qi_write_bit(int bit_off, uint8_t bit_value) {
  // Differential bi-phase (BMC) per Qi spec:
  //   ONE  -> two transitions: at bit start AND mid-bit
  //   ZERO -> one transition:  at bit start only
  //
  // Representation here: 1 SPI bit == 1 half-bit (250us at 4kHz).
  // So each Qi bit becomes 2 SPI bits (two half-bits).
  //
  // qi_phy.b_toggle holds the current output level (0/1).

  uint32_t *msg = qi_phy.raw_samples;

  // Transition at start of every bit (coincident with rising edge of fCLK)
  qi_phy.b_toggle ^= 1;

  // Half-bit #1 (write current level)
  int word_idx = bit_off / 32;
  int bit_idx  = bit_off % 32;
  if (qi_phy.b_toggle) {
    msg[word_idx] |= ((uint32_t)1u << bit_idx);
  }
  bit_off++;

  // Mid-bit transition only for a '1' (coincident with falling edge of fCLK)
  if (bit_value) {
    qi_phy.b_toggle ^= 1;
  }

  // Half-bit #2
  word_idx = bit_off / 32;
  bit_idx  = bit_off % 32;
  if (qi_phy.b_toggle) {
    msg[word_idx] |= ((uint32_t)1u << bit_idx);
  }
  bit_off++;

  return bit_off;
}
static int qi_encode_byte(int bit_off, uint8_t data) {
  uint8_t parity = 1; // Odd parity

  // Start bit (0)
  bit_off = qi_write_bit(bit_off, 0);

  // 8 data bits (LSB first)
  for (int i = 0; i < 8; i++) {
    uint8_t bit = (data >> i) & 0x01;
    bit_off = qi_write_bit(bit_off, bit);
    parity ^= bit;
  }

  // Parity bit
  bit_off = qi_write_bit(bit_off, parity);

  // Stop bit (1)
  bit_off = qi_write_bit(bit_off, 1);

  return bit_off;
}

#if 0
static int qi_write_last_edge(int bit_off) {
  uint32_t *msg = qi_phy.raw_samples;
  int word_idx = bit_off / 32;
  int bit_idx = bit_off % 32;

  if (bit_idx == 0)
    msg[word_idx] = 0;

  // Add final transition if currently high
  if (qi_phy.b_toggle) {
    if (bit_idx == 31) {
      msg[word_idx] |= (1 << bit_idx);
      msg[word_idx + 1] = 1;
    } else {
//      msg[word_idx] |= (3 << bit_idx);
    	msg[word_idx] |= ((uint32_t)3u << bit_idx);
    }
  }

  msg[word_idx + 1] = 0;
  return bit_off + 3;
}
#endif

// Choose whether you want OFF after 1/2 UI (1 half-bit) or 1 UI (2 half-bits)
#ifndef QI_TAIL_HALF_BITS
#define QI_TAIL_HALF_BITS  1   // 2 half-bits = 500us (1 UI) at 4kHz //2: 1UI, 1: half UI
#endif

static int qi_write_last_edge(int bit_off)
{
  uint32_t *msg = qi_phy.raw_samples;

  // 1) Force modulator OFF (line = 0) for a short tail.
  // This matches spec: after stop bit, PRx turns off modulator and line goes low.
  // If the line was high, this creates the final "edge" (spec calls it spurious for odd length).
  for (int i = 0; i < QI_TAIL_HALF_BITS; i++) {
    int word_idx = bit_off / 32;
    int bit_idx  = bit_off % 32;

    // write explicit 0 level -> do nothing (buffer is zeroed)
    // (If you ever stop zeroing the buffer, then explicitly clear bit here.)
    msg[word_idx] &= ~((uint32_t)1u << bit_idx);

    bit_off++;
  }

  // We have now "logically" forced the line low
  qi_phy.b_toggle = 0;

  // 2) Pad to next byte boundary using 0 (prevents memcpy rounding artifacts)
  int pad_to_byte = (8 - (bit_off & 7)) & 7;
  for (int i = 0; i < pad_to_byte; i++) {
    int word_idx = bit_off / 32;
    int bit_idx  = bit_off % 32;
    msg[word_idx] &= ~((uint32_t)1u << bit_idx); // keep low
    bit_off++;
  }

  return bit_off;
}
