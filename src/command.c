#include "ft_ftp.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

uint8_t ftp_accept_message(client_t *client)
{
   dprintf(client->fd, "220 - Welcome to the FTP Server !\r\n");
   client->state = CONNECTED;
    
   return (0);
}

uint8_t ftp_command_not_implemented(client_t *client)
{
    if (client->state < LOGGED) {
        dprintf(client->fd, "530 - you must be logged in to use this feature.\r\n");
        return (0);
    }

    dprintf(client->fd, "502 - Command not implemented.\r\n");

    return (0);
}