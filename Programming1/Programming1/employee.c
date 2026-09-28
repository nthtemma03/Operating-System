#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h> 

int main() {

    srand(time(NULL) ^ getpid());

    int sale = rand() % 101;

    int delaytime = rand() % 401 + 200;

    struct timespec delay;
    delay.tv_sec = 0;
    delay.tv_nsec = delaytime *1000; // microsec to nanosec

    int ns = nanosleep(&delay,NULL);

    //check if nanosleep work

    if (ns == -1){
        return 1;
    }
    
    pid_t epid = getpid();
    printf("Employee PID: %d, Insurance policies sold: %d\n", epid, sale);
  
    return sale;
}