#include "ft_ftp.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <sys/ioctl.h>

uint8_t callback_ftp_user_command(ftp_server_t *server, client_t *client, char **command)
{
    if (command[1] == NULL) {
        dprintf(client->fd, "501 - Syntax error or arguments error.\r\n");
        return (0);
    }

    if (client->state == LOGGED) {
        client->state = AUTH;
        client->username = NULL;
        dprintf(client->fd, "331 - last session was flushed %s received, waiting a password.\r\n", command[1]);

        return (0);
    }

    
    dprintf(client->fd, "331 - %s received, waiting a password.\r\n", command[1]);
    
    client->state = AUTH;
    client->username = command[1];

    return (0);
}

uint8_t callback_ftp_pass_command(ftp_server_t *server, client_t *client, char **command)
{
    if (command[1] == NULL) {
        dprintf(client->fd, "501 - Syntax error or arguments error.\r\n");
        return (0);
    }

    if (client->state < AUTH) {
        dprintf(client->fd, "503 - Bad sequence command. you must be set an username\r\n");
        return (0);
    }

    if (strcmp(server->password, command[1]) == 0) {
        client->state = LOGGED;
        dprintf(client->fd, "230 - you are logged in %s.\r\n", command[1]);
        printf("[%s:%d] Logged in %s\n", inet_ntoa(client->sin.sin_addr), ntohs(client->sin.sin_port), command[1]);
        return (0);
    }

    dprintf(client->fd, "530 - bad username or bad password.\r\n");
    
    client->state = CONNECTED;

    return (0);
}


uint8_t callback_ftp_syst_command(ftp_server_t *server, client_t *client, char **command)
{
    dprintf(client->fd, "215 - Unix : L8\r\n");
    
    return (0);
}

uint8_t callback_ftp_quit_command(ftp_server_t *server, client_t *client, char **command)
{
    dprintf(client->fd, "231 Service finished thanks for using !\r\n");
    
    if (close(client->fd))
        return (1);
    
    printf("[%s:%d] client closed.\n", inet_ntoa(client->sin.sin_addr), ntohs(client->sin.sin_port));

    return (0);
}

uint8_t callback_ftp_port_command(ftp_server_t *server, client_t *client, char **command)
{
    if (client->state < LOGGED) {
        dprintf(client->fd, "530 - you must be logged in to use this feature.\r\n");
        return (0);
    }

    char client_ip[0x10] = {0};
    uint16_t client_port = 0;
    struct sockaddr_in client_socks = {0};

    parse_port_command(command[1], client_ip, &client_port);

    client->fd_data = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (client->fd_data < 0) {
        dprintf(client->fd, "451 - Local error in server\r\n");;
        return (1);
    }
    
    client_socks.sin_addr.s_addr = inet_addr(client_ip);
    client_socks.sin_port = htons(client_port);
    client_socks.sin_family = AF_INET;

    if (connect(client->fd_data, (struct sockaddr *)&client_socks, sizeof(client_socks)) < 0) {
        dprintf(client->fd, "421 - Error to remote connection\r\n");
        return (1);
    }
    
    dprintf(client->fd, "200 - Active data connection established.\r\n");

    return (0);
}

uint8_t callback_ftp_pwd_command(ftp_server_t *server, client_t *client, char **command)
{
    if (client->state < LOGGED) {
        dprintf(client->fd, "530 - you must be logged in to use this feature.\r\n");
        return (0);
    }

    char *client_path = client->path;
    char *server_path = server->path;

    while (*server_path++ && *client_path)
        client_path++;
    
    if (*client_path == 0)
        dprintf(client->fd, "257 - '/' is current directory.\r\n");
    else
        dprintf(client->fd, "257 - '%s' is current directory.\r\n", client_path);

    return (0);
}

uint8_t callback_ftp_type_command(ftp_server_t *server, client_t *client, char **command)
{
    if (client->state < LOGGED) {
        dprintf(client->fd, "530 - you must be logged in to use this feature.\r\n");
        return (0);
    }

    if (command[1][0] != 'A' && command[1][0] != 'I') {
        dprintf(client->fd, "501 - syntax error in arguments\r\n");
        return (0);
    }

    client->type = command[1][0];
    char *ptr_name = (client->type == 'I') ? "binary" : "ascii";

    dprintf(client->fd, "200 - Type set to : %s\r\n", ptr_name); 

    return (0);
}

uint8_t callback_ftp_cwd_command(ftp_server_t *server, client_t *client, char **command)
{
    if (client->state < LOGGED) {
        dprintf(client->fd, "530 - you must be logged in to use this feature.\r\n");
        return (0);
    }

    if (command[1] == NULL) {
        dprintf(client->fd, "501 - Syntax error in arguments\r\n");
        return (0);
    }

    parse_dir(server->path, client->path, command[1], 0x100);

    char *client_path = client->path;
    char *server_path = server->path;

    while (*server_path++)
        client_path++;
    
    if (*client_path == 0)
        dprintf(client->fd, "250 - '/' is current directory\r\n");
    else
        dprintf(client->fd, "250 - '%s' is current directory\r\n", client_path);

    return (0);
}

uint8_t callback_ftp_nlst_command(ftp_server_t *server, client_t *client, char **command)
{
    if (client->state < LOGGED) {
        dprintf(client->fd, "530 - you must be logged in to use this feature.\r\n");
        return (0);
    }

    if (client->fd_data < 0)
        return (0);

    pid_t child = 0;
    char *argv[] = {"ls", client->path, NULL};

    child = fork();

    if (child < 0)
        return (1);

    if (child == 0) {
        // Child process

        dup2(client->fd_data, STDOUT_FILENO);
        dup2(client->fd_data, STDERR_FILENO);

        if (execvp(argv[0], argv) < 0)
            return (1);

    } else {
        // Parent process
        wait(NULL);
        dprintf(client->fd, "125 Data connection already open. Transfer starting.\r\n");
    }

    dprintf(client->fd, "226 Transfer complete.\r\n");
    close(client->fd_data);

    return (0);
}

uint8_t callback_ftp_list_command(ftp_server_t *server, client_t *client, char **command)
{
    if (client->state < LOGGED) {
        dprintf(client->fd, "530 - you must be logged in to use this feature.\r\n");
        return (0);
    }

    if (client->fd_data < 0)
        return (1);

    pid_t child = 0;
    char *argv[] = {"ls", "-la", client->path, NULL};

    child = fork();

    if (child < 0)
        return (1);

    if (child == 0) {
        // Child process

        dup2(client->fd_data, STDOUT_FILENO);
        dup2(client->fd_data, STDERR_FILENO);

        if (execvp(argv[0], argv) < 0)
            return (1);

    } else {
        // Parent process
        wait(NULL);
        dprintf(client->fd, "125 - Data connection already open. Transfer starting.\r\n");
    }

    dprintf(client->fd, "226 - Transfer complete.\r\n");
    close(client->fd_data);

    return (0);
}

uint8_t callback_ftp_cdup_command(ftp_server_t *server, client_t *client, char **command)
{
    if (client->state < LOGGED) {
        dprintf(client->fd, "530 - you must be logged in to use this feature.\r\n");
        return (0);
    }

    parse_dir(server->path, client->path, "/", 0x100);

    // Debug
    //printf("%s\n", client->path);

    dprintf(client->fd, "250 - '/' is current directory\r\n");

    return (0);
}

uint8_t callback_ftp_noop_command(ftp_server_t *server, client_t *client, char **command)
{
    dprintf(client->fd, "200 - I successfully done nothin'.\r\n");

    return (0);
}

uint8_t callback_ftp_pasv_command(ftp_server_t *server, client_t *client, char **command)
{
    if (client->state < LOGGED) {
        dprintf(client->fd, "530 - you must be logged in to use this feature.\r\n");
        return (0);
    }

    char ip[16] = {0};
    char *byte_ip[0x5];
    struct sockaddr_in socks = {0}, client_socks = {0};
    socklen_t size_client_socks = sizeof(client_socks);
    int a = 0, b = 0, fd = 0, option = 1;

    strcpy(ip, server->ip);

    socks.sin_family = AF_INET;
    socks.sin_addr.s_addr = inet_addr(ip);
    socks.sin_port = htons(generate_random_port(&a, &b));

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (fd < 0) {
        dprintf(client->fd, "451 - Local error in server\r\n");;
        return (1);
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0)
        return (1);

    if (bind(fd, (struct sockaddr *)&socks, sizeof(socks)) < 0) {
        dprintf(client->fd, "451 - Local error in server\r\n");;
        return (1);
    }

    if (listen(fd, SOMAXCONN) < 0) {
        dprintf(client->fd, "451 - Local error in server\r\n");;
        return (1);
    }

    split_command(ip, byte_ip, ".");
    
    dprintf(client->fd, "227 - Entering passive mode (%s,%s,%s,%s,%d,%d).\r\n", byte_ip[0], byte_ip[1], byte_ip[2], byte_ip[3], a, b);
    printf("227 - Entering passive mode (%s,%s,%s,%s,%d,%d).\r\n", byte_ip[0], byte_ip[1], byte_ip[2], byte_ip[3], a, b);

    client->fd_data = accept(fd, (struct sockaddr *)&client_socks, &size_client_socks);

    if (client->fd_data < 0) {
        dprintf(client->fd, "451 - Local error in server\r\n");
        return (1);
    }

    return (0);
}

uint8_t callback_ftp_size_command(ftp_server_t *server, client_t *client, char **command)
{
    if (client->state < LOGGED) {
        dprintf(client->fd, "530 - you must be logged in to use this feature.\r\n");
        return (0);
    }

    if (client->type == 'A') {
        dprintf(client->fd, "550 SIZE not allowed in ASCII mode.\r\n");
        return (0);
    }

    if (command[1] == NULL) {
        dprintf(client->fd, "501 - Syntax error in arguments\r\n");
        return (0);
    }

    char path[0x400] = {0};
    struct stat st;

    snprintf(path,0x3FF,"%s/%s", client->path, command[1]);

    stat(path, &st);

    dprintf(client->fd, "213 %ld\r\n", st.st_size);

    return (0);
}

uint8_t callback_ftp_feat_command(ftp_server_t *server, client_t *client, char **command)
{
    if (client->state < LOGGED) {
        dprintf(client->fd, "530 - you must be logged in to use this feature.\r\n");
        return (0);
    }

    dprintf(client->fd, "211 - Features supported:\nSIZE\r\n211 - End FEAT.\r\n");

    return (0);
}

uint8_t callback_ftp_retr_command(ftp_server_t *server, client_t *client, char **command)
{
    if (client->state < LOGGED) {
        dprintf(client->fd, "530 - you must be logged in to use this feature.\r\n");
        return (0);
    }

    if (command[1] == NULL) {
        dprintf(client->fd, "501 - Syntax error in arguments\r\n");
        return (0);
    }

    struct stat st;
    int fd = 0;
    char path[0x400] = {0};
    snprintf(path, 0x3FF, "%s%s", client->path, command[1]);

    // Debug
    //printf("%s\n", path);
    
    stat(path, &st);
    
    fd = open(path, O_RDONLY);

    if (fd < 0) {
        dprintf(client->fd, "550 - no such file or directory\r\n");
        return (0);
    }

    sendfile(client->fd_data, fd, NULL, st.st_size);

    close(fd);
    close(client->fd_data);

    dprintf(client->fd, "125 Data connection already open. Transfer starting.\r\n");

    dprintf(client->fd, "226 - Transfer complete.\r\n");
    
    return (0);
}

uint8_t callback_ftp_mkd_command(ftp_server_t *server, client_t *client, char **command)
{
    if (client->state < LOGGED) {
        dprintf(client->fd, "530 - you must be logged in to use this feature.\r\n");
        return (0);
    }

    if (command[1] == NULL) {
        dprintf(client->fd, "501 - Syntax error in arguments\r\n");
        return (0);
    }

    char path[0x400] = {0};
    struct stat st = {0};
    size_t size_client_path = strlen(client->path);

    if (client->path[size_client_path - 1] == '/')
        snprintf(path, 0x3FF, "%s%s", client->path, command[1]);
    else
        snprintf(path, 0x3FF, "%s/%s", client->path, command[1]);
    
    if (mkdir(path, 0700) < 0) {
        dprintf(client->fd, "550 - Already exists or error access\r\n");
        return (1);
    }

    dprintf(client->fd, "257 '%s' directory created.\r\n", command[1]);
    
    return (0);
}

uint8_t callback_ftp_rmdir_command(ftp_server_t *server, client_t *client, char **command)
{
    if (client->state < LOGGED) {
        dprintf(client->fd, "530 - you must be logged in to use this feature.\r\n");
        return (0);
    }

    if (command[1] == NULL) {
        dprintf(client->fd, "501 - Syntax error in arguments\r\n");
        return (0);
    }

    char filename[0x400] = {0};

    snprintf(filename, 0x3FF, "%s/%s", client->path, command[1]);

    if (rmdir(filename)) {
        dprintf(client->fd, "550 - no such file or directory\r\n");
        return (1);
    }

    dprintf(client->fd, "250 Directory removed.\r\n");
    
    return (0);
}

uint8_t callback_ftp_stor_command(ftp_server_t *server, client_t *client, char **command)
{
    if (client->state < LOGGED) {
        dprintf(client->fd, "530 - you must be logged in to use this feature.\r\n");
        return (0);
    }

    if (command[1] == NULL) {
        dprintf(client->fd, "501 - Syntax error in arguments\r\n");
        return (0);
    }

    dprintf(client->fd, "125 Data connection already open. Transfer starting.\r\n");

    int fd = open(command[1], O_RDWR | O_CREAT | S_IRUSR);
    char buf[0x1000] = {0};
    size_t bytes = 0;

    while ((bytes = read(client->fd_data, buf, 0xFFF)))
        write(fd, buf, bytes);
    
    close(client->fd_data);
    close(fd);

    dprintf(client->fd, "226 - Transfer complete.\r\n");

    return (0);
}

uint8_t callback_ftp_dele_command(ftp_server_t *server, client_t *client, char **command)
{
    if (client->state < LOGGED) {
        dprintf(client->fd, "530 - you must be logged in to use this feature.\r\n");
        return (0);
    }

    if (command[1] == NULL) {
        dprintf(client->fd, "501 - Syntax error in arguments\r\n");
        return (0);
    }

    char buf[0x400] = {0};

    snprintf(buf, 0x3FF, "rm %s/%s", client->path, command[1]);

    if (system(buf) < 0) {
        dprintf(client->fd, "550 - no such file directory\r\n");
        return (1);
    }

    dprintf(client->fd, "250 - file removed.\r\n");

    return (0);
}