
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    FILE *file;
    int processes, resources, vertices;
    int *matrix;
    int *request;
    int *allocation;
    int *available;
    int *work;
    int *finish;
    int *sequence;
    int count = 0;
    int deadlock = 0;

    char line[256];

    if (argc != 2) {
        printf("Usage: %s graph.txt\n", argv[0]);
        return 1;
    }

    file = fopen(argv[1], "r");

    if (file == NULL) {
        printf("Error: Cannot open file.\n");
        return 1;
    }

    if (fgets(line, sizeof(line), file) == NULL ||
        sscanf(line, "%d", &processes) != 1 ||
        fgets(line, sizeof(line), file) == NULL ||
        sscanf(line, "%d", &resources) != 1) {
        printf("Error: Invalid file header.\n");
        fclose(file);
        return 1;
    }

    if (processes <= 0 || resources <= 0 ||
        processes + resources > 1000) {
        printf("Error: Invalid number of processes or resources.\n");
        fclose(file);
        return 1;
    }

    vertices = processes + resources;

    if (fgets(line, sizeof(line), file) == NULL) {
        printf("Error: Missing matrix description.\n");
        fclose(file);
        return 1;
    }

    matrix = calloc((size_t)vertices * vertices, sizeof(int));
    request = calloc((size_t)processes * resources, sizeof(int));
    allocation = calloc((size_t)processes * resources, sizeof(int));
    available = calloc(resources, sizeof(int));
    work = calloc(resources, sizeof(int));
    finish = calloc(processes, sizeof(int));
    sequence = calloc(processes, sizeof(int));

    if (matrix == NULL || request == NULL || allocation == NULL ||
        available == NULL || work == NULL || finish == NULL ||
        sequence == NULL) {
        printf("Error: Memory allocation failed.\n");
        fclose(file);
        free(matrix);
        free(request);
        free(allocation);
        free(available);
        free(work);
        free(finish);
        free(sequence);
        return 1;
    }

    for (int i = 0; i < vertices; i++) {
        for (int j = 0; j < vertices; j++) {
            if (fscanf(file, "%d", &matrix[i * vertices + j]) != 1 ||
                (matrix[i * vertices + j] != 0 &&
                 matrix[i * vertices + j] != 1)) {
                printf("Error: Invalid adjacency matrix.\n");
                fclose(file);
                free(matrix);
                free(request);
                free(allocation);
                free(available);
                free(work);
                free(finish);
                free(sequence);
                return 1;
            }
        }
    }

    fclose(file);

    // REQUEST AND ALLOCATION MATRICES
    for (int i = 0; i < processes; i++) {
        for (int j = 0; j < resources; j++) {
            request[i * resources + j] =
                matrix[i * vertices + processes + j];

            allocation[i * resources + j] =
                matrix[(processes + j) * vertices + i];
        }
    }

    // AVAILABLE RESOURCES
    for (int j = 0; j < resources; j++) {
        available[j] =
            matrix[(processes + j) * vertices + processes + j];

        work[j] = available[j];
    }

    printf("\nResource Allocation Information\n");

    for (int j = 0; j < resources; j++) {
        int allocated = 0;

        for (int i = 0; i < processes; i++) {
            allocated += allocation[i * resources + j];
        }

        printf("R%d: Allocated = %d, Available = %d, Total = %d\n",
               j + 1, allocated, available[j],
               allocated + available[j]);
    }

    // INITIALIZE FINISH ARRAY
    for (int i = 0; i < processes; i++) {
        int has_allocation = 0;

        for (int j = 0; j < resources; j++) {
            if (allocation[i * resources + j] > 0) {
                has_allocation = 1;
                break;
            }
        }

        finish[i] = !has_allocation;
    }

    // DEADLOCK DETECTION
    printf("\nDeadlock Detection\n");

    while (1) {
        int found = 0;

        for (int i = 0; i < processes; i++) {
            if (finish[i]) {
                continue;
            }

            int can_finish = 1;

            for (int j = 0; j < resources; j++) {
                if (request[i * resources + j] > work[j]) {
                    can_finish = 0;
                    break;
                }
            }

            if (can_finish) {
                printf("P%d can finish.\n", i + 1);

                for (int j = 0; j < resources; j++) {
                    work[j] += allocation[i * resources + j];
                }

                finish[i] = 1;
                sequence[count++] = i;
                found = 1;
            }
        }

        if (!found) {
            break;
        }
    }

    // DISPLAY RESULT
    for (int i = 0; i < processes; i++) {
        if (!finish[i]) {
            deadlock = 1;
            break;
        }
    }

    printf("\nResult:\n");

    if (deadlock) {
        printf("Deadlock detected.\n");
        printf("Deadlocked processes: ");

        for (int i = 0; i < processes; i++) {
            if (!finish[i]) {
                printf("P%d ", i + 1);
            }
        }

        printf("\n");
    } else {
        printf("No deadlock detected.\n");

        printf("Safe completion sequence: ");

        for (int i = 0; i < count; i++) {
            printf("P%d", sequence[i] + 1);

            if (i < count - 1) {
                printf(" -> ");
            }
        }

        printf("\n");
    }

    free(matrix);
    free(request);
    free(allocation);
    free(available);
    free(work);
    free(finish);
    free(sequence);

    return 0;
}
