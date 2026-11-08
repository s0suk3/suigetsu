#define CHACHA20_IMPLEMENTATION
#include "chacha20.h"
#include "sha256.h"
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #define _POSIX_C_SOURCE 200809L
    #include <time.h>
#endif

static uint8_t key_obf[32] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
                                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                                0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20 };

static uint8_t key_mask[32] = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22,
                                 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
                                 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22,
                                 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00 };

void get_key(uint8_t key[32]) {
    for (int i = 0; i < 32; i++)
        key[i] = key_obf[i] ^ key_mask[i];
    memset(key_obf, 0, 32);
    memset(key_mask, 0, 32);
}

uint64_t get_counter(uint64_t window_size) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    t -= 116444736000000000ULL;
    t *= 100;
    return t / window_size;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t nanos = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    return nanos / window_size;
#endif
}

uint8_t get_protocol(uint8_t key[32], uint64_t counter) {
    uint8_t nonce[12] = {0};
    nonce[4]  = (counter >> 56) & 0xff;
    nonce[5]  = (counter >> 48) & 0xff;
    nonce[6]  = (counter >> 40) & 0xff;
    nonce[7]  = (counter >> 32) & 0xff;
    nonce[8]  = (counter >> 24) & 0xff;
    nonce[9]  = (counter >> 16) & 0xff;
    nonce[10] = (counter >> 8)  & 0xff;
    nonce[11] = (counter)       & 0xff;
    ChaCha20_Ctx ctx;
    ChaCha20_init(&ctx, key, nonce, 0);
    uint8_t result = 0;
    ChaCha20_xor(&ctx, &result, 1);
    ChaCha20_clear(&ctx);

    return result % 4;
}