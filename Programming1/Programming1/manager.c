#include <unistd.h>
#include <stdio.h>
#include <sys/types.h> 
#include <stdlib.h>
#include <sys/wait.h>

int main (){
    int count = 0; //number of children process (employees) manager create

    for (int i = 0; i < 100; i++){
        pid_t result = fork();

        if (result > 0){
            count++;
            continue;
        } else if (result == 0){
            if (execl("./employee", "employee", (char *)NULL) == -1){
                perror ("execl");
                exit (101);
            }
        }else {
            perror("fork");
            break;
        }
    }
    int total_sales = 0;
    int complete_count = 0; //number of employees that completed the tasks
    
    for (int i = 0; i < count; i++){
        int status;
        pid_t fpid = wait(&status);

        if (fpid == -1){
            perror ("wait");
            continue;
        }

        if (WIFEXITED(status)){
            int employee_sale = WEXITSTATUS(status);
            if (employee_sale >= 0 && employee_sale <= 100){
                total_sales += employee_sale;
                complete_count++;
            }
        }else{
            printf ("Employee PID %d did not terminate normally.\n", fpid);
        }
    }
    if (complete_count == count){
        printf ("%d employees have sold %d insurance policies.\n", complete_count, total_sales);
    }
   
    if (complete_count > 0){ //Avoid dividing a amuber by 0
        double average = (double) total_sales / complete_count;
        printf("Average number of insurance policies sold: %.2f\n", average);
}
    return 0;
}

