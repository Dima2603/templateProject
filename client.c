#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h> 
#include <ws2tcpip.h> 
#include <time.h>

#include "cJSON.h"

#define PORT 9000
#define SERVER_ADDRESS "127.0.0.1"
#define SLEEP_TIME 1

double get_cpu_load() {
    return (double)(rand() % 100);
}

double get_free_memory() {
    return (double)(rand() % 100);
}

int main() {
    SOCKET sock = INVALID_SOCKET; 
    struct sockaddr_in serv_addr;
    WSADATA wsaData;

    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

    sock = socket(AF_INET, SOCK_STREAM, 0);

    memset(&serv_addr, 0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    serv_addr.sin_port = htons(PORT);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
        closesocket(sock);
        WSACleanup();
        return -1;
    }

    srand(time(NULL));

    char buffer[1024]; 

    while (1) {
        double cpu_load = get_cpu_load();
        double memory_free = get_free_memory();

        cJSON *json = cJSON_CreateObject();
        cJSON_AddNumberToObject(json, "cpu_load", cpu_load);
        cJSON_AddNumberToObject(json, "memory_free", memory_free);

        char *json_str = cJSON_PrintUnformatted(json);
        if (json_str == NULL) {
            fprintf(stderr, "Failed to print JSON.\n");
            cJSON_Delete(json);
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        snprintf(buffer, sizeof(buffer), "%s", json_str);
        send(sock, buffer, (int)strlen(buffer), 0);  

        printf("Sent: CPU Load: %f, Free Memory: %f\n", cpu_load, memory_free);

        cJSON_Delete(json);
        free(json_str);

        Sleep(SLEEP_TIME * 2000);  
    }

    closesocket(sock); 
    WSACleanup();  
    return 0;
}
