#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include "Request.h"
#include "Response.h"

#define OPTIONS              "t:l:"
#define BUF_SIZE             4096
#define DEFAULT_THREAD_COUNT 4
#define LOG(...)             fprintf(logfile, __VA_ARGS__);
int buffer[BUF_SIZE];
int in = 0;
int out = 0;
int counter = 0;
int program_end = 0;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t spot = PTHREAD_COND_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;
pthread_t *threadz;
int threads = DEFAULT_THREAD_COUNT;
static FILE *logfile;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
// struct HTTP_Request request;
// struct Response http_response;

// Converts a string to an 16 bits unsigned integer.
// Returns 0 if the string is malformed or out of the range.
static size_t strtouint16(char number[]) {
    char *last;
    long num = strtol(number, &last, 10);
    if (num <= 0 || num > UINT16_MAX || *last != '\0') {
        return 0;
    }
    return num;
}
// Creates a socket for listening for connections.
// Closes the program and prints an error message on error.
static int create_listen_socket(uint16_t port) {
    struct sockaddr_in addr;
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        err(EXIT_FAILURE, "socket error");
    }
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr *) &addr, sizeof addr) < 0) {
        err(EXIT_FAILURE, "bind error");
    }
    if (listen(listenfd, 128) < 0) {
        err(EXIT_FAILURE, "listen error");
    }
    return listenfd;
}
static void handle_connection(int connfd) {
    int rd;
    char buf[2049];
    buf[2048] = '\0';
    char buf_copy[2049];
    buf_copy[2048] = '\0';
    memset(buf_copy, '\0', 2048);
    memset(buf, '\0', 2048);
    struct HTTP_Request *request = calloc(1, sizeof(struct HTTP_Request));
    struct Response *http_response = calloc(1, sizeof(struct Response));
    while ((rd = read(connfd, buf, 1)) > 0) {
        strncat(buf_copy, buf, 1);
        if (strstr(buf_copy, "\r\n\r\n") != NULL) {
            break;
        }
        if (errno == EWOULDBLOCK) {
            printf("THIS IS BLOCKED!!!! \n");
        }
    }
    int content = strlen(buf_copy);
    // request.request_id_number = 0;
    // pthread_mutex_lock(&struct_lock);
    if (content != 0) {
        parse_request(buf_copy, connfd, request, http_response);
        // pthread_mutex_lock(&struct_lock);

        // printf("Request id: %d\n",request.request_id_number);
        // int log_number = fileno(logfile);
        // flock(log_number, LOCK_EX);
        pthread_mutex_lock(&log_lock);
        LOG("%s,%s,%d,%d\n", request->method, request->uri, http_response->status_code,
            request->request_id_number);
        fflush(logfile);
        pthread_mutex_unlock(&log_lock);
        // flock(log_number, LOCK_UN);
    }
    free(request);
    free(http_response);
    // request.request_id_number = 0;
    // pthread_mutex_unlock(&struct_lock);
    memset(buf_copy, '\0', 2048);
    memset(buf, '\0', 2048);
}

static void sigterm_handler(int sig) {
    if (sig == SIGTERM) {
        warnx("received SIGTERM");
        fclose(logfile);
        exit(EXIT_SUCCESS);
    }
    if (sig == SIGINT) {
        // end_program = 0;
        warnx("received SIGINT");
        fclose(logfile);
        // program_end = 1;
        // for (int i = 0; i < threads; i++) {
        //     pthread_cond_signal(&full);
        // }
        // for (int i = 0; i < threads; i++) {
        //     if (pthread_join(threadz[i], NULL) != 0) {
        //         err(1, "pthread_join failed");
        //         printf("broken \n");
        //     }
        // }
        // free(threadz);
        exit(EXIT_SUCCESS);
    }
}
static void usage(char *exec) {
    fprintf(stderr, "usage: %s [-t threads] [-l logfile] <port>\n", exec);
}

void *worker(void *arg) {
    (void) arg;
    int rtn;
    while (program_end == 0) {
        pthread_mutex_lock(&lock);
        while (counter == 0) {
            pthread_cond_wait(&full, &lock);
        }
        rtn = buffer[out];
        out = (out + 1) % BUF_SIZE;
        counter -= 1;
        pthread_mutex_unlock(&lock);
        pthread_cond_signal(&spot);
        handle_connection(rtn);
        close(rtn);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int opt = 0;
    int threads = DEFAULT_THREAD_COUNT;
    logfile = stderr;
    memset(buffer, '\0', BUF_SIZE);
    while ((opt = getopt(argc, argv, OPTIONS)) != -1) {
        switch (opt) {
        case 't':
            threads = strtol(optarg, NULL, 10);
            if (threads <= 0) {
                errx(EXIT_FAILURE, "bad number of threads");
            }
            break;
        case 'l':
            logfile = fopen(optarg, "w");
            if (!logfile) {
                errx(EXIT_FAILURE, "bad logfile");
            }
            break;
        default: usage(argv[0]); return EXIT_FAILURE;
        }
    }
    if (optind >= argc) {
        warnx("wrong number of arguments");
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    uint16_t port = strtouint16(argv[optind]);
    if (port == 0) {
        errx(EXIT_FAILURE, "bad port number: %s", argv[1]);
    }
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);
    threadz = malloc(threads * sizeof(*threadz));
    for (int i = 0; i < threads; i++) {
        if (pthread_create(&threadz[i], NULL, &worker, NULL) != 0) {
            err(1, "pthread_create failed");
        }
    }
    int listenfd = create_listen_socket(port);
    for (;;) {
        int connfd = accept(listenfd, NULL, NULL);
        if (connfd < 0) {
            warn("accept error");
            continue;
        }
        pthread_mutex_lock(&lock);
        while (counter == BUF_SIZE) {
            pthread_cond_wait(&spot, &lock);
        }
        buffer[in] = connfd;
        in = (in + 1) % BUF_SIZE;
        counter += 1;
        pthread_mutex_unlock(&lock);
        pthread_cond_signal(&full);
    }
    return EXIT_SUCCESS;
}
