#ifdef IN_CS202_UNIT_TEST
#define static_unless_test
#else
#define static_unless_test static
#endif

#include <stdio.h>
#include <string.h>
#include "http_prot.h"
#include "error.h"
#include "error.h"
#include <stdlib.h>

// returns 1 on match, 0 otherwise
int http_match_uri(const struct http_message *message, const char *target_uri)
{
    M_REQUIRE_NON_NULL(target_uri);
    M_REQUIRE_NON_NULL(message);
    if(strlen(target_uri) > message->uri.len) {
        return 0;
    }
    return !strncmp(message->uri.val, target_uri, strlen(target_uri));
}

int http_match_verb(const struct http_string* method, const char* verb)
{
    M_REQUIRE_NON_NULL(verb);
    M_REQUIRE_NON_NULL(method);
    if(strlen(verb) != method->len) {
        return 0;
    }
    return !strncmp(method->val, verb, method->len);
}

int http_get_var(const struct http_string* url, const char* name, char* out, size_t out_len)
{
    M_REQUIRE_NON_NULL(url);
    M_REQUIRE_NON_NULL(name);
    M_REQUIRE_NON_NULL(out);
    if (out_len <= 0) {
        return ERR_INVALID_ARGUMENT;
    }
    size_t name_len = strlen(name);
    char* new_name = (char*)malloc(name_len + 2);  // name + "="
    if (new_name == NULL) {
        return ERR_OUT_OF_MEMORY;
    }
    strncpy(new_name, name, name_len);
    new_name[name_len] = '=';
    new_name[name_len + 1] = '\0';

    char* new_url = (char*)malloc(url->len + 1);
    if (new_url == NULL) {
        free(new_name);
        return ERR_OUT_OF_MEMORY;
    }
    strncpy(new_url, url->val, url->len);
    new_url[url->len] = '\0';

    char* needle = strstr(new_url, new_name);
    if (needle) {
        needle += strlen(new_name);
        char* token = strtok(needle, "&");
        if (strlen(token) > out_len) {
            free(new_name);
            free(new_url);
            return ERR_RUNTIME;
        }
        int len = strlen(token);
        strncpy(out, token, len);
        free(new_name);
        free(new_url);
        return len;
    } else {
        free(new_name);
        free(new_url);
        return ERR_NONE;
    }
}

// stream points to the beginning of received buffer even if http_parse_message gets called for the second time from handle_connection
// however realloc might point to another address (only guarantee is that contents are the same, not necessarily an expansion of the memory but maybe a migration of the contents)
// --> consequence: need to rerun enitire http_parse_message as pointers stored in out might turn out different
int http_parse_message(const char *stream, size_t bytes_received, struct http_message *out, int *content_len)
{
    M_REQUIRE_NON_NULL(stream);
    M_REQUIRE_NON_NULL(out);
    M_REQUIRE_NON_NULL(content_len);

    memset(out, 0, sizeof(struct http_message));

    // return 0: interpreted as "message incomplete"
    if (strstr(stream, HTTP_HDR_END_DELIM) == NULL) {
        return 0;
    }

    char* start_addr = stream;

    // start line example: GET /imgfs/read?res=orig&img_id=mure.jpg HTTP/1.1
    // parse method and uri
    char* space = " ";
    stream = get_next_token(stream, space, &out->method);
    stream = get_next_token(stream, space, &out->uri);

    // skip rest of start line
    struct http_string check;
    stream = get_next_token(stream, HTTP_LINE_DELIM, &check);
    if (strncmp(check.val, "HTTP/1.1", check.len)) {
        printf("Unknown protocol %.*s", check.len, check.val);
        return ERR_INVALID_ARGUMENT;
    }

    stream = http_parse_headers(stream, out);

    char* body_addr = stream;

    // extract content-length
    int found = 0;
    for (int i = 0; i < out->num_headers; ++i) {
        if (!strncmp(out->headers[i].key.val, "Content-Length", out->headers[i].key.len)) {
            *content_len = atoi(out->headers[i].value.val);
            found = 1;
            break;
        }
    }

    // return 1 if there is no body (no Content-Length header, or is value is 0) or you were able to read the full body;
    // content length is body length (always take content length as body length (strlen() might not work bcs of null bytes))
    int header_len = body_addr - start_addr;
    if (!found || *content_len == 0 || (bytes_received - header_len + sizeof(HTTP_HDR_END_DELIM)) >= *content_len) {
        out->body.val = stream;
        out->body.len = *content_len;
        return 1;
    }

    // return 0 if not received the full body.
    return 0;
}

static const char* get_next_token(const char* message, const char* delimiter, struct http_string* output)
{
    M_REQUIRE_NON_NULL(message);
    M_REQUIRE_NON_NULL(delimiter);

    int del_length = strlen(delimiter);
    char* del_start = strstr(message, delimiter);

    if (output != NULL && del_start != NULL) {
        output->len = (size_t) (del_start - message);
        output->val = message;
    }

    // return NULL if delimiter not found
    if (del_start == NULL) {
        return NULL;
    }

    return (const char*) del_start + del_length;;
}


static const char* http_parse_headers(const char* header_start, struct http_message* output)
{
    size_t header_count = 0;
    // assuming header delimiter is a double line delimiter --> after taking away a delimiter there is still a delimiter at the beginning
    while (strstr(header_start, HTTP_LINE_DELIM) != header_start) {
        header_start = get_next_token(header_start, HTTP_HDR_KV_DELIM, &output->headers[header_count].key);
        header_start = get_next_token(header_start, HTTP_LINE_DELIM, &output->headers[header_count].value);
        header_count++;
    }
    output->num_headers = header_count;
    return get_next_token(header_start, HTTP_LINE_DELIM, NULL);
}
