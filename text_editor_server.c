#include "text_editor.h"

#define MAX_CLIENTS 10

struct client{
        int used;
        int client_fd;
        socklen_t client_len;
        struct sockaddr_in client_addr;
        pthread_t tid;
        int currentFile;
};

static struct client Clients[MAX_CLIENTS];


int init_socket(int port){
       int server_fd;
       struct sockaddr_in address;

       // creat a socket with AF_INET = IPv4, SOCK_STREAM = con. based, no specific protocol
       if((server_fd = socket(AF_INET,SOCK_STREAM,0))<0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
       }

        // Set socket options to reuse address
        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("Failed to set socket options");
            exit(EXIT_FAILURE);
        }

       // bind socket to PORT 
       address.sin_family = AF_INET;
       address.sin_addr.s_addr= INADDR_ANY;
       address.sin_port = htons(port);

       if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
                perror("Failed to bind socket");
                exit(EXIT_FAILURE);
        }
        printf("Bind socket on port %d succeeded\n", port);

        // set to listening
        if (listen(server_fd, MAX_CLIENTS) < 0) {
                perror("Failed to listen on socket");
                exit(EXIT_FAILURE);
        }
        printf("Listening to socket...\n");

        return server_fd;

}


void *handle_request(void *arg){
        int i = (int)((long)arg);
        char *buffer = malloc(RECV_SIZE);
        memset(buffer, 0, RECV_SIZE);  // Ensure the buffer is cleared
        void *ret = NULL;
        int err = 0;

        ssize_t recv_len = recv(Clients[i].client_fd, buffer, RECV_SIZE, 0);
        if (recv_len > 0){
                printf("Received request: %s\n", buffer);
                char* c = "insert/1.1.0.0.1.0.a/123/test2";
                if(send(Clients[i].client_fd, c, strlen(c), 0) < 0) {
                    perror("Send failed");
                }
        }
        // send empty msg to end conn 
        else if (recv_len == 0) {
                printf("Connection closed by the client.\n");
                free(buffer);
                return (void *)ENOTCONN;    
        }

        free(buffer);

        return ret;
}

void *client_handler(void *arg){
        // current client i
        int i = (int)((long)arg);
        // handle request as long as client doest send empty msg to end conn
        while (1) {
                void *ret = handle_request((void *)((long)i));
                if (ret == (void *)ENOTCONN){
                        break;
                }
        }
        // close connenction
        printf("Connection closed with %s : %d\n",inet_ntoa(Clients[i].client_addr.sin_addr), ntohs(Clients[i].client_addr.sin_port));
        close(Clients[i].client_fd);
        Clients[i].used =0;
        return NULL;
}

void accept_con(int server_fd){
    while(1){
        // check which client is unused
        for(int i=0; i<MAX_CLIENTS;i++){
        // safe information of client
            if(Clients[i].used == 0){
                Clients[i].used =1;
                Clients[i].client_len = sizeof(Clients[i].client_addr);
                // accept client
                if((Clients[i].client_fd = accept(server_fd,(struct sockaddr *)&Clients[i].client_addr,&Clients[i].client_len))<0){
                    perror("failed to accept");
                    continue;
                }
                printf("accepted connenction from %s : %d \n",inet_ntoa(Clients[i].client_addr.sin_addr),ntohs(Clients[i].client_addr.sin_port));
                // start thread for client
                pthread_create(&Clients[i].tid, NULL, client_handler, (void*)((long)i));
                
            }
        }
    }
}

int main() {
    //init socket returns file descriptor for the socket of the server
    int server_fd = init_socket(PORT);
    // waiting to accepting connections from clients
    accept_con(server_fd);
    close(server_fd);
    return 0;
}

//     gcc text_editor_server.c -o text_editor_server
//     ./text_editor_server

