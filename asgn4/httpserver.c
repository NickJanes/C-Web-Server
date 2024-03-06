#include "asgn4_helper_funcs.h"
#include "connection.h"
#include "debug.h"
#include "request.h"
#include "response.h"
#include "queue.h"

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/file.h>

void handle_connection(int);
void handle_get(conn_t *);
void handle_put(conn_t *);
void handle_unsupported(conn_t *);
void *thread_start(void *);

#define QUEUE_SIZE 10

queue_t *connections;
pthread_mutex_t mutex_file;

int main(int argc, char **argv) {
    if (argc != 2 && argc != 4) {
        warnx("wrong arguments: %s [-t thread_count] port_num", argv[0]);
        return EXIT_FAILURE;
    }

    ssize_t port;
    char *endptr = NULL;
    int threads = 4;

    for (int x = 1; x < argc; x++) {
        if (strcmp(argv[x], "-t") == 0) {
            threads = atoi(argv[(x++) + 1]);
            if (threads < 1) {
                warnx("Can't have less than 1 threads, defaulting to 4.");
                threads = 4;
            }
        } else {
            port = (size_t) strtoull(argv[x], &endptr, 10);
            if (endptr && *endptr != '\0') {
                warnx("invalid port number: %s", argv[1]);
                return EXIT_FAILURE;
            }
            if (port < 1 || port > 65535) {
                warnx("invalid port: %zu", port);
                return EXIT_FAILURE;
            }
        }
    }

    signal(SIGPIPE, SIG_IGN);
    Listener_Socket sock;
    if (listener_init(&sock, port)) {
        warnx("failed to listen on port %zu", port);
        return EXIT_FAILURE;
    }

    //===========================================================================
    connections = queue_new(QUEUE_SIZE);
    pthread_mutex_init(&mutex_file, NULL);
    pthread_t th[threads];
    int id[threads];
    int i = 0;

    for (i = 0; i < threads; i++) {
        id[i] = i;
        if (pthread_create(&th[i], NULL, (void *(*) (void *) ) thread_start, (void *) &id[i])
            != 0) {
            perror("Failed to create the thread");
        }
    }

    while (1) {
        uintptr_t connfd = listener_accept(&sock);
        // fprintf(stderr, "pushing %lu\n", connfd);
        queue_push(connections, (void *) connfd);
    }
    queue_delete(&connections);
    pthread_mutex_destroy(&mutex_file);
    return EXIT_SUCCESS;
}

void *thread_start(void *arg) {
    if (arg) {
    }
    // int id = *(int *) arg;
    intptr_t connfd;
    while (1) {
        // fprintf(stderr, "HEY");
        if (!queue_pop(connections, (void **) &connfd)) {
            continue;
        }
        // fprintf(stderr, "Thread%i: handling connection %lu at address %lu\n", id, connfd, (connfd));
        handle_connection(connfd);
    }
}

void handle_connection(int connfd) {
    conn_t *conn = conn_new(connfd);
    const Response_t *res = conn_parse(conn);

    if (res != NULL) {
        conn_send_response(conn, &RESPONSE_BAD_REQUEST);
    } else {
        const Request_t *req = conn_get_request(conn);

        if (req == &REQUEST_GET) {
            handle_get(conn);
        } else if (req == &REQUEST_PUT) {
            handle_put(conn);
        } else {
            handle_unsupported(conn);
        }
    }

    conn_delete(&conn);
    close(connfd);
    return;
}

void audit_response(conn_t *conn, const Response_t *response) {
    const Request_t *req = conn_get_request(conn);
    char *method = conn_get_header(conn, "Request-Id");
    int id = 0;
    if (method != NULL) {
        id = atoi(method);
    }
    if (req == &REQUEST_GET) {
        method = "GET";
    } else if (req == &REQUEST_PUT) {
        method = "PUT";
    } else {
        method = "UNDEFINED";
    }
    fprintf(stderr, "%s,%s,%i,%i\n", method, conn_get_uri(conn), response_get_code(response), id);
    conn_send_response(conn, response);
}

void handle_get(conn_t *conn) {
    char *uri = conn_get_uri(conn);
    struct stat uri_stats;
    pthread_mutex_lock(&mutex_file);
    if (stat(uri, &uri_stats)) {
        audit_response(conn, &RESPONSE_NOT_FOUND);
        pthread_mutex_unlock(&mutex_file);
        return;
    }
    if (!S_ISREG(uri_stats.st_mode)) {
        audit_response(conn, &RESPONSE_FORBIDDEN);
        pthread_mutex_unlock(&mutex_file);
        return;
    }
    int fd = open(uri, O_RDONLY);
    if (fd == -1) {
        audit_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR);
        pthread_mutex_unlock(&mutex_file);
        return;
    }
    flock(fd, LOCK_SH);
    pthread_mutex_unlock(&mutex_file);
    char *id = conn_get_header(conn, "Request-Id");
    if (id == NULL)
        id = "0";
    conn_send_file(conn, fd, uri_stats.st_size);
    fprintf(stderr, "%s,%s,%i,%s\n", "GET", uri, 200, id);
    close(fd);
}

void handle_put(conn_t *conn) {
    char *uri = conn_get_uri(conn);
    struct stat uri_stats;
    pthread_mutex_lock(&mutex_file);
    bool created = true;
    if (!stat(uri, &uri_stats)) {
        created = false;
        if (!S_ISREG(uri_stats.st_mode)) {
            audit_response(conn, &RESPONSE_FORBIDDEN);
            pthread_mutex_unlock(&mutex_file);
            return;
        }
    }

    int fd = open(uri, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        audit_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR);
        pthread_mutex_unlock(&mutex_file);
        return;
    }
    flock(fd, LOCK_EX);
    pthread_mutex_unlock(&mutex_file);
    ftruncate(fd, 0);
    conn_recv_file(conn, fd);
    if (created) {
        audit_response(conn, &RESPONSE_CREATED);
    } else {
        audit_response(conn, &RESPONSE_OK);
    }
    // flock(fd, LOCK_UN);
    close(fd);
    return;
}

void handle_unsupported(conn_t *conn) {
    debug("Unsupported request");
    audit_response(conn, &RESPONSE_NOT_IMPLEMENTED);
    return;
}
