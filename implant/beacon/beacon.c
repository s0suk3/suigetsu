#include <string.h>
#include <stdint.h>
#include <stdlib.h>

struct Beacon
{
    uint8_t implant_id[16];
    uint8_t type;
    uint32_t data_len;
    uint8_t *data;
};

uint8_t* serialize(struct Beacon *b, size_t *out_len) {
    if (!b || !out_len) return NULL;
    uint8_t *buf = (uint8_t*)malloc(21 + b->data_len);
    if (!buf) return NULL;

    memcpy(buf, b->implant_id, 16);
    buf[16] = b->type;
    buf[17] = (b->data_len >> 24) & 0xff;
    buf[18] = (b->data_len >> 16) & 0xff;
    buf[19] = (b->data_len >> 8)  & 0xff;
    buf[20] = (b->data_len)       & 0xff;

    if (b->data_len > 0 && b->data) {
        memcpy(buf + 21, b->data, b->data_len);
    }
    *out_len = 21 + b->data_len;
    return buf;
}

struct Beacon* parse(uint8_t *data, size_t len) {
    if (!data || len < 21) return NULL;

    uint32_t data_len = ((uint32_t)data[17] << 24) |
                        ((uint32_t)data[18] << 16) |
                        ((uint32_t)data[19] << 8)  |
                        ((uint32_t)data[20]);

    if (len < 21 + (size_t)data_len) return NULL;

    struct Beacon *b = (struct Beacon*)malloc(sizeof(struct Beacon));
    if (!b) return NULL;

    memcpy(b->implant_id, data, 16);
    b->type = data[16];
    b->data_len = data_len;

    if (data_len > 0) {
        b->data = (uint8_t*)malloc(data_len);
        if (!b->data) {
            free(b);
            return NULL;
        }
        memcpy(b->data, data + 21, data_len);
    } else {
        b->data = NULL;
    }

    return b;
}
