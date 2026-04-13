

#define MG_OTA_NONE 0       // No OTA support
#define MG_OTA_STM32H5 1    // STM32 H5
#define MG_OTA_STM32H7 2    // STM32 H7
#define MG_OTA_STM32H7_DUAL_CORE 3 // STM32 H7 dual core
#define MG_OTA_STM32F  4    // STM32 F7/F4/F2
#define MG_OTA_CH32V307 100 // WCH CH32V307
#define MG_OTA_U2A 200      // Renesas U2A16, U2A8, U2A6
#define MG_OTA_RT1020 300   // IMXRT1020
#define MG_OTA_RT1050 301   // IMXRT1050
#define MG_OTA_RT1060 302   // IMXRT1060
#define MG_OTA_RT1064 303   // IMXRT1064
#define MG_OTA_RT1170 304   // IMXRT1170
#define MG_OTA_MCXN 310 	  // MCXN947
#define MG_OTA_FRDM 320    // FRDM-RW612
#define MG_OTA_FLASH 900    // OTA via an internal flash
#define MG_OTA_ESP32 910    // ESP32 OTA implementation
#define MG_OTA_PICOSDK 920  // RP2040/2350 using Pico-SDK hardware_flash
#define MG_OTA_CUSTOM 1000  // Custom implementation

#define MG_ARCH MG_ARCH_THREADX

#define MG_ENABLE_TCPIP 0

#define MG_ENABLE_PACKED_FS  1

#define MG_STMPACK_TLS 0

#define MG_ENABLE_DRIVER_STM32 0
#define MG_OTA 	MG_OTA_STM32H7

// Translate to Mongoose macros
#if MG_STMPACK_TLS == 0
#define MG_TLS MG_TLS_NONE
#elif MG_STMPACK_TLS == 1
#define MG_TLS MG_TLS_BUILTIN
#elif MG_STMPACK_TLS == 2
#define MG_TLS MG_TLS_MBEDTLS
#elif MG_STMPACK_TLS == 3
#define MG_TLS MG_TLS_WOLFSSL
#endif


// See https://mongoose.ws/documentation/#build-options
