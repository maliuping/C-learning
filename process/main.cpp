#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include "sys.h"
#include <math.h>


int main() {

   /*  extern char **environ;
    int i;

    for (i=0; environ[i] != NULL; i++)
            printf("%s\n",environ[i]); */

    pid_t pid;
    char message[80];
    int n;
    pid_t parent_id;
    pid_t child_id;
    int m;
    int fd[2];
    char line[80];

    if (pipe(fd) < 0) {
        perror("pipe failed!");
        exit(1);
    }

    pid = fork();

    if (pid < 0) {
        perror("fork failed!");
        exit(1);
    }
    if( pid == 0) {
        child_id = getpid();
        sprintf(message,"This is the child,my id is %d \n",child_id);
        n = 6;
        close(fd[1]); // close write
        read(fd[0],line,80);
        printf(line);
    } else {
        m = pid;
        parent_id = getppid();
        sprintf(message,"This is the parent,my id is %d, my child_id is %d \n",child_id,m);
        n = 3;
        close(fd[0]); // close read
        write(fd[1],"hello world\n",12);
    }
/*
    for (; n > 0; n--) {
        printf(message);
        sleep(1);
    } */

    return 0;
}