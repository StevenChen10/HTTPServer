#ifndef RESPONSE_H
#define RESPONSE_H
#include <regex.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/stat.h>

struct Response {
    int status_code;
    char header_field[2048];
    char *status_phrase;
    char message_body[4096];
};

void check_request();
void bad_request();
void forbidden();
void not_found();
ssize_t write_all();
void not_implemented();
void get_request();
void put_request();
void append_request();
void get_ok();
void created();
void ok();
void internal_server_error();
#endif
