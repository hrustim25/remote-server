#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "connection_handler.h"

enum { REQUIRED_ARG_COUNT = 2 };

void init_signal_handlers() {
    struct sigaction sigchld_sa = {
        .sa_flags = SA_NOCLDWAIT,
    };
    int status = sigaction(SIGCHLD, &sigchld_sa, NULL);
    if (status == -1) {
        perror("sigaction failed");
    }
}

void init_error_logger() {
    int err_fd = open("server_log.txt", O_CREAT | O_RDWR);
    dup2(err_fd, STDERR_FILENO);
    close(err_fd);
}

void start_daemon() {
    int status = daemon(1, 1);
    if (status == -1) {
        perror("daemon could not start.");
        exit(1);
    }
}

int create_socket(char *port) {
    struct addrinfo *res;
    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_PASSIVE,
    };
    int status = getaddrinfo(NULL, port, &hints, &res);
    if (status) {
        fprintf(stderr, "Could not create socket: %s\n", gai_strerror(status));
        exit(1);
    }
    int sock = -1;
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        sock = socket(ai->ai_family, ai->ai_socktype, 0);
        if (sock < 0) {
            continue;
        }
        int opt = 1;
        int status = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                                &opt, sizeof(opt));
        if (status < 0) {
            close(sock);
            sock = -1;
            continue;
        }
        if (bind(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
            close(sock);
            sock = -1;
            continue;
        }
        if (listen(sock, SOMAXCONN) < 0) {
            close(sock);
            sock = -1;
            continue;
        }
        break;
    }
    freeaddrinfo(res);

    if (sock == -1) {
        fprintf(stderr, "Cound not create socket.\n");
        exit(1);
    }
    return sock;
}

int main(int argc, char *argv[]) {
    if (argc != REQUIRED_ARG_COUNT) {
        fprintf(stderr, "Wrong argument count. Usage: ./server PORT");
        return 1;
    }
    init_error_logger();

    char *port = argv[1];

    init_signal_handlers();
    start_daemon();

    int socket_fd = create_socket(port);
    while (1) {
        recieve_connection(socket_fd);
    }

    return 0;
}
