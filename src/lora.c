#include "lora.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "print.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// Default pins for RP2040
//#define LORA_DEFAULT_SS_PIN    17
//#define LORA_DEFAULT_RESET_PIN 15
//#define LORA_DEFAULT_DIO0_PIN  14
#define LORA_DEFAULT_SS_PIN    8
#define LORA_DEFAULT_RESET_PIN 9
#define LORA_DEFAULT_DIO0_PIN  10
#define LORA_DEFAULT_SPI_PORT  spi0
#define LORA_SPI_CLOCK_SPEED   10000000  // 10MHz

// SPI pins for RP2040
#define LORA_SPI_SCK_PIN  18
#define LORA_SPI_MOSI_PIN 19
#define LORA_SPI_MISO_PIN 16

// Forward declarations of static functions
static uint8_t read_register(lora_ctx_t *ctx, uint8_t address);
static void write_register(lora_ctx_t *ctx, uint8_t address, uint8_t value);
static void write_register_burst(lora_ctx_t *ctx, uint8_t address, const uint8_t *buffer, size_t size);
static void read_register_burst(lora_ctx_t *ctx, uint8_t address, uint8_t *buffer, size_t size);
static void set_mode(lora_ctx_t *ctx, uint8_t mode);
static void set_ldo_flag(lora_ctx_t *ctx);
static void explicit_header_mode(lora_ctx_t *ctx);
static void implicit_header_mode(lora_ctx_t *ctx);
static bool is_transmitting(lora_ctx_t *ctx);
static int get_spreading_factor(lora_ctx_t *ctx);
static uint32_t get_signal_bandwidth(lora_ctx_t *ctx);

#if LORA_ERROR_PRINT
void lora_error_print(lora_ctx_t *ctx, const char *format, ...) {
    if (!ctx || !ctx->print.write_buffer) return;
    
    print_str(&ctx->print, "[LoRa Error] ");
    
    // Check if format contains any format specifiers
    if (strchr(format, '%') != NULL) {
        // Handle formatted string
        va_list args;
        va_start(args, format);
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        print_str(&ctx->print, buffer);
    } else {
        // Handle simple string
        print_str(&ctx->print, format);
    }
    
    println(&ctx->print);
}

void lora_error_print_hex(lora_ctx_t *ctx, const char *prefix, const uint8_t *data, size_t len) {
    if (!ctx || !ctx->print.write_buffer || !data || !len) return;
    
    print_str(&ctx->print, "[LoRa Error] ");
    if (prefix) {
        print_str(&ctx->print, prefix);
        print_str(&ctx->print, " ");
    }
    
    for (size_t i = 0; i < len; i++) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02x", data[i]);
        print_str(&ctx->print, hex);
    }
    println(&ctx->print);
}
#endif

// Initialize LoRa context
void lora_init(lora_ctx_t *ctx) {
    memset(ctx, 0, sizeof(lora_ctx_t));
    print_init(&ctx->print);
}

// Begin LoRa operation
bool lora_begin(lora_ctx_t *ctx, uint32_t frequency) {
    lora_error_print(ctx, "lora_begin %ld", frequency);
    
    // Initialize SPI
    spi_init(LORA_DEFAULT_SPI_PORT, LORA_SPI_CLOCK_SPEED);
    
    // Configure SPI pins
    gpio_set_function(LORA_SPI_SCK_PIN, GPIO_FUNC_SPI);   // SCK
    gpio_set_function(LORA_SPI_MOSI_PIN, GPIO_FUNC_SPI);  // MOSI
    gpio_set_function(LORA_SPI_MISO_PIN, GPIO_FUNC_SPI);  // MISO
    
    // Configure SS pin
    gpio_init(LORA_DEFAULT_SS_PIN);
    gpio_set_dir(LORA_DEFAULT_SS_PIN, GPIO_OUT);
    gpio_put(LORA_DEFAULT_SS_PIN, 1);  // Set SS high (inactive)
    
    // Configure RESET pin
    gpio_init(LORA_DEFAULT_RESET_PIN);
    gpio_set_dir(LORA_DEFAULT_RESET_PIN, GPIO_OUT);
    gpio_put(LORA_DEFAULT_RESET_PIN, 1);  // Set RESET high (active)
    
    // Configure DIO0 pin
    gpio_init(LORA_DEFAULT_DIO0_PIN);
    gpio_set_dir(LORA_DEFAULT_DIO0_PIN, GPIO_IN);
    
    // Set SPI format
    //spi_set_format(LORA_DEFAULT_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    
    // Reset module
    gpio_put(LORA_DEFAULT_RESET_PIN, 0);
    sleep_ms(10);
    gpio_put(LORA_DEFAULT_RESET_PIN, 1);
    sleep_ms(10);
    
    // Check version
    uint8_t version = read_register(ctx, REG_VERSION);
    lora_error_print(ctx, "Version: 0x%02x", version);
    if (version != 0x12) {
        lora_error_print(ctx, "Failed to read the REG_VERSION register");
        return false;
    }

    // Put in sleep mode
    lora_sleep(ctx);

    // Set frequency
    lora_set_frequency(ctx, frequency);

    // Set base addresses
    write_register(ctx, REG_FIFO_TX_BASE_ADDR, 0);
    write_register(ctx, REG_FIFO_RX_BASE_ADDR, 0);

    // Set LNA boost
    write_register(ctx, REG_LNA, read_register(ctx, REG_LNA) | 0x03);

    // Set auto AGC
    write_register(ctx, REG_MODEM_CONFIG_3, 0x04);

    // Set output power to 17 dBm
    lora_set_tx_power(ctx, 17, PA_OUTPUT_PA_BOOST_PIN);

    // Put in standby mode
    lora_idle(ctx);

    ctx->initialized = true;
    return true;
}

// End LoRa operation
void lora_end(lora_ctx_t *ctx) {
    // Put in sleep mode
    lora_sleep(ctx);
    ctx->initialized = false;
}

// Send packet
bool lora_begin_packet(lora_ctx_t *ctx, bool implicit_header) {
    if (is_transmitting(ctx)) {
        return false;
    }

    // Put in standby mode
    lora_idle(ctx);

    if (implicit_header) {
        implicit_header_mode(ctx);
    } else {
        explicit_header_mode(ctx);
    }

    // Reset FIFO address and payload length
    write_register(ctx, REG_FIFO_ADDR_PTR, 0);
    write_register(ctx, REG_PAYLOAD_LENGTH, 0);

    return true;
}

bool lora_end_packet(lora_ctx_t *ctx, bool async) {
    if ((async) && (ctx->config.dio0_pin > 0)) {
        write_register(ctx, REG_DIO_MAPPING_1, 0x40); // DIO0 => TXDONE
    }

    // Put in TX mode
    write_register(ctx, REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);

    if (!async) {
        // Wait for TX done
        while ((read_register(ctx, REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) == 0) {
            sleep_ms(1);
        }
        // Clear IRQ's
        write_register(ctx, REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
    }

    return true;
}

// Receive packet
int lora_parse_packet(lora_ctx_t *ctx, int size) {
    int packet_length = 0;
    int irq_flags = read_register(ctx, REG_IRQ_FLAGS);

    if (size > 0) {
        implicit_header_mode(ctx);
        write_register(ctx, REG_PAYLOAD_LENGTH, size & 0xff);
    } else {
        explicit_header_mode(ctx);
    }

    // Clear IRQ's
    write_register(ctx, REG_IRQ_FLAGS, irq_flags);

    if ((irq_flags & IRQ_RX_DONE_MASK) && (irq_flags & IRQ_PAYLOAD_CRC_ERROR_MASK) == 0) {
        // Received a packet
        ctx->packet_index = 0;

        // Read packet length
        if (ctx->implicit_header_mode) {
            packet_length = read_register(ctx, REG_PAYLOAD_LENGTH);
        } else {
            packet_length = read_register(ctx, REG_RX_NB_BYTES);
        }

        // Set FIFO address to current RX address
        write_register(ctx, REG_FIFO_ADDR_PTR, read_register(ctx, REG_FIFO_RX_CURRENT_ADDR));

        // Put in standby mode
        lora_idle(ctx);
    } else if (read_register(ctx, REG_OP_MODE) != (MODE_LONG_RANGE_MODE | MODE_RX_SINGLE)) {
        // Not currently in RX mode
        // Reset FIFO address
        write_register(ctx, REG_FIFO_ADDR_PTR, 0);

        // Put in single RX mode
        write_register(ctx, REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_SINGLE);
    }

    return packet_length;
}

int16_t lora_rssi(lora_ctx_t *ctx) {
    return (read_register(ctx, REG_PKT_RSSI_VALUE) - (ctx->config.frequency < 868E6 ? 164 : 157));
}

float lora_packet_snr(lora_ctx_t *ctx) {
    return ((int8_t)read_register(ctx, REG_PKT_SNR_VALUE)) * 0.25;
}

long lora_packet_frequency_error(lora_ctx_t *ctx) {
    int32_t freq_error = 0;
    freq_error = (int32_t)(read_register(ctx, REG_FREQ_ERROR_MSB) & 0x7);
    freq_error <<= 8L;
    freq_error += (int32_t)(read_register(ctx, REG_FREQ_ERROR_MID));
    freq_error <<= 8L;
    freq_error += (int32_t)(read_register(ctx, REG_FREQ_ERROR_LSB));

    if (read_register(ctx, REG_FREQ_ERROR_MSB) & 0x8) { // Sign bit is on
        freq_error -= 524288; // B1000'0000'0000'0000'0000
    }

    const float fXtal = 32E6; // FXOSC: crystal oscillator (XTAL) frequency (2.5. Chip Specification, p.14)
    const float fError = ((float)(freq_error) * (1L << 24)) / fXtal;

    return (long)(fError * (get_signal_bandwidth(ctx) / 500000.0)); // Given the frequency error in Hz
}

// Write data
size_t lora_write(lora_ctx_t *ctx, const uint8_t *buffer, size_t size) {
    size_t current_length = read_register(ctx, REG_PAYLOAD_LENGTH);

    // Check size
    if ((current_length + size) > MAX_PKT_LENGTH) {
        size = MAX_PKT_LENGTH - current_length;
    }

    // Write data
    for (size_t i = 0; i < size; i++) {
        write_register(ctx, REG_FIFO, buffer[i]);
    }

    // Update length
    write_register(ctx, REG_PAYLOAD_LENGTH, current_length + size);

    return size;
}

size_t lora_write_byte(lora_ctx_t *ctx, uint8_t byte) {
    return lora_write(ctx, &byte, 1);
}

// Read data
int lora_available(lora_ctx_t *ctx) {
    return (read_register(ctx, REG_RX_NB_BYTES) - ctx->packet_index);
}

int lora_read(lora_ctx_t *ctx) {
    if (!lora_available(ctx)) {
        return -1;
    }

    ctx->packet_index++;
    return read_register(ctx, REG_FIFO);
}

int lora_peek(lora_ctx_t *ctx) {
    if (!lora_available(ctx)) {
        return -1;
    }

    // Store current FIFO address
    int current_address = read_register(ctx, REG_FIFO_ADDR_PTR);

    // Read
    uint8_t b = read_register(ctx, REG_FIFO);

    // Restore FIFO address
    write_register(ctx, REG_FIFO_ADDR_PTR, current_address);

    return b;
}

void lora_flush(lora_ctx_t *ctx) {
    // Nothing to do
}

size_t lora_read_bytes(lora_ctx_t *ctx, uint8_t *buffer, size_t size) {
    size_t read_bytes = 0;

    while (read_bytes < size && lora_available(ctx)) {
        buffer[read_bytes++] = lora_read(ctx);
    }

    return read_bytes;
}

// Configuration
void lora_idle(lora_ctx_t *ctx) {
    write_register(ctx, REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
}

void lora_sleep(lora_ctx_t *ctx) {
    write_register(ctx, REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
}

void lora_set_tx_power(lora_ctx_t *ctx, int level, int output_pin) {
    if (PA_OUTPUT_RFO_PIN == output_pin) {
        // RFO
        if (level < 0) {
            level = 0;
        } else if (level > 14) {
            level = 14;
        }

        write_register(ctx, REG_PA_CONFIG, 0x70 | level);
    } else {
        // PA BOOST
        if (level > 17) {
            if (level > 20) {
                level = 20;
            }

            // Subtract 3 from level, so 18 - 20 maps to 15 - 17
            level -= 3;

            // High Power +20 dBm Operation (Semtech SX1276/77/78/79 5.4.3.)
            write_register(ctx, REG_PA_DAC, 0x87);
            lora_set_ocp(ctx, 140);
        } else {
            if (level < 2) {
                level = 2;
            }
            //Default value PA_HF/LF or +17dBm
            write_register(ctx, REG_PA_DAC, 0x84);
            lora_set_ocp(ctx, 100);
        }

        write_register(ctx, REG_PA_CONFIG, PA_BOOST | (level - 2));
    }
}

void lora_set_frequency(lora_ctx_t *ctx, uint32_t frequency) {
    ctx->config.frequency = frequency;

    uint64_t frf = ((uint64_t)frequency << 19) / 32000000;

    write_register(ctx, REG_FRF_MSB, (uint8_t)(frf >> 16));
    write_register(ctx, REG_FRF_MID, (uint8_t)(frf >> 8));
    write_register(ctx, REG_FRF_LSB, (uint8_t)(frf >> 0));
}

void lora_set_spreading_factor(lora_ctx_t *ctx, int sf) {
    if (sf < 6) {
        sf = 6;
    } else if (sf > 12) {
        sf = 12;
    }

    if (sf == 6) {
        write_register(ctx, REG_DETECTION_OPTIMIZE, 0xc5);
        write_register(ctx, REG_DETECTION_THRESHOLD, 0x0c);
    } else {
        write_register(ctx, REG_DETECTION_OPTIMIZE, 0xc3);
        write_register(ctx, REG_DETECTION_THRESHOLD, 0x0a);
    }

    write_register(ctx, REG_MODEM_CONFIG_2, (read_register(ctx, REG_MODEM_CONFIG_2) & 0x0f) | ((sf << 4) & 0xf0));
    set_ldo_flag(ctx);
}

void lora_set_signal_bandwidth(lora_ctx_t *ctx, uint32_t sbw) {
    int bw;

    if (sbw <= 7.8E3) {
        bw = 0;
    } else if (sbw <= 10.4E3) {
        bw = 1;
    } else if (sbw <= 15.6E3) {
        bw = 2;
    } else if (sbw <= 20.8E3) {
        bw = 3;
    } else if (sbw <= 31.25E3) {
        bw = 4;
    } else if (sbw <= 41.7E3) {
        bw = 5;
    } else if (sbw <= 62.5E3) {
        bw = 6;
    } else if (sbw <= 125E3) {
        bw = 7;
    } else if (sbw <= 250E3) {
        bw = 8;
    } else /*if (sbw <= 500E3)*/ {
        bw = 9;
    }

    write_register(ctx, REG_MODEM_CONFIG_1, (read_register(ctx, REG_MODEM_CONFIG_1) & 0x0f) | (bw << 4));
    set_ldo_flag(ctx);
}

void lora_set_coding_rate4(lora_ctx_t *ctx, int denominator) {
    if (denominator < 5) {
        denominator = 5;
    } else if (denominator > 8) {
        denominator = 8;
    }

    int cr = denominator - 4;
    write_register(ctx, REG_MODEM_CONFIG_1, (read_register(ctx, REG_MODEM_CONFIG_1) & 0xf1) | (cr << 1));
}

void lora_set_preamble_length(lora_ctx_t *ctx, uint16_t length) {
    write_register(ctx, REG_PREAMBLE_MSB, (uint8_t)(length >> 8));
    write_register(ctx, REG_PREAMBLE_LSB, (uint8_t)(length >> 0));
}

void lora_set_sync_word(lora_ctx_t *ctx, int sw) {
    write_register(ctx, REG_SYNC_WORD, sw);
}

void lora_enable_crc(lora_ctx_t *ctx) {
    write_register(ctx, REG_MODEM_CONFIG_2, read_register(ctx, REG_MODEM_CONFIG_2) | 0x04);
}

void lora_disable_crc(lora_ctx_t *ctx) {
    write_register(ctx, REG_MODEM_CONFIG_2, read_register(ctx, REG_MODEM_CONFIG_2) & 0xfb);
}

void lora_enable_invert_iq(lora_ctx_t *ctx) {
    write_register(ctx, REG_INVERTIQ, 0x66);
    write_register(ctx, REG_INVERTIQ2, 0x19);
}

void lora_disable_invert_iq(lora_ctx_t *ctx) {
    write_register(ctx, REG_INVERTIQ, 0x27);
    write_register(ctx, REG_INVERTIQ2, 0x1d);
}

// Status
uint8_t lora_random(lora_ctx_t *ctx) {
    return read_register(ctx, REG_RSSI_WIDEBAND);
}

void lora_dump_registers(lora_ctx_t *ctx) {
    for (int i = 0; i < 128; i++) {
        print_str(&ctx->print, "0x");
        print_uchar(&ctx->print, i, HEX);
        print_str(&ctx->print, ": 0x");
        print_uchar(&ctx->print, read_register(ctx, i), HEX);
        println(&ctx->print);
    }
}

float lora_snr(lora_ctx_t *ctx) {
    return ((int8_t)read_register(ctx, REG_PKT_SNR_VALUE)) * 0.25;
}

// Set Over Current Protection (OCP)
void lora_set_ocp(lora_ctx_t *ctx, uint8_t current) {
    uint8_t ocp_trim;

    if (current <= 120) {
        ocp_trim = (current - 45) / 5;
    } else if (current <= 240) {
        ocp_trim = (current + 30) / 10;
    } else {
        ocp_trim = 27;
    }

    write_register(ctx, REG_OCP, 0x20 | (0x1F & ocp_trim));
}

// Low-level SPI
uint8_t lora_single_transfer(lora_ctx_t *ctx, uint8_t address, uint8_t value) {
    uint8_t response;
    uint8_t dummy = 0;
    
    gpio_put(LORA_DEFAULT_SS_PIN, 0);
    
    // Send address
    spi_write_read_blocking(LORA_DEFAULT_SPI_PORT, &address, &response, 1);
    
    // For reads (address & 0x80 == 0), send dummy byte to get response
    // For writes (address & 0x80 == 1), send the value
    if (address & 0x80) {
        spi_write_read_blocking(LORA_DEFAULT_SPI_PORT, &value, &response, 1);
    } else {
        spi_write_read_blocking(LORA_DEFAULT_SPI_PORT, &dummy, &response, 1);
    }
    
    gpio_put(LORA_DEFAULT_SS_PIN, 1);
    return response;
}

// Private functions
static uint8_t read_register(lora_ctx_t *ctx, uint8_t address) {
    return lora_single_transfer(ctx, address & 0x7f, 0x00);
}

static void write_register(lora_ctx_t *ctx, uint8_t address, uint8_t value) {
    lora_single_transfer(ctx, address | 0x80, value);
}

static void write_register_burst(lora_ctx_t *ctx, uint8_t address, const uint8_t *buffer, size_t size) {
    gpio_put(LORA_DEFAULT_SS_PIN, 0);
    address |= 0x80;
    spi_write_blocking(LORA_DEFAULT_SPI_PORT, &address, 1);
    spi_write_blocking(LORA_DEFAULT_SPI_PORT, buffer, size);
    gpio_put(LORA_DEFAULT_SS_PIN, 1);
}

static void read_register_burst(lora_ctx_t *ctx, uint8_t address, uint8_t *buffer, size_t size) {
    gpio_put(LORA_DEFAULT_SS_PIN, 0);
    address &= 0x7f;
    spi_write_blocking(LORA_DEFAULT_SPI_PORT, &address, 1);
    spi_read_blocking(LORA_DEFAULT_SPI_PORT, 0, buffer, size);
    gpio_put(LORA_DEFAULT_SS_PIN, 1);
}

static void set_mode(lora_ctx_t *ctx, uint8_t mode) {
    write_register(ctx, REG_OP_MODE, mode);
}

static void set_ldo_flag(lora_ctx_t *ctx) {
    // Section 4.1.1.5
    uint32_t bandwidth = get_signal_bandwidth(ctx);
    int spreading_factor = get_spreading_factor(ctx);

    // Calculate symbol duration in milliseconds
    uint32_t symbol_duration = (1000 * (1L << spreading_factor)) / (bandwidth / 1000);

    // Section 4.1.1.6
    bool ldo_on = symbol_duration > 16;

    uint8_t config3 = read_register(ctx, REG_MODEM_CONFIG_3);
    if (ldo_on) {
        config3 |= 0x08;
    } else {
        config3 &= ~0x08;
    }
    write_register(ctx, REG_MODEM_CONFIG_3, config3);
}

static void explicit_header_mode(lora_ctx_t *ctx) {
    ctx->implicit_header_mode = 0;
    write_register(ctx, REG_MODEM_CONFIG_1, read_register(ctx, REG_MODEM_CONFIG_1) & 0xfe);
}

static void implicit_header_mode(lora_ctx_t *ctx) {
    ctx->implicit_header_mode = 1;
    write_register(ctx, REG_MODEM_CONFIG_1, read_register(ctx, REG_MODEM_CONFIG_1) | 0x01);
}

static bool is_transmitting(lora_ctx_t *ctx) {
    if ((read_register(ctx, REG_OP_MODE) & MODE_TX) == MODE_TX) {
        return true;
    }

    if (read_register(ctx, REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) {
        // Clear IRQ
        write_register(ctx, REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
    }

    return false;
}

static int get_spreading_factor(lora_ctx_t *ctx) {
    return read_register(ctx, REG_MODEM_CONFIG_2) >> 4;
}

static uint32_t get_signal_bandwidth(lora_ctx_t *ctx) {
    uint8_t bw = (read_register(ctx, REG_MODEM_CONFIG_1) >> 4);
    switch (bw) {
        case 0: return 7.8E3;
        case 1: return 10.4E3;
        case 2: return 15.6E3;
        case 3: return 20.8E3;
        case 4: return 31.25E3;
        case 5: return 41.7E3;
        case 6: return 62.5E3;
        case 7: return 125E3;
        case 8: return 250E3;
        case 9: return 500E3;
    }
    return 0;
} 