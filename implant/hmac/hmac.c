#include <stdint.h>
#include <string.h>
#include "sha256.h"
#include <stdio.h>

void SignBeacon(uint8_t key[32], uint8_t *data, size_t data_len, uint8_t out[32]){
    uint8_t inner_key[64];
    uint8_t outer_key[64];
    memset(inner_key, 0, 64);
    memset(outer_key, 0, 64);
    memcpy(inner_key, key, 32);
    memcpy(outer_key, key, 32);
    for(int i=0;i<64;i++){
        inner_key[i] ^= 0x36;
        outer_key[i] ^= 0x5c;
    }
    uint8_t inner_hash[32];
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, inner_key, 64);
    sha256_update(&ctx, data, data_len);
    sha256_final(&ctx, inner_hash);
    SHA256_CTX ctx1;
    sha256_init(&ctx1);
    sha256_update(&ctx1, outer_key, 64);
    sha256_update(&ctx1, inner_hash, 32);
    sha256_final(&ctx1, out);
}

int VerifyBeacon(uint8_t key[32], uint8_t *data, size_t data_len, uint8_t sig[32]){
    uint8_t expected[32];
    SignBeacon(key,data,data_len,expected);
    return memcmp(expected, sig ,32) == 0;
}