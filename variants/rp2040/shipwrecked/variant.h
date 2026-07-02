#pragma once

#define ARDUINO_ARCH_AVR

// #define EXT_NOTIFY_OUT 22 // Conflict with LORA_BUSY (Pin 22)
#define BUTTON_PIN -1

// #define LED_PIN PIN_LED // Conflict with PIN_EINK_DC (PIN_LED is 25 on rpipico)

#define HAS_CPU_SHUTDOWN 1

#define USE_SX1262

#define USE_EINK

#define PIN_EINK_CS 24
#define PIN_EINK_DC 25
#define PIN_EINK_RES 26
#define PIN_EINK_BUSY 27

#define EINK_DISPLAY_MODEL GxEPD2_154_D67
#define EINK_WIDTH 200
#define EINK_HEIGHT 200

#undef LORA_SCK
#undef LORA_MISO
#undef LORA_MOSI
#undef LORA_CS

#define LORA_SCK 18
#define LORA_MISO 20
#define LORA_MOSI 19
#define LORA_CS 21

#define LORA_DIO0 RADIOLIB_NC
#define LORA_RESET 23
#define LORA_BUSY 22
#define LORA_DIO1 17
#define LORA_DIO2 RADIOLIB_NC
#define LORA_DIO3 RADIOLIB_NC
#define LORA_DIO4 RADIOLIB_NC

#ifdef USE_SX1262
#define SX126X_CS LORA_CS
#define SX126X_DIO1 LORA_DIO1
#define SX126X_BUSY LORA_BUSY
#define SX126X_RESET LORA_RESET
#define SX126X_DIO2_AS_RF_SWITCH
#endif

#define I2C_SDA 0
#define I2C_SCL 1

#define PCA9555_ADDR 0x20
#define PIN_PCA9555_INT 2
