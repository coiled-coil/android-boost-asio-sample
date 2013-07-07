#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct request_header_tag {
    int content_length;
    int chunked;
    int keep_alive;
} request_header_t;

int parse_header(const char *buf, int len, request_header_t *h);

#if defined(__cplusplus)
}
#endif
