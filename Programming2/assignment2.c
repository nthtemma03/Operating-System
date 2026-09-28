// Name: Thuan Nguyen
//Student ID: 11594515

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>

typedef struct {
    int client;
    int (*reported)[12];
    int (*resolved)[12];
    int total_reported;
    int total_resolved;
} ClientData;

void *calculate_totals(void *arg) {
    ClientData *data = (ClientData *)arg;

    int client = data->client;

    // Initialize the totals stored in the structure
    data->total_reported = 0;
    data->total_resolved = 0;

    // Calculate the annual totals for this client
    for (int month = 0; month < 12; month++) {
        data->total_reported += data->reported[client][month];
        data->total_resolved += data->resolved[client][month];
    }

    return NULL;
}

int main() {

    int reported[5][12];
    int resolved[5][12];

    pthread_t threads[5];
    ClientData client_data[5];

    // Seed the random number generator
    srand(time(NULL));

    // Generate random issue data for 5 clients and 12 months
    for (int client = 0; client < 5; client++) {

        printf("-------------- CLIENT %d --------------\n", client + 1);

        for (int month = 0; month < 12; month++) {

            // Generate reported issues from 0 to 9
            reported[client][month] = rand() % 10;

            // Generate resolved issues from 0 to reported issues
            resolved[client][month] =
                rand() % (reported[client][month] + 1);

            printf("Month %d: Reported = %d, Resolved = %d\n",
                   month + 1,
                   reported[client][month],
                   resolved[client][month]);
        }

        printf("\n");
    }

    // Create one thread for each client
    for (int i = 0; i < 5; i++) {

        client_data[i].client = i;
        client_data[i].reported = reported;
        client_data[i].resolved = resolved;

        pthread_create(
            &threads[i],
            NULL,
            calculate_totals,
            &client_data[i]
        );
    }

    // Wait for all threads to finish
    for (int i = 0; i < 5; i++) {
        pthread_join(threads[i], NULL);
    }

    // Calculate combined totals for all clients
    int total_reported = 0;
    int total_resolved = 0;

    printf("\n=========== ANNUAL TOTALS ============\n");

    for (int i = 0; i < 5; i++) {

        printf("Client %d: Reported = %d, Resolved = %d\n",
               client_data[i].client + 1,
               client_data[i].total_reported,
               client_data[i].total_resolved);

        total_reported += client_data[i].total_reported;
        total_resolved += client_data[i].total_resolved;
    }

    printf("--------------------------------------\n");

    printf("All Clients: Reported = %d, Resolved = %d\n",
           total_reported,
           total_resolved);

    return 0;
}