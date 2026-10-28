
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
#include <unistd.h>

#define PORT 5050
#define CAPACITY 5
#define ITEM_LEN 256
#define BACKLOG 5
#define SEM_NAME_LEN 80

typedef struct {
    char items[CAPACITY][ITEM_LEN];
    int head, tail, count, stopping;

    sem_t *mutex;
    sem_t *empty;
    sem_t *full;

    char mutex_name[SEM_NAME_LEN];
    char empty_name[SEM_NAME_LEN];
    char full_name[SEM_NAME_LEN];
} SharedBuffer;

static sem_t *open_named_sem(const char *name,
                             unsigned int initial_value) {
    /* Remove a stale name left by a previous crashed run. */
    sem_unlink(name);

    sem_t *s = sem_open(name, O_CREAT | O_EXCL, 0600,
                        initial_value);

    if (s == SEM_FAILED) {
        perror("sem_open");
        return NULL;
    }

    return s;
}

static SharedBuffer *shared_buffer(void) {
    SharedBuffer *b = mmap(NULL, sizeof(*b),
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
             "/pa3_srv_%ld_mutex", pid);
    snprintf(b->empty_name, sizeof(b->empty_name),
             "/pa3_srv_%ld_empty", pid);
    snprintf(b->full_name, sizeof(b->full_name),
             "/pa3_srv_%ld_full", pid);

    b->mutex = open_named_sem(b->mutex_name, 1);
    b->empty = open_named_sem(b->empty_name, CAPACITY);
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

static int send_response(int fd, SharedBuffer *b,
                         const char *message) {
    if (sem_wait(b->mutex) < 0) return -1;

    int available = CAPACITY - b->count;

    sem_post(b->mutex);

    char response[512];
    int n = snprintf(response, sizeof(response),
                     "AVAILABLE: %d\n%s\n",
                     available, message);

    if (n < 0 || (size_t)n >= sizeof(response))
        return -1;

    return send_all(fd, response, (size_t)n);
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

/* Consumer process: remove requests from the shared buffer. */
static void consumer(SharedBuffer *b) {
    for (;;) {
        while (sem_wait(b->full) < 0) {
            if (errno == EINTR) continue;
            _exit(EXIT_FAILURE);
        }

        if (sem_wait(b->mutex) < 0)
            _exit(EXIT_FAILURE);

        if (b->stopping && b->count == 0) {
            sem_post(b->mutex);
            break;
        }

        if (b->count == 0) {
            sem_post(b->mutex);
            continue;
        }

        char item[ITEM_LEN];
        snprintf(item, sizeof(item), "%s",
                 b->items[b->head]);

        b->head = (b->head + 1) % CAPACITY;
        --b->count;

        sem_post(b->mutex);
        sem_post(b->empty);

        printf("[Consumer] Processed request: %s\n", item);
        fflush(stdout);

        /* Simulate processing time to test the bounded buffer. */
        sleep(3);
    }

    _exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    int port = PORT;

    if (argc == 2) {
        char *end = NULL;
        long value = strtol(argv[1], &end, 10);

        if (*argv[1] == '\0' || *end != '\0' ||
            value < 1 || value > 65535) {
            fprintf(stderr, "Invalid port number.\n");
            return EXIT_FAILURE;
        }

        port = (int)value;
    }

    signal(SIGPIPE, SIG_IGN);

    SharedBuffer *b = shared_buffer();

    pid_t child = fork();

    if (child < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (child == 0)
        consumer(b);

    int listener = socket(AF_INET, SOCK_STREAM, 0);

    if (listener < 0) {
        perror("socket");
        kill(child, SIGTERM);
        waitpid(child, NULL, 0);
        return EXIT_FAILURE;
    }

    int yes = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
               &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);

    if (bind(listener, (struct sockaddr *)&addr,
             sizeof(addr)) < 0 ||
        listen(listener, BACKLOG) < 0) {
        perror("bind/listen");
        close(listener);
        kill(child, SIGTERM);
        waitpid(child, NULL, 0);
        return EXIT_FAILURE;
    }

    printf("Server listening on port %d "
           "(buffer capacity %d).\n", port, CAPACITY);
    fflush(stdout);

    for (;;) {
        struct sockaddr_in peer;
        socklen_t peer_len = sizeof(peer);

        int client = accept(listener,
                            (struct sockaddr *)&peer,
                            &peer_len);

        if (client < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }

        printf("Client connected.\n");
        fflush(stdout);

        char line[ITEM_LEN + 16];
        int status;

        while ((status = read_line(client, line,
                                   sizeof(line))) > 0) {
            if (strcmp(line, "CHECK") == 0) {
                if (send_response(client, b, "STATUS") < 0)
                    break;

            } else if (strncmp(line, "DATA ", 5) == 0) {
                const char *payload = line + 5;

                if (strlen(payload) >= ITEM_LEN) {
                    if (send_response(client, b,
                                      "REJECTED: request too long") < 0)
                        break;
                    continue;
                }

                if (sem_trywait(b->empty) == 0) {
                    if (sem_wait(b->mutex) < 0) {
                        sem_post(b->empty);
                        break;
                    }

                    snprintf(b->items[b->tail], ITEM_LEN,
                             "%s", payload);

                    b->tail = (b->tail + 1) % CAPACITY;
                    ++b->count;

                    sem_post(b->mutex);
                    sem_post(b->full);

                    if (send_response(client, b,
                                      "ACK: Request accepted") < 0)
                        break;
                } else {
                    if (errno == EINTR) continue;

                    if (send_response(client, b,
                                      "REJECTED: Buffer full") < 0)
                        break;
                }

            } else if (strcmp(line, "QUIT") == 0) {
                send_response(client, b, "BYE");
                break;

            } else {
                if (send_response(client, b,
                                  "ERROR: Unknown command") < 0)
                    break;
            }
        }

        close(client);
        printf("Client disconnected.\n");
        fflush(stdout);
    }

    close(listener);

    if (sem_wait(b->mutex) == 0) {
        b->stopping = 1;
        sem_post(b->mutex);
    }

    sem_post(b->full);
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
