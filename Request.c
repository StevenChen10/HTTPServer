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
#include "Request.h"
#include "Response.h"

void parse_request(char *buf, int connfd, struct HTTP_Request *request,
    struct Response *http_response) { //checks if the request line is valid.
    char *expr = "(^[a-zA-Z]{1,8}) (/[a-zA-Z0-9_.]{1,19}) (HTTP/[0-9].[0-9]\r\n)";
    regex_t regex;
    int result = regcomp(&regex, expr, REG_EXTENDED);
    char *request_line = calloc(2048, sizeof(char));
    if (request_line == NULL) {
        internal_server_error(connfd);
    }
    strcpy(request_line, buf);
    regmatch_t found[4];

    result = regexec(&regex, request_line, 4, found, 0);
    int start = found[1].rm_so;
    int end = found[1].rm_eo;

    memset(request->method, '\0', sizeof(request->method));
    memset(request->uri, '\0', sizeof(request->uri));
    memset(request->version, '\0', sizeof(request->version));

    snprintf(request->method, sizeof(request->method), "%.*s", end - start, request_line + start);

    int second_start = found[2].rm_so;
    int second_end = found[2].rm_eo;
    snprintf(request->uri, sizeof(request->uri), "%.*s", second_end - second_start,
        request_line + second_start);

    int third_start = found[3].rm_so;
    int third_end = found[3].rm_eo;
    snprintf(request->version, sizeof(request->version) + 1, "%.*s", third_end - third_start,
        request_line + third_start);
    parse_header(buf, connfd, request, http_response);
    regfree(&regex);
    free(request_line);
}

void parse_header(char *buf, int connfd, struct HTTP_Request *request,
    struct Response *http_response) { //parses the headers to make sure they are all valid
    char *expr = "[a-zA-Z-]*: [a-zA-Z0-9:./*]*\r\n";
    regex_t regex;
    regcomp(&regex, expr, REG_EXTENDED);

    char *header_field = buf;
    regmatch_t found[100];
    char *header = calloc(2048, sizeof(char));
    if (header == NULL) {
        internal_server_error(connfd);
    }

    int offset = 0;
    int second_result = 0;
    for (int i = 0; i < 100;
         i++) { //Citing Ian Mackinnon for giving me an idea of how to be looping regex matching.
        if ((second_result = regexec(&regex, header_field, 100, found, 0))) {
            if (second_result == REG_NOMATCH) {
                break;
            }
        }
        for (int j = 0; j < 100; j++) {
            if (found[j].rm_so == -1) {
                break;
            }
            if (j == 0) {
                offset = found[j].rm_eo;
            }
            snprintf(header, (found[j].rm_so + found[j].rm_eo), "%.*s",
                found[j].rm_eo - found[j].rm_so, header_field + found[j].rm_so);
            if (strstr(header, "Content-Length: ") != NULL) {
                strcpy(request->header_field, header);
            }
            if (strstr(header, "Request-Id: ") != NULL) {
                strcpy(request->request_id, header);
                char *token = strtok(request->request_id, " ");
                token = strtok(NULL, request->request_id);
                char str[40];
                strcpy(str, token);
                char *ptr;
                request->request_id_number = strtol(str, &ptr, 10);
            }
            header_field += offset;
        }
    }
    regfree(&regex);
    free(header);
    check_request(connfd, request, http_response);
}
