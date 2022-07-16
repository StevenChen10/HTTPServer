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
#include "Response.h"
#include "Request.h"

struct stat file_stat;

ssize_t write_all(int connfd, char buf[], size_t nbytes) {
    size_t total = 0; //track the total amount of bytes written
    size_t bytes = 0; //tracks the bytes written from a single write() call
    do {
        bytes = write(connfd, buf + total, nbytes - total);
        if (bytes < 0) {
            return -1;
        }
        total += bytes;
    } while (total < nbytes);
    return total;
}

void check_request(int connfd, struct HTTP_Request *request,
    struct Response
        *http_response) { //checks for which method to be used, or if isn't a supported request.
    int is_get = strcmp(request->method, "GET");
    int is_put = strcmp(request->method, "PUT");
    int is_append = strcmp(request->method, "APPEND");
    if (is_get == 0) {
        get_request(connfd, request, http_response);
    } else if (is_put == 0) {
        put_request(connfd, request, http_response);
    } else if (is_append == 0) {
        append_request(connfd, request, http_response);
    }
}

void get_request(int connfd, struct HTTP_Request *request,
    struct Response *http_response) { //handles all the work for GET request
    char *file_name = request->uri;
    file_name++;
    int fd = 0;
    int rd;
    char file_buf[4096];
    memset(file_buf, '\0', sizeof(file_buf));
    fd = open(file_name, O_RDONLY);
    flock(fd, LOCK_SH);
    if (fd == -1) {
        if (errno == 2) {
            not_found(connfd, http_response);
        } else if (errno == 9) {
            not_found(connfd, http_response);
        }
    } else {
        fstat(fd, &file_stat);
        int file_bytes = file_stat.st_size;
        get_ok(connfd, file_bytes, http_response);
        while ((rd = read(fd, file_buf, sizeof(file_buf))) > 0) {
            write_all(connfd, file_buf, rd);
        }
    }
    close(fd);
}

void put_request(int connfd, struct HTTP_Request *request,
    struct Response *http_response) { //handles all the work for a put request
    char write_buf[2048];
    memset(write_buf, '\0', sizeof(write_buf));
    char *file_name = request->uri;
    file_name++;
    int fd = 0;
    int rd;
    char file_buf[4096];
    char *token = strtok(request->header_field, " ");
    token = strtok(NULL, request->header_field);
    char str[40];
    strcpy(str, token);
    char *ptr;
    request->bytes = strtol(str, &ptr, 10);
    fstat(fd, &file_stat);
    fd = open(file_name, O_WRONLY | O_TRUNC);
    flock(fd, LOCK_EX);
    if (fd == -1) {
        fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        ssize_t total = 0;
        while (total < request->bytes && (rd = read(connfd, file_buf, 4096)) > 0) {
            ssize_t bytes_write = 0;
            // bytes_write = write(fd, request.start_of_message_body, request.bytes);
            total += bytes_write;
            bytes_write = write(fd, file_buf + bytes_write, rd - bytes_write);
            total += bytes_write;
        }
        while (total < request->bytes && (rd = read(connfd, file_buf, 4096)) == 0) {
            ssize_t bytes_write = 0;
            bytes_write = write(fd, request->start_of_message_body, request->bytes);
            total += bytes_write;
        }
        created(connfd, http_response);
    } else {
        // ok(connfd);
        ssize_t total = 0;
        while (total < request->bytes && (rd = read(connfd, file_buf, 4096)) > 0) {
            ssize_t bytes_write = 0;
            bytes_write = write_all(fd, file_buf + bytes_write, rd - bytes_write);
            total += bytes_write;
        }
        while (total < request->bytes && (rd = read(connfd, file_buf, 4096)) == 0) {
            ssize_t bytes_write = 0;
            bytes_write = write_all(fd, request->start_of_message_body, request->bytes);
            total += bytes_write;
        }
        ok(connfd, http_response);
    }
    close(fd);
}

void append_request(int connfd, struct HTTP_Request *request,
    struct Response *http_response) { //handles all the work for an append request
    char write_buf[2048];
    memset(write_buf, '\0', sizeof(write_buf));
    char *file_name = request->uri;
    file_name++;
    int fd = 0;
    int rd;
    char file_buf[4096];
    char *token = strtok(request->header_field, " ");
    token = strtok(NULL, request->header_field);
    request->bytes = atoi(token);
    fd = open(file_name, O_WRONLY | O_APPEND);
    flock(fd, LOCK_EX);
    if (fd == -1) {
        if (errno == 2) {
            not_found(connfd, http_response);
        }
    } else {
        ssize_t total = 0;
        while (total < request->bytes && (rd = read(connfd, file_buf, 4096)) > 0) {
            ssize_t bytes_write = 0;
            bytes_write = write(fd, file_buf + bytes_write, rd - bytes_write);
            total += bytes_write;
        }
        while (total < request->bytes && (rd = read(connfd, file_buf, 4096)) == 0) {
            ssize_t bytes_write = 0;
            bytes_write = write(fd, request->start_of_message_body, request->bytes);
            total += bytes_write;
        }
        ok(connfd, http_response);
    }
    close(fd);
}

void not_found(int connfd, struct Response *http_response) { //print not found to socket
    char write_buf[2048];
    memset(write_buf, '\0', sizeof(write_buf));
    http_response->status_code = 404;
    http_response->status_phrase = "Not Found";
    snprintf(http_response->header_field, sizeof(http_response->header_field),
        "Content-Length: %s\r\n\r\n", "10");
    strcpy(http_response->message_body, http_response->status_phrase);
    snprintf(write_buf, sizeof(write_buf), "HTTP/1.1 %d %s\r\n%s%s\n", http_response->status_code,
        http_response->status_phrase, http_response->header_field, http_response->message_body);
    write_all(connfd, write_buf, 56);
    memset(write_buf, '\0', sizeof(write_buf));
}

void created(int connfd, struct Response *http_response) { //print created to socket
    char write_buf[2048];
    memset(write_buf, '\0', sizeof(write_buf));
    http_response->status_code = 201;
    http_response->status_phrase = "Created";
    snprintf(http_response->header_field, sizeof(write_buf), "Content-Length: %s", "8");
    strcpy(http_response->message_body, http_response->status_phrase);
    snprintf(write_buf, sizeof(write_buf), "HTTP/1.1 %d %s\r\n%s\r\n\r\n%s\n",
        http_response->status_code, http_response->status_phrase, http_response->header_field,
        http_response->message_body);
    write_all(connfd, write_buf, 51);
    memset(write_buf, '\0', sizeof(write_buf));
}

void get_ok(
    int connfd, int file_bytes, struct Response *http_response) { //print GET's 200 ok to socket
    char write_buf[2048];
    memset(write_buf, '\0', sizeof(write_buf));
    http_response->status_code = 200;
    int size = 0;
    http_response->status_phrase = "OK";
    snprintf(http_response->header_field, sizeof(write_buf), "Content-Length: %d", file_bytes);
    snprintf(write_buf, sizeof(write_buf), "HTTP/1.1 %d %s\r\n%s\r\n\r\n",
        http_response->status_code, http_response->status_phrase, http_response->header_field);
    size = strlen(write_buf);
    write_all(connfd, write_buf, size);
    memset(write_buf, '\0', sizeof(write_buf));
}

void ok(int connfd, struct Response *http_response) { //print normal 200 ok to socket
    char write_buf[2048];
    memset(write_buf, '\0', sizeof(write_buf));
    http_response->status_code = 200;
    http_response->status_phrase = "OK";
    snprintf(http_response->header_field, sizeof(write_buf), "Content-Length: %d", 3);
    strcpy(http_response->message_body, http_response->status_phrase);
    snprintf(write_buf, sizeof(write_buf), "HTTP/1.1 %d %s\r\n%s\r\n\r\n%s\n",
        http_response->status_code, http_response->status_phrase, http_response->header_field,
        http_response->message_body);
    write_all(connfd, write_buf, 41);
    memset(write_buf, '\0', sizeof(write_buf));
}

void internal_server_error(int connfd, struct Response *http_response) {
    char write_buf[2048];
    memset(write_buf, '\0', sizeof(write_buf));
    http_response->status_code = 500;
    http_response->status_phrase = "Internal Server Error";
    strcpy(http_response->message_body, http_response->status_phrase);
    snprintf(write_buf, sizeof(write_buf), "HTTP/1.1 %d %s\r\n%s\r\n\r\n%s\n",
        http_response->status_code, http_response->status_phrase, http_response->header_field,
        http_response->message_body);
    write_all(connfd, write_buf, 70);
    memset(write_buf, '\0', sizeof(write_buf));
}
