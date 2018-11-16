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

    pid = fork();

    if (pid < 0) {
        perror("for failed!");
        exit(1);
    }
    if( pid == 0) {
        child_id = getpid();
        sprintf(message,"This is the child,my id is %d \n",child_id);
        n = 6;
    } else {
        m = pid;
        parent_id = getppid();
        sprintf(message,"This is the parent,my id is %d, my child_id is %d \n",child_id,m);
        n = 3;
    }

    for (; n > 0; n--) {
        printf(message);
        sleep(1);
    }

    return 0;
}