#include "ft_ftp.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static volatile int running = 1;

void handle_signal(int sig)
{
    signal(sig, SIG_IGN);
    usleep(2000);
    printf("[+] wait moment....\n");
    running = 0;
}

int main(int argc, char **argv)
{

    if (argc < 4) {
        fprintf(stderr, "usage : %s <ip> <port> <username> <password>\n", argv[0]);
        return (1);
    }

    ftp_server_t server = {0};
    struct epoll_event events[MAX_EVENTS_EPOLL] = {0};
    int new_fd, ev_fd, i;

    strcpy(server.ip, argv[1]);
    server.username = argv[3];
    server.password = argv[4];

    getcwd(server.path, sizeof(server.path));
    strcat(server.path, "/");

    signal(SIGINT, handle_signal);

    if (create_ftp_server(&server, server.ip, atoi(argv[2]))) {
        perror("create_ftp_server()");
        return (1);
    }

    printf("[+] FTP server pid (%d)\n", getpid());

    while (running) {

        ev_fd = epoll_wait(server.e_fd, events, MAX_EVENTS_EPOLL, -1);

        for (i = 0; i < ev_fd; ++i) {
            if (events[i].data.fd == server.s_fd) {
                
                client_t *client = (client_t *)malloc(sizeof(client_t));

                if (accept_ftp_server(&server, client)) {
                    perror("accept_ftp_server()");
                    continue;
                }

                client->type = 'A';

                getcwd(client->path, sizeof(client->path));
                strcat(client->path, "/");

            } else {
                if (handle_client(&server, (client_t *)events[i].data.ptr))
                    perror("error");
            }
        }
    }

    if (close_ftp_server(&server)) {
        perror("close_ftp_server()");
        return (1);
    }
    
    printf("\n[+] Server stopped !\n");

    return (0);
}