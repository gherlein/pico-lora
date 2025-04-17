#ifndef PRINT_H
#define PRINT_H

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

// Base number formats
#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

// Print context structure
typedef struct print_ctx {
    int write_error;
    // Virtual functions
    size_t (*write_byte)(struct print_ctx *ctx, uint8_t b);
    size_t (*write_buffer)(struct print_ctx *ctx, const uint8_t *buffer, size_t size);
    int (*available_for_write)(struct print_ctx *ctx);
    void (*flush)(struct print_ctx *ctx);
} print_ctx_t;

// Initialize print context
void print_init(print_ctx_t *ctx);

// Error handling
int print_get_write_error(print_ctx_t *ctx);
void print_clear_write_error(print_ctx_t *ctx);

// Write functions
size_t print_write_str(print_ctx_t *ctx, const char *str);
size_t print_write_buffer(print_ctx_t *ctx, const uint8_t *buffer, size_t size);
size_t print_write_char_buffer(print_ctx_t *ctx, const char *buffer, size_t size);

// Print functions
size_t print_char(print_ctx_t *ctx, char c);
size_t print_str(print_ctx_t *ctx, const char *str);
size_t print_uchar(print_ctx_t *ctx, unsigned char b, int base);
size_t print_int(print_ctx_t *ctx, int n, int base);
size_t print_uint(print_ctx_t *ctx, unsigned int n, int base);
size_t print_long(print_ctx_t *ctx, long n, int base);
size_t print_ulong(print_ctx_t *ctx, unsigned long n, int base);
size_t print_longlong(print_ctx_t *ctx, long long n, int base);
size_t print_ulonglong(print_ctx_t *ctx, unsigned long long n, int base);
size_t print_double(print_ctx_t *ctx, double n, int digits);

// Println functions
size_t println(print_ctx_t *ctx);
size_t println_str(print_ctx_t *ctx, const char *str);
size_t println_char(print_ctx_t *ctx, char c);
size_t println_uchar(print_ctx_t *ctx, unsigned char b, int base);
size_t println_int(print_ctx_t *ctx, int n, int base);
size_t println_uint(print_ctx_t *ctx, unsigned int n, int base);
size_t println_long(print_ctx_t *ctx, long n, int base);
size_t println_ulong(print_ctx_t *ctx, unsigned long n, int base);
size_t println_longlong(print_ctx_t *ctx, long long n, int base);
size_t println_ulonglong(print_ctx_t *ctx, unsigned long long n, int base);
size_t println_double(print_ctx_t *ctx, double n, int digits);

#endif // PRINT_H 