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
#define SERVER_PORT 443

void https_send_beacon(uint8_t *beacon, size_t beacon_len, uint8_t *task_out, size_t *task_len) {
    printf("sending beacon, len: %zu\n", beacon_len);printf("sending beacon, len: %zu\n", beacon_len);
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    server.sin_addr.s_addr = inet_addr(SERVER_IP);

    connect(sock, (struct sockaddr*)&server, sizeof(server));

    // build HTTP POST request
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "POST /beacon HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        SERVER_IP, beacon_len);

    send(sock, header, header_len, 0);
    send(sock, beacon, beacon_len, 0);

    // read response
    uint8_t response[4096];
    int received = recv(sock, response, sizeof(response) - 1, 0);
    response[received] = 0;

    // skip HTTP headers, get body
    uint8_t *body = (uint8_t*)strstr((char*)response, "\r\n\r\n");
    if (body) {
        body += 4;
        *task_len = received - (body - response);
        memcpy(task_out, body, *task_len);
    }

#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}