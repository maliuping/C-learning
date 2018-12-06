#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include "sys.h"


// #define ERR_EXIT(m) (perror(m), exit(EXIT_FAILURE))
#define ERR_EXIT(m) \
    do { \
        perror(m); \
        exit(EXIT_FAILURE); \
    } while(0)



int main (int argc, char **argv) {
    printf("fileno(stdin) = %d\n", fileno(stdin));

    int fd;

    fd = open("test.txt", O_WRONLY | O_CREAT, 0666);
    if (fd == -1)
        ERR_EXIT("open error");
    printf("open success.\n");
    close(fd);

    return 0;
}