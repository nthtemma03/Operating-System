#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

int main (){

    pid_t result = fork ();

    if (result == 0){
        //child
        printf ("Child PID: %d is terminating.\n", getpid());
        exit (0);
    }else if (result > 0) {
        //Parent must not call wait or waitpid
        printf ("Parent PID: %d, Child PID: %d.\n", getpid(), result);
        while(1){
            sleep(5);
        }
    }
    else{
        perror("fork");
        return 1;
    }
// So Child died first but parents still waiting
    return 0;
}