#pragma once
#include <stdint.h>
#include <stddef.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_EVENTS_EPOLL 16
#define CONNECTED 0
#define AUTH 1
#define LOGGED 2

typedef struct ftp_server
{
    char path[0x100];
    char ip[16];
    char *username;
    char *password;
    int s_fd;
    int e_fd;
} ftp_server_t;

typedef struct client
{
    char path[0x100];
    struct sockaddr_in sin;
    char *username;
    int fd;
    int fd_data;
    uint8_t type;
    uint8_t state;
} client_t;

typedef struct ftp_command
{
    char *name;
    uint8_t (*func)(ftp_server_t *server, client_t *client, char **command);
} ftp_command_t;

// server.c
uint8_t create_ftp_server(ftp_server_t *server, const char *address, uint16_t port);
uint8_t accept_ftp_server(ftp_server_t *server, client_t *client);
uint8_t handle_client(ftp_server_t *server, client_t *client);
uint8_t check_read_client(ftp_server_t *server, client_t *client);
uint8_t close_ftp_server(ftp_server_t *server);

// utils.c
uint16_t generate_random_port(int *a, int *b);
char *append_chr(char *dst, char *src, size_t max_buffer_size);

// command.c
uint8_t ftp_accept_message(client_t *client);
uint8_t ftp_command_not_implemented(client_t *client);

// callback.c
uint8_t callback_ftp_user_command(ftp_server_t *server, client_t *client, char **command);
uint8_t callback_ftp_pass_command(ftp_server_t *server, client_t *client, char **command);
uint8_t callback_ftp_quit_command(ftp_server_t *server, client_t *client, char **command);
uint8_t callback_ftp_syst_command(ftp_server_t *server, client_t *client, char **command);
uint8_t callback_ftp_port_command(ftp_server_t *server, client_t *client, char **command);
uint8_t callback_ftp_pwd_command(ftp_server_t *server, client_t *client, char **command);
uint8_t callback_ftp_type_command(ftp_server_t *server, client_t *client, char **command);
uint8_t callback_ftp_cwd_command(ftp_server_t *server, client_t *client, char **command);
uint8_t callback_ftp_nlst_command(ftp_server_t *server, client_t *client, char **command);
uint8_t callback_ftp_list_command(ftp_server_t *server, client_t *client, char **command);
uint8_t callback_ftp_cdup_command(ftp_server_t *server, client_t *client, char **command);
uint8_t callback_ftp_noop_command(ftp_server_t *server, client_t *client, char **command);
uint8_t callback_ftp_pasv_command(ftp_server_t *server, client_t *client, char **command);
uint8_t callback_ftp_size_command(ftp_server_t *server, client_t *client, char **command);
uint8_t callback_ftp_feat_command(ftp_server_t *server, client_t *client, char **command);
uint8_t callback_ftp_retr_command(ftp_server_t *server, client_t *client, char **command);
uint8_t callback_ftp_mkd_command(ftp_server_t *server,  client_t *client,  char **command);
uint8_t callback_ftp_rmdir_command(ftp_server_t *server, client_t *client, char **command);
uint8_t callback_ftp_stor_command(ftp_server_t *server, client_t *client, char **command);
uint8_t callback_ftp_dele_command(ftp_server_t *server, client_t *client, char **command);

// parse.c
uint32_t split_command(char *buffer, char **command, char *delim);
void parse_port_command(char *buffer, char *ip, uint16_t *port);
char *reverse_str_stop_at(char *buffer, char at, size_t size);
char *replace_chr(char *str, char old, char new);
void parse_dir(char *root_path, char *client_path, char *new_path, size_t path_max);