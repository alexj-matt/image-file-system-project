/*
 * @file imgfs_server_services.c
 * @brief ImgFS server part, bridge between HTTP server layer and ImgFS library
 *
 * @author Konstantinos Prasopoulos
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // uint16_t
#include <pthread.h>
#include <vips/vips.h>

#include "error.h"
#include "util.h" // atouint16
#include "imgfs.h"
#include "http_net.h"
#include "imgfs_server_service.h"

// Main in-memory structure for imgFS
static struct imgfs_file fs_file;
static uint16_t server_port;
static pthread_mutex_t mut;

#define URI_ROOT "/imgfs"

/********************************************************************//**
 * Startup function. Create imgFS file and load in-memory structure.
 * Pass the imgFS file name as argv[1] and optionnaly port number as argv[2]
 ********************************************************************** */
int server_startup (int argc, char **argv)
{
    if (argc < 2) {
        printf("Specify imgfs file.\n");
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    if (VIPS_INIT(argv[0])) {
        vips_error_exit("unable to start VIPS");
    }

    if (pthread_mutex_init(&mut, NULL)) {
        perror("Error initializing mutex");
        return ERR_THREADING;
    }
    pthread_mutex_lock(&mut);
    int ret = do_open(argv[1], "rb+", &fs_file);
    pthread_mutex_unlock(&mut);
    if(ret) {
        return ret;
    }
    print_header(&fs_file.header);

    server_port = DEFAULT_LISTENING_PORT;
    if (argc > 2 && atoi(argv[2]) > 0) {
        server_port = atoi(argv[2]);
    }

    // sets handle_http_message as CallBack function
    http_init(server_port, handle_http_message);
    printf("\"ImgFS server started on http://localhost:%d\"\n", server_port);
    return ERR_NONE;
}

/********************************************************************//**
 * Shutdown function. Free the structures and close the file.
 ********************************************************************** */
void server_shutdown (void)
{
    fprintf(stderr, "Shutting down...\n");
    http_close();
    pthread_mutex_lock(&mut);
    do_close(&fs_file);

    pthread_mutex_unlock(&mut);
    pthread_mutex_destroy(&mut);

    vips_shutdown();
}
/**********************************************************************
 * Sends error message.
 ********************************************************************** */
static int reply_error_msg(int connection, int error)
{
#define ERR_MSG_SIZE 256
    char err_msg[ERR_MSG_SIZE]; // enough for any reasonable err_msg
    if (snprintf(err_msg, ERR_MSG_SIZE, "Error: %s\n", ERR_MSG(error)) < 0) {
        fprintf(stderr, "reply_error_msg(): sprintf() failed...\n");
        return ERR_RUNTIME;
    }
    return http_reply(connection, "500 Internal Server Error", "",
                      err_msg, strlen(err_msg));
}

/**********************************************************************
 * Sends 302 OK message.
 ********************************************************************** */
static int reply_302_msg(int connection)
{
    char location[ERR_MSG_SIZE];
    if (snprintf(location, ERR_MSG_SIZE, "Location: http://localhost:%d/" BASE_FILE HTTP_LINE_DELIM,
                 server_port) < 0) {
        fprintf(stderr, "reply_302_msg(): sprintf() failed...\n");
        return ERR_RUNTIME;
    }
    return http_reply(connection, "302 Found", location, "", 0);
}

/**********************************************************************
 * Simple handling of http message. TO BE UPDATED WEEK 13
 ********************************************************************** */
int handle_http_message(struct http_message* msg, int connection)
{
    M_REQUIRE_NON_NULL(msg);
    debug_printf("handle_http_message() on connection %d. URI: %.*s\n",
                 connection,
                 (int) msg->uri.len, msg->uri.val);
    if (http_match_verb(&msg->uri, "/") || http_match_uri(msg, "/index.html")) {
        return http_serve_file(connection, BASE_FILE);
    }

    if (http_match_uri(msg, URI_ROOT "/list")) {
        return handle_list_call(msg, connection);
    } else if (http_match_uri(msg, URI_ROOT "/insert") && http_match_verb(&msg->method, "POST")) {
        return handle_insert_call(msg, connection);
    } else if (http_match_uri(msg, URI_ROOT "/read")) {
        return handle_read_call(msg, connection);
    } else if (http_match_uri(msg, URI_ROOT "/delete")) {
        return handle_delete_call(msg, connection);
    } else {
        return reply_error_msg(connection, ERR_INVALID_COMMAND);
    }
}

int handle_list_call(struct http_message* msg, int connection)
{
    if(msg == NULL) {
        return reply_error_msg(connection, ERR_INVALID_ARGUMENT);
    }

    char* json = NULL;
    pthread_mutex_lock(&mut);
    int ret = do_list(&fs_file, JSON, &json);
    pthread_mutex_unlock(&mut);

    if (ret) {
        return reply_error_msg(connection, ret);
    }

    // header information, additional to    HTTP_PROTOCOL_ID -> HTTP/1.1    (added in http_reply)
    //                                      status                          (given as arg)
    //                                      Content-Length                  (given as arg)
    // Content-Type: application/json\r\n

    char* add_header = "Content-Type: application/json\r\n";
    int i =http_reply(connection, HTTP_OK, add_header, json, strlen(json));
    free(json);
    return i;
}

int handle_read_call(struct http_message* msg, int connection)
{
    const int RES_SIZE = 10;
    char res[RES_SIZE];
    memset(res, 0, sizeof(RES_SIZE));
    int ret = http_get_var(&(msg->uri), "res", res, RES_SIZE);
    if (ret <= 0) {
        return reply_error_msg(connection, ERR_NOT_ENOUGH_ARGUMENTS);
    }

    int resolution;
    if (strncmp(res, "orig",4) == 0 || strncmp(res, "original",8) == 0) {
        resolution = ORIG_RES;
    } else if (strncmp(res, "small",5) == 0) {
        resolution = SMALL_RES;
    } else if (strncmp(res, "thumb",5) == 0 || strncmp(res, "thumbnail",9) == 0 ) {
        resolution = THUMB_RES;
    } else {
        resolution = -1;
    }

    if (resolution == -1) {
        return reply_error_msg(connection, ERR_RESOLUTIONS);
    }

    char img_id[MAX_IMG_ID];
    memset(img_id, 0, MAX_IMG_ID);
    ret = http_get_var(&(msg->uri), "img_id", img_id, MAX_IMG_ID);
    if (ret <= 0) {
        return reply_error_msg(connection, ERR_NOT_ENOUGH_ARGUMENTS);
    }

    char* image_buffer = NULL;
    uint32_t image_size = 0;
    pthread_mutex_lock(&mut);
    ret = do_read(img_id, resolution, &image_buffer, &image_size, &fs_file);
    pthread_mutex_unlock(&mut);
    if (ret) {
        free(image_buffer);
        return reply_error_msg(connection, ret);
    }

    char* add_header = "Content-Type: image/jpeg\r\n";
    int i = http_reply(connection, HTTP_OK, add_header, image_buffer, image_size);
    free(image_buffer);
    return i;
}

int handle_delete_call(struct http_message* msg, int connection)
{
    char img_id[MAX_IMG_ID];
    memset(img_id, 0, MAX_IMG_ID);
    int ret = http_get_var(&(msg->uri), "img_id", img_id, MAX_IMG_ID);
    if (ret <= 0) {
        return reply_error_msg(connection, ERR_NOT_ENOUGH_ARGUMENTS);
    }

    pthread_mutex_lock(&mut);
    ret = do_delete(img_id, &fs_file);
    pthread_mutex_unlock(&mut);
    if (ret) {
        return reply_error_msg(connection, ret);
    }

    return reply_302_msg(connection);   // URL is in our case always localhost? Otherwise change (custom function)
}

int handle_insert_call(struct http_message* msg, int connection)
{

    if (msg->body.val == NULL || msg->body.len <= 0) {
        return reply_error_msg(connection, ERR_INVALID_COMMAND);
    }

    char name[MAX_IMGFS_NAME];
    memset(name, 0, MAX_IMGFS_NAME);
    int ret = http_get_var(&(msg->uri), "name", name, MAX_IMGFS_NAME);
    if (ret <= 0) {
        return reply_error_msg(connection, ERR_NOT_ENOUGH_ARGUMENTS);
    }

    char* image_buffer = calloc(1, msg->body.len);
    memcpy(image_buffer, msg->body.val, msg->body.len);

    pthread_mutex_lock(&mut);
    ret = do_insert(image_buffer, msg->body.len, name, &fs_file);
    pthread_mutex_unlock(&mut);
    free(image_buffer);
    if (ret) {
        return reply_error_msg(connection, ret);
    }

    return reply_302_msg(connection);   // URL is in our case always localhost? Otherwise change (custom function)
}
