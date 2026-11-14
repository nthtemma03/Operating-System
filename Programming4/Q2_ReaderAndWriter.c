
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <signal.h>
#include <semaphore.h>
#include <poll.h>
#include <errno.h>
#include <string.h>

#define MAX_READERS 7
#define TOTAL_READERS 10
#define TOTAL_WRITERS 2
#define READ_DURATION 8
#define WRITE_DURATION 3
#define WRITER_TIME_LIMIT 3

typedef struct {
    int a_readers;
    int a_writers;
    int w_writers;
    pid_t reader_pids[TOTAL_READERS];
    time_t deadline;
} ShareData;

typedef struct {
    int writer_id;
    int time_limit;
} WriterRequest;

typedef struct {
    int time_limit;
    time_t deadline;
} ReaderNotification;

sem_t *mutex;
sem_t *document_lock;
sem_t *reader_slots;
sem_t *writer_gate;

void wait_sem(sem_t *sem) {
    while (sem_wait(sem) == -1) {
        if (errno != EINTR) {
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
    }
}

void write_all(int fd, const void *buffer, size_t size) {
    const char *ptr = buffer;
    size_t total = 0;

    while (total < size) {
        ssize_t n = write(fd, ptr + total, size - total);

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("write");
            exit(EXIT_FAILURE);
        }

        total += n;
    }
}

void read_all(int fd, void *buffer, size_t size) {
    char *ptr = buffer;
    size_t total = 0;

    while (total < size) {
        ssize_t n = read(fd, ptr + total, size - total);

        if (n == 0) {
            fprintf(stderr, "Pipe closed unexpectedly.\n");
            exit(EXIT_FAILURE);
        }

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("read");
            exit(EXIT_FAILURE);
        }

        total += n;
    }
}

ShareData *initialize_shared_memory(void) {
    ShareData *shared_data = mmap(
        NULL,
        sizeof(ShareData),
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS,
        -1,
        0
    );

    if (shared_data == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    memset(shared_data, 0, sizeof(ShareData));

    return shared_data;
}

void initialize_semaphores(void) {
    sem_unlink("/reader_writer_mutex");
    sem_unlink("/reader_writer_document_lock");
    sem_unlink("/reader_writer_reader_slots");
    sem_unlink("/reader_writer_writer_gate");

    mutex = sem_open(
        "/reader_writer_mutex",
        O_CREAT | O_EXCL,
        0644,
        1
    );

    document_lock = sem_open(
        "/reader_writer_document_lock",
        O_CREAT | O_EXCL,
        0644,
        1
    );

    reader_slots = sem_open(
        "/reader_writer_reader_slots",
        O_CREAT | O_EXCL,
        0644,
        MAX_READERS
    );

    writer_gate = sem_open(
        "/reader_writer_writer_gate",
        O_CREAT | O_EXCL,
        0644,
        1
    );

    if (mutex == SEM_FAILED ||
        document_lock == SEM_FAILED ||
        reader_slots == SEM_FAILED ||
        writer_gate == SEM_FAILED) {

        perror("sem_open");
        exit(EXIT_FAILURE);
    }
}

void reader_process(
    ShareData *shared_data,
    int reader_id,
    int notify_fd
) {
    pid_t my_pid = getpid();
    int registered = 0;

    wait_sem(reader_slots);

    while (!registered) {
        wait_sem(writer_gate);
        wait_sem(mutex);

        if (shared_data->w_writers > 0) {
            sem_post(mutex);
            sem_post(writer_gate);
            sleep(1);
            continue;
        }

        if (shared_data->a_readers == 0) {
            wait_sem(document_lock);
        }

        int slot = reader_id -1;

        shared_data->reader_pids[slot] = my_pid;
        shared_data->a_readers++;

        registered = 1;

        printf(
            "Reader %d (PID: %d) started reading.\n",
            reader_id,
            my_pid
        );
        fflush(stdout);

        sem_post(mutex);
        sem_post(writer_gate);
    }

    time_t finish_time = time(NULL) + READ_DURATION;
    int reading = 1;

    while (reading) {
        time_t now = time(NULL);

        if (now >= finish_time) {
            break;
        }

        int timeout = (int)((finish_time - now) * 1000);

        if (timeout > 200) {
            timeout = 200;
        }

        struct pollfd pfd;

        pfd.fd = notify_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int result = poll(&pfd, 1, timeout);

        if (result > 0 && (pfd.revents & POLLIN)) {
            ReaderNotification notification;

            read_all(
                notify_fd,
                &notification,
                sizeof(notification)
            );

            printf(
                "Reader %d received notification: %d seconds to finish.\n",
                reader_id,
                notification.time_limit
            );
            fflush(stdout);

            if (notification.time_limit == 0) {
                break;
            }

            finish_time = notification.deadline;
        }

        if (result == -1 && errno != EINTR) {
            perror("poll");
            break;
        }
    }

    wait_sem(mutex);
    shared_data->reader_pids[reader_id - 1] = 0;
    shared_data->a_readers--;

    if (shared_data->a_readers == 0) {
        sem_post(document_lock);
    }

    sem_post(reader_slots);
    sem_post(mutex);

    printf(
        "Reader %d (PID: %d) finished reading.\n",
        reader_id,
        my_pid
    );
    fflush(stdout);

    close(notify_fd);
    exit(EXIT_SUCCESS);
}

void writer_process(
    ShareData *shared_data,
    int writer_id,
    int request_fd,
    int grant_fd
) {
    wait_sem(mutex);

    shared_data->w_writers++;

    sem_post(mutex);

    WriterRequest request;

    request.writer_id = writer_id;
    request.time_limit = WRITER_TIME_LIMIT;

    write_all(
        request_fd,
        &request,
        sizeof(request)
    );

    int grant;

    read_all(grant_fd, &grant, sizeof(grant));

    wait_sem(writer_gate);
    wait_sem(document_lock);

    wait_sem(mutex);

    shared_data->w_writers--;
    shared_data->a_writers++;

    sem_post(mutex);

    printf(
        "Writer %d (PID: %d) started writing.\n",
        writer_id,
        getpid()
    );
    fflush(stdout);

    sleep(WRITE_DURATION);

    wait_sem(mutex);

    shared_data->a_writers--;

    sem_post(mutex);

    printf(
        "Writer %d (PID: %d) finished writing.\n",
        writer_id,
        getpid()
    );
    fflush(stdout);

    sem_post(document_lock);
    sem_post(writer_gate);

    close(request_fd);
    close(grant_fd);

    exit(EXIT_SUCCESS);
}

int active_reader_count(ShareData *shared_data) {
    int count;

    wait_sem(mutex);

    count = shared_data->a_readers;

    sem_post(mutex);

    return count;
}

int find_reader_pipe(
    ShareData *shared_data,
    pid_t reader_pid
) {
    for (int i = 0; i < TOTAL_READERS; i++) {
        if (shared_data->reader_pids[i] == reader_pid) {
            return i;
        }
    }

    return -1;
}

void handle_writer_request(
    ShareData *shared_data,
    WriterRequest request,
    int notification_pipes[TOTAL_READERS][2]
) {
    printf(
        "\nParent received Writer %d request. Time limit: %d seconds.\n",
        request.writer_id,
        request.time_limit
    );
    fflush(stdout);

    time_t deadline = time(NULL) + request.time_limit;

    wait_sem(mutex);

    shared_data->deadline = deadline;

    for (int i = 0; i < TOTAL_READERS; i++) {
        pid_t reader_pid = shared_data->reader_pids[i];

        if (reader_pid != 0) {
            int pipe_index = find_reader_pipe(
                shared_data,
                reader_pid
            );

            if (pipe_index != -1) {
                ReaderNotification notification;

                notification.time_limit = request.time_limit;
                notification.deadline = deadline;

                write_all(
                    notification_pipes[pipe_index][1],
                    &notification,
                    sizeof(notification)
                );
            }
        }
    }

    sem_post(mutex);

    // Wait for readers to finish
    while (request.time_limit > 0) {
        if (active_reader_count(shared_data) == 0) {
            break;
        }

        if (time(NULL) >= deadline) {
            break;
        }

        struct timespec delay;

        delay.tv_sec = 0;
        delay.tv_nsec = 100000000;

        nanosleep(&delay, NULL);
    }

    // Give readers time to release their claims
    if (request.time_limit > 0) {
        time_t cleanup_deadline = time(NULL) + 1;

        while (active_reader_count(shared_data) > 0 &&
               time(NULL) < cleanup_deadline) {

            struct timespec delay;

            delay.tv_sec = 0;
            delay.tv_nsec = 100000000;

            nanosleep(&delay, NULL);
        }
    }

    // Force remaining readers to stop
    wait_sem(mutex);

    for (int i = 0; i < TOTAL_READERS; i++) {
        pid_t reader_pid = shared_data->reader_pids[i];

        if (reader_pid != 0) {
            printf(
                "Parent terminating Reader PID %d.\n",
                reader_pid
            );
            fflush(stdout);

            kill(reader_pid, SIGKILL);

            shared_data->reader_pids[i] = 0;
            shared_data->a_readers--;

            sem_post(reader_slots);
        }
    }

    if (shared_data->a_readers == 0) {
        sem_post(document_lock);
    }

    shared_data->deadline = 0;

    sem_post(mutex);

    printf(
        "Parent grants Writer %d permission to write.\n\n",
        request.writer_id
    );
    fflush(stdout);
}


int main(void) {
    signal(SIGPIPE, SIG_IGN);

    ShareData *shared_data = initialize_shared_memory();

    initialize_semaphores();

    int notification_pipes[TOTAL_READERS][2];
    int grant_pipes[TOTAL_WRITERS][2];
    int request_pipe[2];

    pid_t reader_pids[TOTAL_READERS];
    pid_t writer_pids[TOTAL_WRITERS];

    // Create reader notification pipes
    for (int i = 0; i < TOTAL_READERS; i++) {
        if (pipe(notification_pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Create writer grant pipes
    for (int i = 0; i < TOTAL_WRITERS; i++) {
        if (pipe(grant_pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Create writer request pipe
    if (pipe(request_pipe) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // Create reader processes
    for (int i = 0; i < TOTAL_READERS; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {
            for (int j = 0; j < TOTAL_READERS; j++) {
                close(notification_pipes[j][1]);

                if (j != i) {
                    close(notification_pipes[j][0]);
                }
            }

            for (int j = 0; j < TOTAL_WRITERS; j++) {
                close(grant_pipes[j][0]);
                close(grant_pipes[j][1]);
            }

            close(request_pipe[0]);
            close(request_pipe[1]);

            reader_process(
                shared_data,
                i + 1,
                notification_pipes[i][0]
            );
        }

        reader_pids[i] = pid;

        printf(
            "Parent created Reader %d with PID %d\n",
            i + 1,
            pid
        );

        close(notification_pipes[i][0]);
    }

    // Create writer processes
    for (int i = 0; i < TOTAL_WRITERS; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {
            for (int j = 0; j < TOTAL_READERS; j++) {
                close(notification_pipes[j][0]);
                close(notification_pipes[j][1]);
            }

            close(request_pipe[0]);

            for (int j = 0; j < TOTAL_WRITERS; j++) {
                if (j != i) {
                    close(grant_pipes[j][0]);
                }

                close(grant_pipes[j][1]);
            }

            writer_process(
                shared_data,
                i + 1,
                request_pipe[1],
                grant_pipes[i][0]
            );
        }

        writer_pids[i] = pid;

        printf(
            "Parent created Writer %d with PID %d\n",
            i + 1,
            pid
        );

        close(grant_pipes[i][0]);
    }

    close(request_pipe[1]);

    // Handle writer requests
    for (int i = 0; i < TOTAL_WRITERS; i++) {
        WriterRequest request;

        read_all(
            request_pipe[0],
            &request,
            sizeof(request)
        );

        handle_writer_request(
            shared_data,
            request,
            notification_pipes
        );

        int grant = 1;

        write_all(
            grant_pipes[request.writer_id - 1][1],
            &grant,
            sizeof(grant)
        );
    }

    close(request_pipe[0]);

    // Wait for reader processes
    for (int i = 0; i < TOTAL_READERS; i++) {
        waitpid(reader_pids[i], NULL, 0);
    }

    // Wait for writer processes
    for (int i = 0; i < TOTAL_WRITERS; i++) {
        waitpid(writer_pids[i], NULL, 0);
    }

    // Close remaining pipes
    for (int i = 0; i < TOTAL_READERS; i++) {
        close(notification_pipes[i][1]);
    }

    for (int i = 0; i < TOTAL_WRITERS; i++) {
        close(grant_pipes[i][1]);
    }

    printf("\nAll reader and writer processes have finished.\n");

    // Clean up semaphores
    sem_close(mutex);
    sem_close(document_lock);
    sem_close(reader_slots);
    sem_close(writer_gate);

    sem_unlink("/reader_writer_mutex");
    sem_unlink("/reader_writer_document_lock");
    sem_unlink("/reader_writer_reader_slots");
    sem_unlink("/reader_writer_writer_gate");

    // Clean up shared memory
    munmap(shared_data, sizeof(ShareData));

    return 0;
}
