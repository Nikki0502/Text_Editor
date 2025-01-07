#include "text_editor.h"

#define MAX_CLIENTS 2
#define BUFFER_SIZE 1024

typedef struct{
    int client_sock;
    struct sockaddr_in client_addr;
    char openFile;
}ClientInfo;

int current_num_clients = 0;

int clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

int init_socket(){
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_address;

    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);
}

void *client_hanlder(void* arg){
    ClientInfo *client_info = (ClientInfo*)arg;
    int client_sock = client_info->client_sock;
    char buffer[BUFFER_SIZE];

    while (1) {
        int bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            printf("Client disconnected\n");
            close(client_sock);
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_CLIENTS; ++i) {
                if (clients[i] == client_sock) {
                    clients[i] = 0;
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            break;
        }

        buffer[bytes_received] = '\0';
        printf("Received: %s\n", buffer);
    }

    free(client_info);
    return NULL;

}

void accept_clients(int server_socket){
    while (1){
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_socket = accept(server_socket,(struct sockaddr *)&client_addr, &client_addr_len);

        if(client_socket<0){
            perror("Accept faled");
            continue;
        }

        //is this nec?
        if(current_num_clients>=MAX_CLIENTS){
            perror("Max clients reached");
            close(client_socket);
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        for(int i=0; i<MAX_CLIENTS;i++){
            if(clients[i]==0){
                clients[i] = client_socket;
                current_num_clients ++;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        printf("New client on Port: %d, and Addr:%d  connected\n",client_addr.sin_port , client_addr.sin_addr.s_addr);

        ClientInfo *client_info = malloc(sizeof(ClientInfo));
        client_info->client_sock = client_socket;
        client_info->client_addr = client_addr;

        pthread_t thread;
        pthread_create(&thread,NULL,client_hanlder,(void *)client_info);
        pthread_detach(thread);

    }
    
}

int main(){
    int server_socket = init_socket();
}