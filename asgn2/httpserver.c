#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <regex.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#include "asgn2_helper_funcs.h"

#define BUFFER_SIZE 2048

void write_response(int socket, char *response) {
    // fprintf(stderr, "%s", response);
    write_all(socket, response, strlen(response));
}

void write_error_response(int socket, int error_code, char body[BUFFER_SIZE]) {
    char msg[BUFFER_SIZE] = "";
    sprintf(msg, "HTTP/1.1 %d %s\r\nContent-Length: %lu\r\n\r\n%s\n", error_code, body,
        strlen(body) + 1, body);
    write_response(socket, msg);
}

//Parses method, uri, and version from an html request line
int process_request_line(
    char (*request)[2048], char (*method)[9], char (*uri)[64], char (*version)[9]) {
    regex_t regex;
    regmatch_t match[4];

    if (regcomp(&regex, "([a-zA-z]{1,8}) /([a-zA-Z0-9.-]{1,63}) (HTTP/[0-9]\\.[0-9])\r\n",
            REG_EXTENDED)) {
        // fprintf(stderr, "get reg error");
        regfree(&regex);
        return -1;
    }

    if (regexec(&regex, *request, 4, match, 0) != 0) {
        regfree(&regex);
        // fprintf(stderr, "No regex match for string:\n%s\n", *request);
        return -1;
    }

    strncpy(*method, *request + match[1].rm_so, match[1].rm_eo - match[1].rm_so);
    (*method)[match[1].rm_eo - match[1].rm_so] = '\0';
    strncpy(*uri, *request + match[2].rm_so, match[2].rm_eo - match[2].rm_so);
    (*uri)[match[2].rm_eo - match[2].rm_so] = '\0';
    strncpy(*version, *request + match[3].rm_so, match[3].rm_eo - match[3].rm_so);
    (*version)[match[3].rm_eo - match[3].rm_so] = '\0';

    strncpy(*request, *request + match[3].rm_eo - 1, strlen(*request) - match[3].rm_eo);
    (*request)[strlen(*request) - match[3].rm_eo + 1] = '\0';

    regfree(&regex);
    return 0;
}

int main(int argc, char **argv) {
    int port;

    if (argc != 2) {
        port = 1234;
        // fprintf(stderr, "No port specified, using 1234.\n");
    } else {
        port = atoi(argv[1]);

        // fprintf(stderr, "Inputted port is %i.\n", port);
    }
    if (port < 1 || port > 65535) {
        printf("Invalid Port\n");
        return 1;
    }
    Listener_Socket socket;

    if (listener_init(&socket, port)) {
        // fprintf(stderr, "Error opening socket on port %i.", port);
        return 1;
    }
    while (true) {
        // fprintf(stderr, "Listening for connection on port http://127.0.0.1:%i\n", port);
        int client_fd = listener_accept(&socket);
        // fprintf(stderr, "Handling connection\n");
        if (client_fd == -1) {
            // fprintf(stderr, "Error opening connection");
            close(client_fd);
            break;
        }
        char buf[BUFFER_SIZE];
        int bytes_read = read_until(client_fd, buf, BUFFER_SIZE, "\r\n\r\n");
        if (bytes_read < 0) {
            write_error_response(client_fd, 500, "Internal Error");
            close(client_fd);
            continue;
        }
        char method[9];
        char uri[64];
        char version[9];

        if (process_request_line(&buf, &method, &uri, &version)) {
            write_error_response(client_fd, 400, "Bad Request");
            close(client_fd);
            continue;
        }

        if (strcmp(version, "HTTP/1.1")) {
            write_error_response(client_fd, 505, "Version Not Supported");
            close(client_fd);
            continue;
        }

        if (strncmp(method, "GET", 3) == 0) {
            struct stat uri_stats;
            if (stat(uri, &uri_stats)) {
                write_error_response(client_fd, 404, "Not Found");
                close(client_fd);
                continue;
            }
            if (!S_ISREG(uri_stats.st_mode)) {
                write_error_response(client_fd, 403, "Forbidden");
                close(client_fd);
                continue;
            }
            int fd = open(uri, O_RDONLY);
            if (fd == -1) {
                write_error_response(client_fd, 500, "Internal Error");
                close(client_fd);
                continue;
            }
            int bytes_written = 0;

            char msg[BUFFER_SIZE] = "";
            sprintf(msg, "HTTP/1.1 200 OK\nContent-Length: %li\n\n", uri_stats.st_size);
            write_response(client_fd, msg);
            do {
                bytes_written = pass_bytes(fd, client_fd, BUFFER_SIZE);
                // fprintf("LOCKED HERE");
            } while (bytes_written > 0);
            close(fd);
        } else if (strncmp(method, "PUT", 3) == 0) {
            int code = 201;
            struct stat uri_stats;
            if (stat(uri, &uri_stats) == 0) {
                code = 200;
                if (!S_ISREG(uri_stats.st_mode)) {
                    write_error_response(client_fd, 403, "Forbidden");
                    close(client_fd);
                    continue;
                }
            }

            int fd = open(uri, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            regex_t regex;
            regmatch_t match[2];
            if (fd == -1) {
                write_error_response(client_fd, 500, "Internal Error");
                close(client_fd);
                continue;
            }

            if (regcomp(&regex, "Content-Length\\s*:\\s*([0-9]+)", REG_EXTENDED)) {
                regfree(&regex);
                write_error_response(client_fd, 500, "Internal Error");
                close(client_fd);
                continue;
            }
            if (regexec(&regex, buf, 2, match, 0) != 0) {
                regfree(&regex);
                fprintf(stderr, "BUF WAS: \n%s\nTEST\n\n", buf);
                write_error_response(client_fd, 400, "Bad Request");
                close(client_fd);
                continue;
            }
            char temp[64];
            strncpy(temp, buf + match[1].rm_so, match[1].rm_eo - match[1].rm_so);
            temp[match[1].rm_eo - match[1].rm_so] = '\0';
            int length = atoi(temp);

            regfree(&regex);
            if (regcomp(&regex, "\r\n\r\n", REG_EXTENDED)) {
                regfree(&regex);
                write_error_response(client_fd, 500, "Internal Error");
                close(client_fd);
                continue;
            }
            if (regexec(&regex, buf, 1, match, 0) != 0) {
                regfree(&regex);
                write_error_response(client_fd, 400, "Bad Request");
                close(client_fd);
                continue;
            }
            regfree(&regex);

            strncpy(temp, buf + match[0].rm_eo, strlen(buf) - match[0].rm_eo);
            temp[strlen(buf) - match[0].rm_eo] = '\0';
            char msg[BUFFER_SIZE] = "";

            ssize_t words = write_all(fd, msg, strlen(msg));
            length -= words;
            pass_bytes(client_fd, fd, length);
            if (code == 200) {
                write_error_response(client_fd, code, "OK");
            } else if (code == 201) {
                write_error_response(client_fd, code, "Created");
            } else {
                write_error_response(client_fd, 500, "Internal Error");
            }
        } else {
            write_error_response(client_fd, 501, "Not Implemented");
        }

        close(client_fd);
    }
    return 0;
}
