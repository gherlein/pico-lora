#include "print.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

// Private function declarations
static size_t print_number(print_ctx_t *ctx, unsigned long n, uint8_t base);
static size_t print_ull_number(print_ctx_t *ctx, unsigned long long n64, uint8_t base);
static size_t print_float(print_ctx_t *ctx, double number, int digits);

// Stdout write functions
static size_t stdout_write_byte(print_ctx_t *ctx, uint8_t b) {
    if (putchar(b) == EOF) {
        ctx->write_error = 1;
        return 0;
    }
    return 1;
}

static size_t stdout_write_buffer(print_ctx_t *ctx, const uint8_t *buffer, size_t size) {
    size_t n = 0;
    while (size--) {
        if (putchar(*buffer++) == EOF) {
            ctx->write_error = 1;
            break;
        }
        n++;
    }
    return n;
}

static int stdout_available(print_ctx_t *ctx) {
    return 1;  // Always available for stdout
}

static void stdout_flush(print_ctx_t *ctx) {
    fflush(stdout);
}

// Initialize print context
void print_init(print_ctx_t *ctx) {
    ctx->write_error = 0;
    ctx->write_byte = stdout_write_byte;
    ctx->write_buffer = stdout_write_buffer;
    ctx->available_for_write = stdout_available;
    ctx->flush = stdout_flush;
}

// Error handling
int print_get_write_error(print_ctx_t *ctx) {
    return ctx->write_error;
}

void print_clear_write_error(print_ctx_t *ctx) {
    ctx->write_error = 0;
}

// Write functions
size_t print_write_str(print_ctx_t *ctx, const char *str) {
    if (str == NULL) return 0;
    return print_write_buffer(ctx, (const uint8_t *)str, strlen(str));
}

size_t print_write_buffer(print_ctx_t *ctx, const uint8_t *buffer, size_t size) {
    return ctx->write_buffer(ctx, buffer, size);
}

size_t print_write_char_buffer(print_ctx_t *ctx, const char *buffer, size_t size) {
    return print_write_buffer(ctx, (const uint8_t *)buffer, size);
}

// Print functions
size_t print_char(print_ctx_t *ctx, char c) {
    return ctx->write_byte(ctx, (uint8_t)c);
}

size_t print_str(print_ctx_t *ctx, const char *str) {
    return print_write_str(ctx, str);
}

size_t print_uchar(print_ctx_t *ctx, unsigned char b, int base) {
    return print_ulong(ctx, (unsigned long)b, base);
}

size_t print_int(print_ctx_t *ctx, int n, int base) {
    return print_long(ctx, (long)n, base);
}

size_t print_uint(print_ctx_t *ctx, unsigned int n, int base) {
    return print_ulong(ctx, (unsigned long)n, base);
}

size_t print_long(print_ctx_t *ctx, long n, int base) {
    if (base == 0) {
        return ctx->write_byte(ctx, (uint8_t)n);
    } else if (base == 10) {
        if (n < 0) {
            size_t t = print_char(ctx, '-');
            n = -n;
            return print_number(ctx, n, 10) + t;
        }
        return print_number(ctx, n, 10);
    } else {
        return print_number(ctx, n, base);
    }
}

size_t print_ulong(print_ctx_t *ctx, unsigned long n, int base) {
    if (base == 0) return ctx->write_byte(ctx, (uint8_t)n);
    else return print_number(ctx, n, base);
}

size_t print_longlong(print_ctx_t *ctx, long long n, int base) {
    if (base == 0) {
        return ctx->write_byte(ctx, (uint8_t)n);
    } else if (base == 10) {
        if (n < 0) {
            size_t t = print_char(ctx, '-');
            n = -n;
            return print_ull_number(ctx, n, 10) + t;
        }
        return print_ull_number(ctx, n, 10);
    } else {
        return print_ull_number(ctx, n, base);
    }
}

size_t print_ulonglong(print_ctx_t *ctx, unsigned long long n, int base) {
    if (base == 0) return ctx->write_byte(ctx, (uint8_t)n);
    else return print_ull_number(ctx, n, base);
}

size_t print_double(print_ctx_t *ctx, double n, int digits) {
    return print_float(ctx, n, digits);
}

// Println functions
size_t println(print_ctx_t *ctx) {
    return print_write_str(ctx, "\r\n");
}

size_t println_str(print_ctx_t *ctx, const char *str) {
    size_t n = print_str(ctx, str);
    n += println(ctx);
    return n;
}

size_t println_char(print_ctx_t *ctx, char c) {
    size_t n = print_char(ctx, c);
    n += println(ctx);
    return n;
}

size_t println_uchar(print_ctx_t *ctx, unsigned char b, int base) {
    size_t n = print_uchar(ctx, b, base);
    n += println(ctx);
    return n;
}

size_t println_int(print_ctx_t *ctx, int num, int base) {
    size_t n = print_int(ctx, num, base);
    n += println(ctx);
    return n;
}

size_t println_uint(print_ctx_t *ctx, unsigned int num, int base) {
    size_t n = print_uint(ctx, num, base);
    n += println(ctx);
    return n;
}

size_t println_long(print_ctx_t *ctx, long num, int base) {
    size_t n = print_long(ctx, num, base);
    n += println(ctx);
    return n;
}

size_t println_ulong(print_ctx_t *ctx, unsigned long num, int base) {
    size_t n = print_ulong(ctx, num, base);
    n += println(ctx);
    return n;
}

size_t println_longlong(print_ctx_t *ctx, long long num, int base) {
    size_t n = print_longlong(ctx, num, base);
    n += println(ctx);
    return n;
}

size_t println_ulonglong(print_ctx_t *ctx, unsigned long long num, int base) {
    size_t n = print_ulonglong(ctx, num, base);
    n += println(ctx);
    return n;
}

size_t println_double(print_ctx_t *ctx, double num, int digits) {
    size_t n = print_double(ctx, num, digits);
    n += println(ctx);
    return n;
}

// Private functions
static size_t print_number(print_ctx_t *ctx, unsigned long n, uint8_t base) {
    char buf[8 * sizeof(long) + 1]; // Assumes 8-bit chars plus zero byte.
    char *str = &buf[sizeof(buf) - 1];

    *str = '\0';

    // prevent crash if called with base == 1
    if (base < 2) base = 10;

    do {
        char c = n % base;
        n /= base;

        *--str = c < 10 ? c + '0' : c + 'A' - 10;
    } while(n);

    return print_write_str(ctx, str);
}

static size_t print_ull_number(print_ctx_t *ctx, unsigned long long n64, uint8_t base) {
    char buf[64];
    uint8_t i = 0;
    uint8_t innerLoops = 0;

    // prevent crash if called with base == 1
    if (base < 2) base = 10;

    // process chunks that fit in "16 bit math"
    uint16_t top = 0xFFFF / base;
    uint16_t th16 = 1;
    while (th16 < top) {
        th16 *= base;
        innerLoops++;
    }

    while (n64 > th16) {
        // 64 bit math part
        uint64_t q = n64 / th16;
        uint16_t r = n64 - q*th16;
        n64 = q;

        // 16 bit math loop to do remainder
        for (uint8_t j=0; j < innerLoops; j++) {
            uint16_t qq = r/base;
            buf[i++] = r - qq*base;
            r = qq;
        }
    }

    uint16_t n16 = n64;
    while (n16 > 0) {
        uint16_t qq = n16/base;
        buf[i++] = n16 - qq*base;
        n16 = qq;
    }

    size_t bytes = i;
    for (; i > 0; i--) {
        char c = (buf[i - 1] < 10) ? ('0' + buf[i - 1]) : ('A' + buf[i - 1] - 10);
        print_char(ctx, c);
    }

    return bytes;
}

static size_t print_float(print_ctx_t *ctx, double number, int digits) {
    if (digits < 0) digits = 2;

    size_t n = 0;

    if (isnan(number)) return print_str(ctx, "nan");
    if (isinf(number)) return print_str(ctx, "inf");
    if (number > 4294967040.0) return print_str(ctx, "ovf");
    if (number < -4294967040.0) return print_str(ctx, "ovf");

    // Handle negative numbers
    if (number < 0.0) {
        n += print_char(ctx, '-');
        number = -number;
    }

    // Round correctly so that print(1.999, 2) prints as "2.00"
    double rounding = 0.5;
    for (uint8_t i=0; i<digits; ++i)
        rounding /= 10.0;

    number += rounding;

    // Extract the integer part of the number and print it
    unsigned long int_part = (unsigned long)number;
    double remainder = number - (double)int_part;
    n += print_ulong(ctx, int_part, 10);

    // Print the decimal point, but only if there are digits beyond
    if (digits > 0) {
        n += print_char(ctx, '.');
    }

    // Extract digits from the remainder one at a time
    while (digits-- > 0) {
        remainder *= 10.0;
        unsigned int toPrint = (unsigned int)remainder;
        n += print_uint(ctx, toPrint, 10);
        remainder -= toPrint;
    }

    return n;
} 