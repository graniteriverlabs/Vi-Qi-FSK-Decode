#ifndef QI_H
#define QI_H

#include <stdint.h>

/* Qi Protocol Definitions ---------------------------------------------------*/
#define QI_PREAMBLE_BITS 25
#define QI_SS_PREAMBLE_BITS 11

#define QI_MAX_PAYLOAD_BYTES 32
#define QI_MAX_PACKET_SIZE 512

/* Qi Message Headers (from Qi v1.2 spec) -----------------------------------*/
#define QI_MSG_SIGNAL_STRENGTH 0x01
#define QI_MSG_END_POWER_TRANSFER 0x02
#define QI_MSG_CONTROL_ERROR 0x03
#define QI_MSG_RECEIVED_POWER 0x04
#define QI_MSG_CHARGE_STATUS 0x05
#define QI_MSG_POWER_CONTROL_HOLD 0x06
#define QI_MSG_CONFIGURATION 0x51
#define QI_MSG_IDENTIFICATION 0x71
#define QI_MSG_EXTENDED_IDENT 0x81

/* Qi Packet Structure -------------------------------------------------------*/
typedef struct {
  uint8_t header;
  uint8_t payload[QI_MAX_PAYLOAD_BYTES];
  uint8_t payload_len;
  uint8_t checksum;
} Qi_Packet_t;

/* Public Function Prototypes ------------------------------------------------*/

/**
 * @brief Build a Qi packet from header and payload
 */
int Qi_BuildPacket(Qi_Packet_t *packet, uint8_t header, uint8_t *payload,
                   uint8_t payload_len);

/**
 * @brief Encode Qi packet to SPI byte stream
 */
int Qi_EncodeToSPI(Qi_Packet_t *packet, uint8_t *spi_buffer,
                   uint16_t buffer_size);

/* Convenience Functions -----------------------------------------------------*/
int Qi_CreateSignalStrength(Qi_Packet_t *packet, uint8_t strength);
int Qi_CreateEndPowerTransfer(Qi_Packet_t *packet, uint8_t reason);
int Qi_CreateControlError(Qi_Packet_t *packet, uint8_t error);
int Qi_CreateReceivedPower(Qi_Packet_t *packet, uint8_t power);
int Qi_CreateChargeStatus(Qi_Packet_t *packet, uint8_t status);
int Qi_CreateConfiguration(Qi_Packet_t *packet, uint8_t *config_data,
                           uint8_t len);
int Qi_CreateIdentification(Qi_Packet_t *packet, uint8_t *ident_data,
                            uint8_t len);

#endif /* QI_H */
