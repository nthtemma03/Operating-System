#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

int main(){

    pid_t result = fork();

    if (result > 0){
        //Parent 
        // Parent terminates so the child becomes an orphan
        printf ("Parent PID: %d is terminating.\n", getpid());
        exit (0);
    }else if (result == 0){
        //Child
        printf ("Child PID: %d.\n", getpid());
        sleep(1);
        printf("Child PID: %d. Parent PID after termination: %d.\n", getpid(), getppid());
        while (1){
            sleep(1);
        }
    }else{
        perror("fork");
        return 1;
    }
    return 0;
}
