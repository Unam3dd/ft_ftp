#include "ft_ftp.h"
#include <time.h>
#include <stdlib.h>

uint16_t generate_random_port(int *a, int *b)
{
    int c = 0;

    while (c < 0x7530) {
        srand(time(0));
    
        *a = (150 + (rand() % 100));
        *b = (rand() % 100);

        c = (((*a) * 256) + *b) & 0xFFFF;
    }

    return (c);
}

char *append_chr(char *dst, char *src, size_t max_buffer_size)
{
    char *tmp = dst;

    while (*tmp && --max_buffer_size)
        tmp++;
    
    while (*src && --max_buffer_size)
        *tmp++ = *src++;
    
    return (dst);
}