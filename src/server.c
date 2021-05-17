#include "ft_ftp.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>

const ftp_command_t cmd_list[] = {
    { "USER", &callback_ftp_user_command  },
    { "PASS", &callback_ftp_pass_command  },
    { "QUIT", &callback_ftp_quit_command  },
    { "SYST", &callback_ftp_syst_command  },
    { "PORT", &callback_ftp_port_command  },
    { "PWD",  &callback_ftp_pwd_command   },
    { "XPWD", &callback_ftp_pwd_command   },
    { "TYPE", &callback_ftp_type_command  },
    { "CWD",  &callback_ftp_cwd_command   },
    { "NLST", &callback_ftp_nlst_command  },
    { "LIST", &callback_ftp_list_command  },
    { "CDUP", &callback_ftp_cdup_command  },
    { "NOOP", &callback_ftp_noop_command  },
    { "PASV", &callback_ftp_pasv_command  },
    { "SIZE", &callback_ftp_size_command  },
    { "FEAT", &callback_ftp_feat_command  },
    { "RETR", &callback_ftp_retr_command  },
    { "MKD",  &callback_ftp_mkd_command   },
    { "XMKD", &callback_ftp_mkd_command   },
    { "XRMD", &callback_ftp_rmdir_command },
    { "RMD",  &callback_ftp_rmdir_command },
    { "STOR", &callback_ftp_stor_command  },
    { "DELE", &callback_ftp_dele_command  },
};

uint8_t create_ftp_server(ftp_server_t *server, const char *address, uint16_t port)
{
    struct sockaddr_in s = {0};
    struct epoll_event e = {0};
    int option = 1;

    server->s_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (server->s_fd < 0)
        return (1);
    
    // to network byte order
    s.sin_addr.s_addr = inet_addr(address);
    
    s.sin_port = htons(port);
    s.sin_family = AF_INET;

    if (setsockopt(server->s_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0)
        return (1);

    if (bind(server->s_fd, (struct sockaddr *)&s, sizeof(s)) < 0)
        return (1);
    
    if (listen(server->s_fd, SOMAXCONN) < 0)
        return (1);
    
    // create epoll file descriptor
    server->e_fd = epoll_create1(0);

    if (server->e_fd < 0)
        return (1);
    
    e.events = EPOLLIN;
    e.data.fd = server->s_fd;
    
    // add server file descriptor to the epoll queue
    if (epoll_ctl(server->e_fd, EPOLL_CTL_ADD, server->s_fd, &e) < 0)
        return (1);
    
    printf("[+] Listening on %s:%d\n", inet_ntoa(s.sin_addr), ntohs(s.sin_port));

    return (0);
}


uint8_t accept_ftp_server(ftp_server_t *server, client_t *client)
{
    struct epoll_event event = {0};
    socklen_t size_socks = sizeof(client->sin);

    if ((client->fd = accept(server->s_fd, (struct sockaddr *)&client->sin, &size_socks)) < 0)
        return (1);
    
    printf("[%s:%d] New client connected !\n", inet_ntoa(client->sin.sin_addr), ntohs(client->sin.sin_port));
    
    event.data.fd = client->fd;
    event.data.ptr = (client_t *)client;
    event.events = EPOLLIN;

    if (epoll_ctl(server->e_fd, EPOLL_CTL_ADD, client->fd, &event) < 0)
        return (1);
    
    if (ftp_accept_message(client))
        perror("ftp_accept_message()");

    return (0);
}

uint8_t check_read_client(ftp_server_t *server, client_t *client)
{   
    int fd = client->fd;

    size_t len = 0;

    // check if byte is present in file descriptor
    ioctl(fd, FIONREAD, &len);

    // byte present in file descriptor
    if (len)
        return (0);
    
    return (1);
}

uint8_t handle_client(ftp_server_t *server, client_t *client)
{
    if (client == NULL)
        return (0);

    if (check_read_client(server, client)) {
        printf("[%s:%d] closed connection\n", inet_ntoa(client->sin.sin_addr), ntohs(client->sin.sin_port));
        close(client->fd);
        free(client);
        client = NULL;
        return (0);
    }

    char buffer[0x400] = {0};
    char *argv[0x100] = {0};
    size_t byte = read(client->fd, buffer, 0x3FF);

    if (byte < 0) {
        printf("[%s:%d] ", inet_ntoa(client->sin.sin_addr), ntohs(client->sin.sin_port));
        perror("read");
        return (1);
    }

    if (byte == 0 || *buffer < 32)
        return (ftp_command_not_implemented(client));
    
    
    printf("[%s:%d] %s\n", inet_ntoa(client->sin.sin_addr), ntohs(client->sin.sin_port), buffer);

    split_command(buffer, argv, " \r\n");

    for (int i = 0; i < sizeof(cmd_list) / sizeof(cmd_list[0]); i++)
        if (strcmp(argv[0], cmd_list[i].name) == 0)
            return (cmd_list[i].func(server, client, argv));
    
    return (ftp_command_not_implemented(client));
}

uint8_t close_ftp_server(ftp_server_t *server)
{
    if (close(server->e_fd) < 0)
        return (1);
    
    if (server->s_fd)
        close(server->s_fd);
    
    return (0);
}