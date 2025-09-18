#include <stddef.h> // size_t
#include <stdint.h> // uint16_t
#include <sys/types.h> // ssize_t
#include "imgfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <fcntl.h>
#include <unistd.h>


#define BACK_LOG 6

// creates socket with "sock_id" and binds it with "port" and "INADDR_ANY"
// returns sock_id
int tcp_server_init(uint16_t port)
{
    int sock_id = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_id == -1) {
        perror("Error creating socket");
        return ERR_IO;
    }

    int opt = 1;
    if (setsockopt(sock_id, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Error setting socket options");
        return ERR_IO;
    }
    int flags = fcntl(sock_id, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        close(sock_id);
        return ERR_IO;
    }
    if (flags & O_NONBLOCK) {
        fcntl(sock_id, F_SETFL, flags & ~O_NONBLOCK);
    }

    struct sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);
    sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    int ret = bind(sock_id, (struct sockaddr *)&sockaddr, sizeof(sockaddr));

    if (ret < 0) {
        perror("bind failed");
        close(sock_id);
        return ERR_IO;
    }

    if(listen(sock_id, BACK_LOG) < 0) {
        perror("listen failed");
        close(sock_id);
        return ERR_IO;
    }
    return sock_id;
}

int tcp_accept(int passive_socket)
{
    return accept(passive_socket, NULL, NULL);
}

ssize_t tcp_read(int active_socket, char* buf, size_t buflen)
{
    if(buflen < 0 || active_socket == -1) {     // sock_id == -1 in case of error
        return ERR_INVALID_ARGUMENT;
    }
    return recv(active_socket, buf, buflen, 0);
}

ssize_t tcp_send(int active_socket, const char* response, size_t response_len)
{
    if(response_len < 0 || active_socket == -1) {
        return ERR_INVALID_ARGUMENT;
    }
    return send(active_socket, response, response_len, 0);
}