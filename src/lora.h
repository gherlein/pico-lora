#ifndef LORA_H
#define LORA_H

#include <stdint.h>
#include <stdbool.h>
#include "print.h"
#include "pico/time.h"

// Error printing configuration
#ifndef LORA_ERROR_PRINT
#define LORA_ERROR_PRINT 1
#endif

// Maximum packet length
#define MAX_PKT_LENGTH 255

// LoRa registers
#define REG_FIFO                 0x00
#define REG_OP_MODE             0x01
#define REG_FRF_MSB             0x06
#define REG_FRF_MID             0x07
#define REG_FRF_LSB             0x08
#define REG_PA_CONFIG           0x09
#define REG_LNA                 0x0c
#define REG_FIFO_ADDR_PTR       0x0d
#define REG_FIFO_TX_BASE_ADDR   0x0e
#define REG_FIFO_RX_BASE_ADDR   0x0f
#define REG_FIFO_RX_CURRENT_ADDR 0x10
#define REG_IRQ_FLAGS           0x12
#define REG_RX_NB_BYTES         0x13
#define REG_PKT_SNR_VALUE       0x19
#define REG_PKT_RSSI_VALUE      0x1a
#define REG_MODEM_CONFIG_1      0x1d
#define REG_MODEM_CONFIG_2      0x1e
#define REG_PREAMBLE_MSB        0x20
#define REG_PREAMBLE_LSB        0x21
#define REG_PAYLOAD_LENGTH      0x22
#define REG_MODEM_CONFIG_3      0x26
#define REG_FREQ_ERROR_MSB      0x28
#define REG_FREQ_ERROR_MID      0x29
#define REG_FREQ_ERROR_LSB      0x2a
#define REG_RSSI_WIDEBAND       0x2c
#define REG_DETECTION_OPTIMIZE   0x31
#define REG_DETECTION_THRESHOLD  0x37
#define REG_SYNC_WORD           0x39
#define REG_DIO_MAPPING_1       0x40
#define REG_VERSION             0x42
#define REG_PA_DAC              0x4d
#define REG_INVERTIQ            0x33
#define REG_INVERTIQ2           0x3b
#define REG_OCP                 0x0b

// Modes
#define MODE_LONG_RANGE_MODE    0x80
#define MODE_SLEEP             0x00
#define MODE_STDBY             0x01
#define MODE_TX                0x03
#define MODE_RX_CONTINUOUS     0x05
#define MODE_RX_SINGLE         0x06
#define MODE_CAD               0x07

// PA config
#define PA_BOOST               0x80
#define PA_OUTPUT_RFO_PIN      0
#define PA_OUTPUT_PA_BOOST_PIN 1

// IRQ masks
#define IRQ_TX_DONE_MASK           0x08
#define IRQ_PAYLOAD_CRC_ERROR_MASK 0x20
#define IRQ_RX_DONE_MASK           0x40

// LoRa configuration
typedef struct {
    uint32_t frequency;
    int8_t power;
    uint8_t tx_power;
    uint8_t spreading_factor;
    uint8_t signal_bandwidth;
    uint8_t coding_rate;
    uint8_t preamble_length;
    uint8_t sync_word;
    bool crc_enabled;
    bool invert_iq;
    uint8_t dio0_pin;
} lora_config_t;

// LoRa context
typedef struct {
    print_ctx_t print;
    lora_config_t config;
    bool initialized;
    int implicit_header_mode;
    uint8_t frequency_error;
    int8_t packet_index;
    int8_t packet_length;
    int16_t packet_rssi;
    float packet_snr;
    bool is_receiving;
    bool enable_crc;
} lora_ctx_t;

// Initialize LoRa context
void lora_init(lora_ctx_t *ctx);

// Begin LoRa operation
bool lora_begin(lora_ctx_t *ctx, uint32_t frequency);

// End LoRa operation
void lora_end(lora_ctx_t *ctx);

// Send packet
bool lora_begin_packet(lora_ctx_t *ctx, bool implicit_header);
bool lora_end_packet(lora_ctx_t *ctx, bool async);

// Receive packet
int lora_parse_packet(lora_ctx_t *ctx, int size);
int lora_packet_rssi(lora_ctx_t *ctx);
float lora_packet_snr(lora_ctx_t *ctx);
long lora_packet_frequency_error(lora_ctx_t *ctx);

// Write data
size_t lora_write(lora_ctx_t *ctx, const uint8_t *buffer, size_t size);
size_t lora_write_byte(lora_ctx_t *ctx, uint8_t byte);

// Read data
int lora_available(lora_ctx_t *ctx);
int lora_read(lora_ctx_t *ctx);
int lora_peek(lora_ctx_t *ctx);
void lora_flush(lora_ctx_t *ctx);
size_t lora_read_bytes(lora_ctx_t *ctx, uint8_t *buffer, size_t size);

// Configuration
void lora_idle(lora_ctx_t *ctx);
void lora_sleep(lora_ctx_t *ctx);
void lora_set_tx_power(lora_ctx_t *ctx, int level, int output_pin);
void lora_set_frequency(lora_ctx_t *ctx, uint32_t frequency);
void lora_set_spreading_factor(lora_ctx_t *ctx, int sf);
void lora_set_signal_bandwidth(lora_ctx_t *ctx, uint32_t sbw);
void lora_set_coding_rate4(lora_ctx_t *ctx, int denominator);
void lora_set_preamble_length(lora_ctx_t *ctx, uint16_t length);
void lora_set_sync_word(lora_ctx_t *ctx, int sw);
void lora_enable_crc(lora_ctx_t *ctx);
void lora_disable_crc(lora_ctx_t *ctx);
void lora_enable_invert_iq(lora_ctx_t *ctx);
void lora_disable_invert_iq(lora_ctx_t *ctx);
void lora_set_ocp(lora_ctx_t *ctx, uint8_t current);

// Status
uint8_t lora_random(lora_ctx_t *ctx);
void lora_dump_registers(lora_ctx_t *ctx);
int16_t lora_rssi(lora_ctx_t *ctx);
float lora_snr(lora_ctx_t *ctx);

// Low-level SPI
uint8_t lora_single_transfer(lora_ctx_t *ctx, uint8_t address, uint8_t value);

// Error printing functions
#if LORA_ERROR_PRINT
void lora_error_print(lora_ctx_t *ctx, const char *format, ...);
void lora_error_print_hex(lora_ctx_t *ctx, const char *prefix, const uint8_t *data, size_t len);
#endif

#endif // LORA_H 