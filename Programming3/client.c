
#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_PORT 5050
#define LOCAL_CAPACITY 5
#define ITEM_LEN 256
#define SEM_NAME_LEN 80

typedef struct {
    char items[LOCAL_CAPACITY][ITEM_LEN];
    int head, tail, count, stopping;

    sem_t *mutex;
    sem_t *empty;
    sem_t *full;

    char mutex_name[SEM_NAME_LEN];
    char empty_name[SEM_NAME_LEN];
    char full_name[SEM_NAME_LEN];
} LocalBuffer;

static sem_t *open_named_sem(const char *name,
                             unsigned int initial_value) {
    sem_unlink(name);

    sem_t *s = sem_open(name, O_CREAT | O_EXCL, 0600,
                        initial_value);

    if (s == SEM_FAILED) {
        perror("sem_open");
        return NULL;
    }

    return s;
}

static LocalBuffer *make_buffer(void) {
    LocalBuffer *b = mmap(NULL, sizeof(*b),
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_ANON,
                          -1, 0);

    if (b == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    memset(b, 0, sizeof(*b));

    long pid = (long)getpid();

    snprintf(b->mutex_name, sizeof(b->mutex_name),
             "/pa3_cli_%ld_mutex", pid);
    snprintf(b->empty_name, sizeof(b->empty_name),
             "/pa3_cli_%ld_empty", pid);
    snprintf(b->full_name, sizeof(b->full_name),
             "/pa3_cli_%ld_full", pid);

    b->mutex = open_named_sem(b->mutex_name, 1);
    b->empty = open_named_sem(b->empty_name, LOCAL_CAPACITY);
    b->full = open_named_sem(b->full_name, 0);

    if (b->mutex == NULL || b->empty == NULL ||
        b->full == NULL) {
        if (b->mutex != NULL) sem_close(b->mutex);
        if (b->empty != NULL) sem_close(b->empty);
        if (b->full != NULL) sem_close(b->full);

        sem_unlink(b->mutex_name);
        sem_unlink(b->empty_name);
        sem_unlink(b->full_name);
        munmap(b, sizeof(*b));
        exit(EXIT_FAILURE);
    }

    return b;
}

static int send_all(int fd, const char *buf, size_t len) {
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);

        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }

        if (n == 0) return -1;
        sent += (size_t)n;
    }

    return 0;
}

static int send_line(int fd, const char *line) {
    return send_all(fd, line, strlen(line));
}

static int read_line(int fd, char *out, size_t size) {
    size_t used = 0;

    if (size < 2) return -1;

    while (used + 1 < size) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);

        if (n == 0)
            return used == 0 ? 0 : (out[used] = '\0', 1);

        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }

        if (c == '\n') break;
        if (c != '\r') out[used++] = c;
    }

    out[used] = '\0';
    return 1;
}

static int connect_server(const char *host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr,
                "Use an IPv4 address such as 127.0.0.1.\n");
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr,
                sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }

    return fd;
}

/* Child process: send queued requests to the server. */
static void communication_process(LocalBuffer *b,
                                  const char *host, int port) {
    signal(SIGPIPE, SIG_IGN);

    int fd = connect_server(host, port);

    if (fd < 0)
        _exit(2);

    while (1) {
        if (sem_wait(b->mutex) < 0)
            _exit(EXIT_FAILURE);

        int stopping = b->stopping;
        int has_data = b->count > 0;

        sem_post(b->mutex);

        if (stopping && !has_data)
            break;

        if (!has_data) {
            struct timespec pause_time = {0, 100000000L};
            nanosleep(&pause_time, NULL);
            continue;
        }

        /* Ask the server for its current available capacity. */
        if (send_line(fd, "CHECK\n") < 0)
            break;

        char available_line[128];
        char status_line[256];

        if (read_line(fd, available_line,
                      sizeof(available_line)) <= 0 ||
            read_line(fd, status_line,
                      sizeof(status_line)) <= 0)
            break;

        int available = 0;

        if (sscanf(available_line, "AVAILABLE: %d",
                   &available) != 1) {
            fprintf(stderr,
                    "[Client network] Invalid server response.\n");
            break;
        }

        printf("[Client network] Server says %d slots available.\n",
               available);
        fflush(stdout);

        if (available <= 0) {
            sleep(1);
            continue;
        }

        /* Copy the oldest request without removing it yet. */
        char item[ITEM_LEN];

        if (sem_wait(b->mutex) < 0)
            _exit(EXIT_FAILURE);

        if (b->count == 0) {
            sem_post(b->mutex);
            continue;
        }

        snprintf(item, sizeof(item), "%s",
                 b->items[b->head]);

        sem_post(b->mutex);

        char request[ITEM_LEN + 8];

        int len = snprintf(request, sizeof(request),
                           "DATA %s\n", item);

        if (len < 0 || (size_t)len >= sizeof(request) ||
            send_line(fd, request) < 0)
            break;

        /* Read the acknowledgment and updated capacity. */
        if (read_line(fd, available_line,
                      sizeof(available_line)) <= 0 ||
            read_line(fd, status_line,
                      sizeof(status_line)) <= 0)
            break;

        printf("[Client network] %s | %s\n",
               available_line, status_line);
        fflush(stdout);

        if (strncmp(status_line, "ACK:", 4) == 0) {
            if (sem_wait(b->mutex) < 0)
                _exit(EXIT_FAILURE);

            if (b->count > 0) {
                b->head = (b->head + 1) % LOCAL_CAPACITY;
                --b->count;
            }

            sem_post(b->mutex);
            sem_post(b->empty);
        } else if (strncmp(status_line, "REJECTED:", 9) == 0) {
            sleep(1);
        } else {
            fprintf(stderr,
                    "[Client network] Unexpected server response.\n");
            break;
        }
    }

    send_line(fd, "QUIT\n");
    close(fd);
    _exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    const char *host = "127.0.0.1";
    int port = DEFAULT_PORT;

    if (argc >= 2)
        host = argv[1];

    if (argc >= 3) {
        char *end = NULL;
        long value = strtol(argv[2], &end, 10);

        if (*argv[2] == '\0' || *end != '\0' ||
            value < 1 || value > 65535) {
            fprintf(stderr, "Invalid port number.\n");
            return EXIT_FAILURE;
        }

        port = (int)value;
    }

    signal(SIGPIPE, SIG_IGN);

    LocalBuffer *b = make_buffer();

    pid_t child = fork();

    if (child < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (child == 0)
        communication_process(b, host, port);

    printf("Client connecting to %s:%d.\n", host, port);
    printf("Enter requests, one per line.\n");
    printf("Type /quit to stop accepting input. "
           "Local buffer capacity: %d.\n", LOCAL_CAPACITY);
    fflush(stdout);

    char input[ITEM_LEN];

    while (1) {
        printf("> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL)
            break;

        size_t input_len = strlen(input);

        if (input_len > 0 && input[input_len - 1] == '\n') {
            input[--input_len] = '\0';
        } else if (!feof(stdin)) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {}
            fprintf(stderr, "Request is too long.\n");
            continue;
        }

        if (strcmp(input, "/quit") == 0)
            break;

        if (input[0] == '\0')
            continue;

        if (sem_trywait(b->empty) != 0) {
            if (errno == EAGAIN) {
                printf("Local buffer is full. "
                       "Request was not queued.\n");
                continue;
            }

            perror("sem_trywait");
            break;
        }

        if (sem_wait(b->mutex) < 0) {
            sem_post(b->empty);
            perror("sem_wait");
            break;
        }

        snprintf(b->items[b->tail], ITEM_LEN, "%s", input);

        b->tail = (b->tail + 1) % LOCAL_CAPACITY;
        ++b->count;

        sem_post(b->mutex);
        sem_post(b->full);

        printf("Queued locally.\n");
    }

    if (sem_wait(b->mutex) == 0) {
        b->stopping = 1;
        sem_post(b->mutex);
    }

    waitpid(child, NULL, 0);

    sem_close(b->mutex);
    sem_close(b->empty);
    sem_close(b->full);

    sem_unlink(b->mutex_name);
    sem_unlink(b->empty_name);
    sem_unlink(b->full_name);

    munmap(b, sizeof(*b));

    return EXIT_SUCCESS;
}
