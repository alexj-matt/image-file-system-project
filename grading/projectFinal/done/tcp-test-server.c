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
#include <stdio.h>
#include "socket_layer.h"

int main(int argc, const char** argv)
{
    printf("Starting the server \n");
    int port = atoi(argv[1]);
    int listening_sock = tcp_server_init((uint16_t)port);
    printf("Sever started on port %d \n", port);
    while(1) {
        printf("Waiting for a size \n");
        int connected_sock = tcp_accept(listening_sock);

        char resp_buff[5];
        tcp_read(connected_sock, resp_buff, 5);
        int size = atoi(resp_buff);
        if(size) {
            if(size<1024) {
                printf("Received a size: %d --> accepted \n", size);
                tcp_send(connected_sock, "1", 2);
                char file_buf[size + 1];
                printf("About to receive file of %d bytes \n", size);
                tcp_read(connected_sock, file_buf, (size_t)size + 1);
                printf("Received a file: \n");
                printf("%s \n",file_buf);
                tcp_send(connected_sock, "1", 2);
            } else {
                printf("Received a size: %d --> rejected \n", size);
                tcp_send(connected_sock, "0", 2);
                close(connected_sock);
                break;
            }
        }
    }
    close(listening_sock);
}
