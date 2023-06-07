#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>

enum { ARG_MIN_COUNT = 4 };

enum { BUFFER_SIZE = 10000 };

int socket_fd, socket_fd_2;

void sigchld_handler(int sig_num) {
    shutdown(socket_fd, SHUT_WR);
}

char *parse_server_address(char *addr) {
    char *ptr = addr;
    while (*ptr != '\0') {
        if (*ptr == ':') {
            *ptr = '\0';
            return ++ptr;
        }
        ++ptr;
    }
    return NULL;
}

int create_connection(char *node, char *service) {
    struct addrinfo *res = NULL;
    struct addrinfo hint = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };
    int gai_err = getaddrinfo(node, service, &hint, &res);
    if (gai_err) {
        printf("%s\n", gai_strerror(gai_err));
        return -1;
    }
    int sock = -1;
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        sock = socket(ai->ai_family, ai->ai_socktype, 0);
        if (sock < 0) {
            continue;
        }
        if (connect(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
            close(sock);
            sock = -1;
            continue;
        }
        break;
    }
    freeaddrinfo(res);

    if (sock < 0) {
        printf("Connection failed.\n");
        exit(1);
    }
    return sock;
}

int main(int argc, char *argv[]) {
    if (argc < ARG_MIN_COUNT) {
        printf("wrong arg count. Usage: ./client <server_address:port> spawn "
               "<cmd> [args]\n");
        return 1;
    }
    char *port = parse_server_address(argv[1]);

    int status;

    socket_fd = create_connection(argv[1], port);
    socket_fd_2 = dup(socket_fd);
    FILE *fout = fdopen(socket_fd_2, "w");
    setvbuf(fout, NULL, _IONBF, 0);

    fprintf(fout, "%d\n", argc - 3);
    for (int i = 3; i < argc; ++i) {
        int status = fprintf(fout, "%s\n", argv[i]);
        if (status < 1) {
            printf("print to server failed.\n");
            --i;
        }
    }

    struct sigaction sigchld_sa = {
        .sa_handler = sigchld_handler,
        .sa_flags = SA_RESTART,
    };
    status = sigaction(SIGCHLD, &sigchld_sa, NULL);

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return 1;
    }
    if (pid > 0) {
        fclose(fout);
        char buf[BUFFER_SIZE + 1];
        while ((status = read(socket_fd, buf, BUFFER_SIZE)) != 0) {
            buf[status] = '\0';
            printf("%s", buf);
        }
        kill(pid, SIGTERM);
        wait(NULL);
    } else {
        close(socket_fd);
        char buf[BUFFER_SIZE + 1];
        while ((status = scanf("%s", buf)) > 0) {
            fprintf(fout, "%s\n", buf);
        }
    }

    return 0;
}
