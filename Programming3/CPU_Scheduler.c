
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char name[50];
    double arrive, burst, priority;
    double waiting, turnaround, response, completion;
    double first_start, remaining;
    int finished;
} Process;

/* Reset scheduling values before running each algorithm */
static void reset_process(Process *destination,
                          const Process *src, int n) {
    for (int i = 0; i < n; i++) {
        destination[i] = src[i];
        destination[i].waiting = 0.0;
        destination[i].turnaround = 0.0;
        destination[i].response = 0.0;
        destination[i].completion = 0.0;
        destination[i].first_start = -1.0;
        destination[i].remaining = destination[i].burst;
        destination[i].finished = 0;
    }
}

/* Calculate dynamic priority at a given time */
static double priority_at(const Process *p, double time) {
    double arrival_time = p->arrive < 0.1 ? 0.1 : p->arrive;
    double waiting_time = time - p->arrive;

    if (waiting_time < 0.0) {
        waiting_time = 0.0;
    }

    return 0.15 / arrival_time
         + 0.25 / p->burst
         + 0.35 * waiting_time;
}

/* Print results and averages */
static void print_processes(const char *title, Process *p, int n) {
    double total_waiting = 0.0;
    double total_turnaround = 0.0;
    double total_response = 0.0;

    printf("\n====== %s ======\n", title);
    printf("%-12s %12s %15s %15s %15s\n",
           "Process", "Waiting", "Turnaround",
           "Response", "Completion");

    for (int i = 0; i < n; i++) {
        printf("%-12s %12.3f %15.3f %15.3f %15.3f\n",
               p[i].name,
               p[i].waiting,
               p[i].turnaround,
               p[i].response,
               p[i].completion);

        total_waiting += p[i].waiting;
        total_turnaround += p[i].turnaround;
        total_response += p[i].response;
    }

    printf("%-12s %12.3f %15.3f %15.3f %15s\n",
           "Average",
           total_waiting / n,
           total_turnaround / n,
           total_response / n,
           "N/A");

    printf("=================================================================================\n");
}

/* FCFS, nonpreemptive SJF, and nonpreemptive Priority */
static void run_nonpreemptive(const Process *input, int n, int mode) {
    Process *p = malloc((size_t)n * sizeof(*p));
    int *done = calloc((size_t)n, sizeof(*done));

    if (p == NULL || done == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        free(p);
        free(done);
        return;
    }

    reset_process(p, input, n);

    double time = 0.0;
    int completed = 0;

    while (completed < n) {
        int best = -1;

        for (int i = 0; i < n; i++) {
            if (done[i] || p[i].arrive > time) {
                continue;
            }

            if (best == -1) {
                best = i;
                continue;
            }

            if (mode == 0) {
                /* FCFS: earliest arrival first */
                if (p[i].arrive < p[best].arrive) {
                    best = i;
                }
            } else if (mode == 1) {
                /* SJF: shortest burst first */
                if (p[i].burst < p[best].burst) {
                    best = i;
                }
            } else {
                /* Priority: highest calculated score first */
                double coming = priority_at(&p[i], time);
                double current = priority_at(&p[best], time);

                if (coming > current) {
                    best = i;
                }
            }
        }

        if (best == -1) {
            /* Jump forward when no process has arrived */
            double next_arrival = -1.0;

            for (int i = 0; i < n; i++) {
                if (!done[i] &&
                    (next_arrival < 0.0 ||
                     p[i].arrive < next_arrival)) {
                    next_arrival = p[i].arrive;
                }
            }

            if (next_arrival >= 0.0) {
                time = next_arrival;
            }

            continue;
        }

        p[best].priority = priority_at(&p[best], time);
        p[best].first_start = time;
        p[best].response = time - p[best].arrive;
        p[best].waiting = p[best].response;

        time += p[best].burst;

        p[best].completion = time;
        p[best].turnaround = time - p[best].arrive;

        done[best] = 1;
        completed++;
    }

    const char *title =
        mode == 0 ? "FCFS" :
        mode == 1 ? "SJF" : "Priority";

    print_processes(title, p, n);

    free(p);
    free(done);
}
/* Round Robin scheduling */
static void run_round_robin(const Process *input,
                            int n, double quantum) {
    Process *p = malloc((size_t)n * sizeof(*p));
    int *queue = malloc((size_t)(n + 1) * sizeof(*queue));
    int *in_queue = calloc((size_t)n, sizeof(*in_queue));

    if (p == NULL || queue == NULL || in_queue == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        free(p);
        free(queue);
        free(in_queue);
        return;
    }

    reset_process(p, input, n);

    int head = 0;
    int tail = 0;
    int count = 0;
    int completed = 0;

    double time = 0.0;

    while (completed < n) {
        /* Add newly arrived processes to the ready queue */
        for (int i = 0; i < n; i++) {
            if (!p[i].finished &&
                !in_queue[i] &&
                p[i].remaining > 0.0 &&
                p[i].arrive <= time) {

                queue[tail] = i;
                tail = (tail + 1) % (n + 1);
                in_queue[i] = 1;
                count++;
            }
        }

        /* If the queue is empty, jump to the next arrival */
        if (count == 0) {
            double next_arrival = -1.0;

            for (int i = 0; i < n; i++) {
                if (!p[i].finished &&
                    p[i].remaining > 0.0 &&
                    (next_arrival < 0.0 ||
                     p[i].arrive < next_arrival)) {

                    next_arrival = p[i].arrive;
                }
            }

            if (next_arrival < 0.0) {
                break;
            }

            time = next_arrival;
            continue;
        }

        /* Remove the next process from the ready queue */
        int i = queue[head];
        head = (head + 1) % (n + 1);
        count--;
        in_queue[i] = 0;

        /* Record response time on the first CPU visit */
        if (p[i].first_start < 0.0) {
            p[i].first_start = time;
            p[i].response = time - p[i].arrive;
        }

        /* Execute for one quantum or until completion */
        double exec_time =
            p[i].remaining < quantum
                ? p[i].remaining
                : quantum;

        time += exec_time;
        p[i].remaining -= exec_time;

        /* Mark the process complete before updating the queue */
        if (p[i].remaining <= 1e-9) {
            p[i].remaining = 0.0;
            p[i].finished = 1;
            p[i].completion = time;
            p[i].turnaround = time - p[i].arrive;
            p[i].waiting = p[i].turnaround - p[i].burst;
            p[i].priority = priority_at(&p[i], time);

            completed++;
        }

        /* Add processes that arrived during the time slice */
        for (int j = 0; j < n; j++) {
            if (!p[j].finished &&
                !in_queue[j] &&
                p[j].remaining > 0.0 &&
                p[j].arrive <= time) {

                queue[tail] = j;
                tail = (tail + 1) % (n + 1);
                in_queue[j] = 1;
                count++;
            }
        }

        /* Requeue the running process if it still needs CPU time */
        if (!p[i].finished && p[i].remaining > 0.0) {
            queue[tail] = i;
            tail = (tail + 1) % (n + 1);
            in_queue[i] = 1;
            count++;
        }
    }

    print_processes("Round Robin", p, n);

    free(p);
    free(queue);
    free(in_queue);
}

int main(void) {
    int n;

    printf("Enter the number of processes: ");

    if (scanf("%d", &n) != 1 || n <= 0) {
        fprintf(stderr, "Invalid number of processes.\n");
        return 1;
    }

    Process *p = calloc((size_t)n, sizeof(*p));

    if (p == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        return 1;
    }

    for (int i = 0; i < n; i++) {
        printf("\nEnter process %d name: ", i + 1);

        if (scanf("%49s", p[i].name) != 1) {
            fprintf(stderr, "Invalid process name.\n");
            free(p);
            return 1;
        }

        printf("Enter process %d arrival time: ", i + 1);

        if (scanf("%lf", &p[i].arrive) != 1 ||
            p[i].arrive < 0.0) {
            fprintf(stderr, "Invalid arrival time.\n");
            free(p);
            return 1;
        }

        printf("Enter process %d burst time: ", i + 1);

        if (scanf("%lf", &p[i].burst) != 1 ||
            p[i].burst <= 0.0) {
            fprintf(stderr, "Invalid burst time.\n");
            free(p);
            return 1;
        }

        p[i].priority = 0.0;
    }

    double quantum;

    printf("\nEnter the time quantum for Round Robin scheduling: ");

    if (scanf("%lf", &quantum) != 1 || quantum <= 0.0) {
        fprintf(stderr, "Invalid time quantum.\n");
        free(p);
        return 1;
    }

    run_nonpreemptive(p, n, 0);  /* FCFS */
    run_nonpreemptive(p, n, 1);  /* SJF */
    run_nonpreemptive(p, n, 2);  /* Priority */
    run_round_robin(p, n, quantum);

    free(p);
    return 0;
}
