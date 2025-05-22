#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h> // Windows Sockets
#include <ws2tcpip.h> // Windows Sockets TCP/IP functions
#include <pthread.h>
#include <time.h>

#include "cJSON.h"

#define PORT 9000
#define BUFFER_SIZE 1024
#define HISTORY_SIZE 100

typedef struct {
    double cpu_load[HISTORY_SIZE];
    double memory_free[HISTORY_SIZE];
    int current_index;
    pthread_mutex_t mutex;
} DataHistory;

DataHistory data_history;

void init_data_history() {
    for (int i = 0; i < HISTORY_SIZE; i++) {
        data_history.cpu_load[i] = 0.0;
        data_history.memory_free[i] = 0.0;
    }
    data_history.current_index = 0;
    pthread_mutex_init(&data_history.mutex, NULL);
}

void add_data(double cpu_load, double memory_free) {
    pthread_mutex_lock(&data_history.mutex);
    data_history.cpu_load[data_history.current_index] = cpu_load;
    data_history.memory_free[data_history.current_index] = memory_free;
    data_history.current_index = (data_history.current_index + 1) % HISTORY_SIZE;
    pthread_mutex_unlock(&data_history.mutex);
}

void create_data_file() {
    FILE *fp = fopen("data.json", "w");

    pthread_mutex_lock(&data_history.mutex);
    for (int i = 0; i < HISTORY_SIZE; i++) {
        int index = (data_history.current_index + i) % HISTORY_SIZE; // Corrected Indexing
        fprintf(fp, "%d %f %f\n", i, data_history.cpu_load[index], data_history.memory_free[index]);
    }
    pthread_mutex_unlock(&data_history.mutex);

    fclose(fp);
}

void create_gnuplot_script() {
    FILE *fp = fopen("plot.gp", "w");

    fprintf(fp, "set terminal png size 800,600\n");
    fprintf(fp, "set output 'plot.png'\n");
    fprintf(fp, "set title 'System Load'\n");
    fprintf(fp, "set xlabel 'Time'\n");
    fprintf(fp, "set ylabel 'Percentage'\n");
    fprintf(fp, "plot 'data.json' using 1:2 with lines title 'CPU Load', \\\n");
    fprintf(fp, "     'data.json' using 1:3 with lines title 'Free Memory'\n");

    fclose(fp);
}

void plot_data() {
    create_data_file();
    create_gnuplot_script();
    system("gnuplot plot.gp");
}

// Client handling thread
void *handle_client(void *socket_desc) {
    SOCKET sock = *(SOCKET*)socket_desc; // Changed to SOCKET
    char buffer[BUFFER_SIZE];
    int read_size;

    while ((read_size = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[read_size] = '\0';

        cJSON *json = cJSON_Parse(buffer);
        if (json == NULL) {
            const char *error_ptr = cJSON_GetErrorPtr();
            break;
        }

        cJSON *cpu_load_json = cJSON_GetObjectItemCaseSensitive(json, "cpu_load");
        cJSON *memory_free_json = cJSON_GetObjectItemCaseSensitive(json, "memory_free");

        if (cJSON_IsNumber(cpu_load_json) && cJSON_IsNumber(memory_free_json)) {
            double cpu_load = cpu_load_json->valuedouble;
            double memory_free = memory_free_json->valuedouble;

            printf("we get: CPU: %f, Free Memory: %f\n", cpu_load, memory_free);
            add_data(cpu_load, memory_free);
            plot_data();
        }

        cJSON_Delete(json);
    }

    closesocket(sock);  // Use closesocket on Windows
    free(socket_desc);
    return NULL;
}


int main() {
    SOCKET server_fd, new_socket, *new_sock;  // Changed to SOCKET
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t thread_id;
    WSADATA wsaData; //needed for winsock

     // Initialize Winsock
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);  // Request version 2.2
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }


    init_data_history();

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) { // Changed to INVALID_SOCKET
        fprintf(stderr, "socket failed with error: %d\n", WSAGetLastError());  // Windows error reporting
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) { // Changed to SOCKET_ERROR
        fprintf(stderr, "bind failed with error: %d\n", WSAGetLastError());
        closesocket(server_fd);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 3) == SOCKET_ERROR) {  // Changed to SOCKET_ERROR
        fprintf(stderr, "listen failed with error: %d\n", WSAGetLastError());
        closesocket(server_fd);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    puts("Server listening...");

    while (1) {
        // Accept connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (int*)&addrlen)) == INVALID_SOCKET) { // Changed to SOCKET and cast addrlen
            fprintf(stderr, "accept failed with error: %d\n", WSAGetLastError());
            continue;
        }

        puts("Connection accepted");

        new_sock = malloc(sizeof(SOCKET)); // Allocate enough space for a SOCKET
        *new_sock = new_socket;

        // Create thread to handle the client
        if (pthread_create(&thread_id, NULL, handle_client, (void*) new_sock) != 0) { //Condition changed since pthread_create on windows returns 0 on success
            perror("could not create thread");
            free(new_sock);
            closesocket(new_socket);
            continue;
        }

        pthread_detach(thread_id);
    }

    closesocket(server_fd); // Use closesocket on Windows
    WSACleanup();
    pthread_mutex_destroy(&data_history.mutex);
    return 0;
}
