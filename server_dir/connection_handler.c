#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "connection_handler.h"

enum { BUFFER_SIZE = 1000 };

enum { MAX_ARG_COUNT = 100, MAX_ARG_LEN = 1000 };

void sigchld_handler(int sig_num) {
    exit(0);
}

int recieve_and_parse_cmd(int connection_fd, char **cmd) {
    int temp_conn_fd = dup(connection_fd);
    FILE *fin = fdopen(temp_conn_fd, "r");
    int arg_cnt;
    int status = fscanf(fin, "%d", &arg_cnt);
    if (status <= 0) {
        fprintf(stderr, "command parse failed. wrong format\n");
        fclose(fin);
        return -1;
    }
    for (int i = 0; i < arg_cnt; ++i) {
        status = fscanf(fin, "%1000s", cmd[i]);
        if (status <= 0) {
            fprintf(stderr, "command scanf failed\n");
            fclose(fin);
            return -1;
        }
    }
    cmd[arg_cnt] = NULL;
    fclose(fin);

    return arg_cnt;
}

void handle_worker(int connection_fd, int arg_cnt, char **cmd_args) {
    int pipe_fds[2];
    int status = pipe(pipe_fds);
    if (status == -1) {
        perror("pipe failed");
        close(connection_fd);
        return;
    }
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        close(connection_fd);
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return;
    }
    if (pid > 0) {
        struct sigaction sigchld_sa = {
            .sa_handler = sigchld_handler,
        };
        status = sigaction(SIGCHLD, &sigchld_sa, NULL);

        close(pipe_fds[0]);
        int wait_status;
        char buf[BUFFER_SIZE + 1];
        while ((status = read(connection_fd, buf, BUFFER_SIZE)) > 0) {
            buf[status] = '\0';
            int written = 0;
            while (written < status) {
                int write_status =
                    write(pipe_fds[1], buf + written, status - written);
                if (write_status == -1) {
                    perror("write failed");
                    // break?
                } else {
                    written += write_status;
                }
            }
        }
        status = kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        close(pipe_fds[1]);
        exit(0);
    } else {
        close(pipe_fds[1]);
        dup2(pipe_fds[0], STDIN_FILENO);
        close(pipe_fds[0]);
        dup2(connection_fd, STDOUT_FILENO);
        close(connection_fd);

        status = execvp(cmd_args[0], cmd_args);
        if (status == -1) {
            perror("command exec failed");
        }
        exit(1);
    }
}

void handle_connection(int connection_fd) {
    char full_command[MAX_ARG_COUNT + 1][MAX_ARG_LEN + 1];
    char *cmd_args[MAX_ARG_COUNT + 1];
    for (int i = 0; i < MAX_ARG_COUNT; ++i) {
        for (int j = 0; j < MAX_ARG_LEN; ++j) {
            full_command[i][j] = '\0';
        }
        cmd_args[i] = full_command[i];
    }
    int arg_cnt = recieve_and_parse_cmd(connection_fd, cmd_args);

    handle_worker(connection_fd, arg_cnt, cmd_args);
}

void recieve_connection(int socket_fd) {
    struct sockaddr conn_addr;
    socklen_t sockaddr_len = sizeof(conn_addr);
    int connection_fd = accept(socket_fd, &conn_addr, &sockaddr_len);
    if (connection_fd == -1) {
        perror("accept connection failed");
        return;
    }
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        close(connection_fd);
        return;
    }
    if (pid > 0) {
        close(connection_fd);
        return;
    } else {
        close(socket_fd);
        handle_connection(connection_fd);
    }
}
