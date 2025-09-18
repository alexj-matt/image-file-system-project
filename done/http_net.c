/*
 * @file http_net.c
 * @brief HTTP server layer for CS-202 project
 *
 * @author Konstantinos Prasopoulos
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <signal.h>
#include <pthread.h>

#include "http_prot.h"
#include "http_net.h"
#include "socket_layer.h"
#include "error.h"

static int passive_socket = -1;
static EventCallback cb;

#define MK_OUR_ERR(X) \
static int our_ ## X = X

MK_OUR_ERR(ERR_NONE);
MK_OUR_ERR(ERR_INVALID_ARGUMENT);
MK_OUR_ERR(ERR_OUT_OF_MEMORY);
MK_OUR_ERR(ERR_IO);

/*******************************************************************
 * Handle connection, multithreaded (detached --> return values dont matter --> simple "return NULL;")
 * arg is the socket file descriptor (on heap)
 * handle_connection in charge of closing tcp socket and freeing socket file descriptor on heap
 */
static void *handle_connection(void *arg)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT );
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    if (arg == NULL) return &our_ERR_INVALID_ARGUMENT;

    int active_socket = *(int*)arg;

    char* rcvbuf = calloc(1, MAX_HEADER_SIZE);
    if(rcvbuf == NULL) {
        close(active_socket);
        return NULL;
    }
    ssize_t read_ = 0;
    ssize_t r_total = 0;
    struct http_message message;
    int content_length = 0;
    int extended = 0;
    while(1) {
        read_ = tcp_read(active_socket, rcvbuf + r_total, (size_t)MAX_HEADER_SIZE + (size_t)content_length - (size_t)r_total);
        // connection abandoned or recv error
        if (read_ <= 0) {
            if (close(active_socket) == -1) {
                perror("Error in close() of active_socket");
            }
            free(rcvbuf);
            free(arg);
            return NULL;
        }
        r_total += read_;

        int ret = http_parse_message(rcvbuf, (size_t)r_total, &message, &content_length);

        // case: problem
        if(ret < 0) {
            free(rcvbuf);
            if (close(active_socket) == -1) {
                perror("Error in close() of active_socket");
            }
            free(arg);
            return NULL;
        }

        // case: extended but still need a few reads
        if (extended && message.body.len < content_length) {
            continue;
        }

        // case: allocate more memory
        if (ret == 0 && !extended && content_length > 0) {
            extended = 1;
            char* new_rcvbuf = realloc(rcvbuf, (unsigned long)MAX_HEADER_SIZE + (unsigned long)content_length);
            if (new_rcvbuf == NULL) {
                free(rcvbuf);
                if (close(active_socket) == -1) {
                    perror("Error in close() of active_socket");
                }
                free(arg);
                return NULL;
            }
            rcvbuf = new_rcvbuf;
            memset(rcvbuf + MAX_HEADER_SIZE, 0, content_length);
        }

        // case: message fully received, now process it on our end (server side)
        if (ret > 0) {
            cb(&message, active_socket);
            read_ = 0;
            r_total = 0;
            content_length = 0;
            extended = 0;
            free(rcvbuf);
            rcvbuf = calloc(1, MAX_HEADER_SIZE);
            if (rcvbuf == NULL) {
                if (close(active_socket) == -1) {
                    perror("Error in close() of active_socket");
                }
                free(arg);
                return NULL;
            }
        }
    }

    if (close(active_socket) == -1) {
        perror("Error in close() of active_socket");
    }

    free(arg);
    return NULL;
}


/*******************************************************************
 * Init connection
 */
int http_init(uint16_t port, EventCallback callback)
{
    passive_socket = tcp_server_init(port);
    cb = callback;
    return passive_socket;
}

/*******************************************************************
 * Close connection
 */
void http_close(void)
{
    if (passive_socket > 0) {
        if (close(passive_socket) == -1)
            perror("close() in http_close()");
        else
            passive_socket = -1;
    }
}

/*******************************************************************
 * Receive content
 */
int http_receive(void)
{
    // part 1: connect to socket with tcp_accept
    int* active_socket = calloc(1, sizeof(int));
    if(active_socket == NULL) {
        return ERR_OUT_OF_MEMORY;
    }
    *active_socket = tcp_accept(passive_socket);
    if(*active_socket < 0) {
        free(active_socket);
        return ERR_IO;
    }

    // part 2: tool function handle_connection
    pthread_attr_t attr;
    if (pthread_attr_init(&attr)) {
        perror("Error initializing thread attribute");
        free(active_socket);
        return ERR_THREADING;
    }

    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) {
        perror("Error setting thread attribute");
        pthread_attr_destroy(&attr);
        free(active_socket);
        return ERR_THREADING;
    }

    pthread_t thread;
    if (pthread_create(&thread, &attr, handle_connection, active_socket)) {
        perror("Error creating thread");
        pthread_attr_destroy(&attr);
        free(active_socket);
        return ERR_THREADING;
    }

    return ERR_NONE;
}

/*******************************************************************
 * Serve a file content over HTTP
 */
int http_serve_file(int connection, const char* filename)
{
    M_REQUIRE_NON_NULL(filename);

    // open file
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "http_serve_file(): Failed to open file \"%s\"\n", filename);
        return http_reply(connection, "404 Not Found", "", "", 0);
    }

    // get its size
    fseek(file, 0, SEEK_END);
    const long pos = ftell(file);
    if (pos < 0) {
        fprintf(stderr, "http_serve_file(): Failed to tell file size of \"%s\"\n",
                filename);
        fclose(file);
        return ERR_IO;
    }
    rewind(file);
    const size_t file_size = (size_t) pos;

    // read file content
    char* const buffer = calloc(file_size + 1, 1);
    if (buffer == NULL) {
        fprintf(stderr, "http_serve_file(): Failed to allocate memory to serve \"%s\"\n", filename);
        fclose(file);
        return ERR_IO;
    }

    const size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != file_size) {
        fprintf(stderr, "http_serve_file(): Failed to read \"%s\"\n", filename);
        fclose(file);
        return ERR_IO;
    }

    // send the file
    const int  ret = http_reply(connection, HTTP_OK,
                                "Content-Type: text/html; charset=utf-8" HTTP_LINE_DELIM,
                                buffer, file_size);

    // garbage collecting
    fclose(file);
    free(buffer);
    return ret;
}


/*******************************************************************
 * Create and send HTTP reply
 */
int http_reply(int connection, const char* status, const char* headers, const char *body, size_t body_len)
{
    M_REQUIRE_NON_NULL(status);
    M_REQUIRE_NON_NULL(headers);
    if (body_len > 0) {
        M_REQUIRE_NON_NULL(body);
    }

    char header[MAX_HEADER_SIZE];
    memset(header, 0, sizeof(MAX_HEADER_SIZE));
    int header_len = snprintf(header, sizeof(header), "%s%s%s%sContent-Length: %d%s",
                              HTTP_PROTOCOL_ID, status, HTTP_LINE_DELIM, headers, body_len, HTTP_HDR_END_DELIM);

    int msg_len = header_len + body_len;
    char* buffer = (char*)calloc(msg_len, 1);

    if (buffer == NULL) {
        return ERR_OUT_OF_MEMORY;
    }

    if (memcpy(buffer, header, header_len) == NULL) {
        free(buffer);
        return ERR_IO;
    }

    if (body_len > 0) {
        if (memcpy(buffer + header_len, body, body_len) == NULL) {
            free(buffer);
            return ERR_IO;
        }
    }

    ssize_t ret = tcp_send(connection, buffer, msg_len);
    free(buffer);
    if (ret != msg_len) {   // strict statement but should be true
        return ERR_IO;
    }

    return ERR_NONE;
}
