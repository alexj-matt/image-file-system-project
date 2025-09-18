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

#define BUFFER_SIZE 1024
#define MAX_FILE_SIZE 2048
int main(int argc, char** argv)
{
    //port are u_int16, socket are int
    uint16_t port = atoi(argv[1]);
    char* file_name = argv[2];
    int active_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(connect(active_socket, (struct sockaddr*)&server_addr, sizeof(server_addr))) {
        printf("Unable to connect\n");
        return -1;
    }
    printf("Talking to %d \n", port);

    FILE* file = fopen(file_name, "rb");
    if (file == NULL) {
        return ERR_IO;
    }
    fseek(file, 0, SEEK_END);
    int size = (int)ftell(file);
    if(size >= MAX_FILE_SIZE) {
        return ERR_IO;
    }
    fseek(file, 0, SEEK_SET);

    char size_str[5];   // 2048 is max size --> + 0 byte
    sprintf(size_str, "%d", size);

    printf("Sending size %s: \n", size_str);
    int ret = (int)tcp_send(active_socket, size_str, sizeof(size_str));
    if(ret == -1) {
        printf("Unable to send\n");
    }
    //wait for ack
    char ack_buff [2];
    ret = (int)tcp_read(active_socket, ack_buff, 2);
    if(ret == -1) {
        printf("Unable to read\n");
    }
    printf("Server responded: \"%s\" \n", ack_buff);

    if(atoi(ack_buff)) {
        char file_buff[size + 1];
        file_buff[size] = '\0';
        ret = (int)fread(file_buff, (unsigned long)size, 1, file);
        printf("Sending %s: \n", argv[2]);
        ret = (int)tcp_send(active_socket, file_buff, (size_t)size + 1);
        if (ret == -1) {
            printf("Unable to send\n");
        }
    }
    return ERR_NONE;
}
