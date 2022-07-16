#ifndef REQUEST_H
#define REQUEST_H
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
#include <sys/file.h>

struct HTTP_Request {
    char method[9];
    char uri[20];
    char version[10];
    char header_field[2048];
    char request_id[2048];
    char *start_of_message_body;
    int bytes;
    int request_id_number;
};
void parse_request();
void parse_header();
void find_end_of_request();
#endif
