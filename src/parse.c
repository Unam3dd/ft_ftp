#include "ft_ftp.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define IS_BACK(str) (str[0] == '.' && str[1] == '.')

uint32_t split_command(char *buffer, char **command, char *delim)
{
    char *token = strtok(buffer, delim);
    int i = 0;

    while (token) {
        command[i++] = token;
        token = strtok(NULL, delim);
    }

    command[i] = NULL;

    return (i);
}

void parse_port_command(char *buffer, char *ip, uint16_t *port)
{
    char *argv[0x8] = {0};

    split_command(buffer, argv, ",");

    for (int i = 0; i < 4 && argv[i]; i++) {
        strncat(ip, argv[i], strlen(argv[i]));

        if (i < 3)
            strcat(ip, ".");
    }
    
    *port = ((atoi(argv[4]) << 8) + atoi(argv[5]) & 0xFFFF);
}

char *reverse_str_stop_at(char *buffer, char at, size_t size)
{
    char *tmp = (buffer + size) - 2;

    while (*tmp && *tmp != at)
        *(tmp)-- = 0;
    
    return (buffer);
}

char *replace_chr(char *str, char old, char new)
{
    char *tmp = str;

    while (*tmp) {
        if (*tmp == old)
            *tmp = new;
        tmp++;
    }
    
    return (str);
}

void parse_dir(char *root_path, char *client_path, char *new_path, size_t path_max)
{
    char *tmp = client_path, *fmt = client_path;

    while (*fmt)
        fmt++;
    
    if (*(fmt - 1) != '/')
        *fmt = '/';

    if ((strlen(new_path) == 1) && (new_path[0] == '/')) {
        strncpy(client_path, root_path, path_max);
        return;
    }

    if (*new_path == '/')
        new_path++;

    while (*tmp && --path_max)
        tmp++;
    
    while (*new_path) {

        while ((new_path[0] == '.') && (new_path[1] == '.')) {

            if (strcmp(root_path, client_path) == 0)
                return;
            
            tmp = (*(tmp - 1) == '/') ? tmp - 2 : tmp - 1;

            while (*tmp && *tmp != '/')
                *tmp-- = 0;
        
            new_path += 2;

            if (*new_path == '/')
                new_path++;
        }

        if (*(tmp - 1) != '/')
            *tmp++ = '/';
    
        while (*new_path && !IS_BACK(new_path) && --path_max)
            *tmp++ = *new_path++;
    
        if (*(tmp - 1) != '/')
            *tmp++ = '/';

        *tmp = 0;
    }
}