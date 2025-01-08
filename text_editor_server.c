#include "text_editor.h"
#include <signal.h>

#define MAX_CLIENTS 2

struct client{
    int used;
    int client_fd;
    socklen_t client_len;
    struct sockaddr_in client_addr;
    pthread_t tid;
    int currentFile;
};

static struct client Clients[MAX_CLIENTS];
const char* dirName = "./texts";

static int server_fd = -1;  

void cleanup_socket() {
    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
    }
}
void handle_signal(int sig) {
    printf("\nReceived signal %d. Shutting down server...\n", sig);
    cleanup_socket();
    exit(0);
}

int init_socket(int port){
    int server_fd;
    struct sockaddr_in address;

       // creat a socket with AF_INET = IPv4, SOCK_STREAM = con. based, no specific protocol
    if((server_fd = socket(AF_INET,SOCK_STREAM,0))<0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
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

char* Char_to_char(Char ch) {
    int buffer_size = 100;
    char* result = malloc(buffer_size * sizeof(char));
    if (result == NULL) {
        return NULL;
    }
    
    // Use unsigned char to prevent sign extension issues
    unsigned char uvalue = (unsigned char)ch.value;
    
    snprintf(result, buffer_size, "%d.%d.%d.%d.%d.%d.%u",
        ch.id.client_id, 
        ch.id.opCounter,
        ch.at.id_added_to.client_id,
        ch.at.id_added_to.opCounter,
        ch.at.pos_in_id,
        ch.at.lampertClock, 
        uvalue);  // Use unsigned int format
    return result;
}

Message parseChar(char* ch) {
    Message msg = {0};  // Initialize all fields to 0
    char* ch_cpy = strdup(ch);
    if (!ch_cpy) {
        // Handle allocation error
        return msg;
    }

    char* saveptr1 = NULL;
    char* saveptr2 = NULL;
    char* token = strtok_r(ch_cpy, "/", &saveptr1);
    
    if (token) {
        msg.operation = strdup(token);
        token = strtok_r(NULL, "/", &saveptr1);
        
        if (token) {
            char* new_Char = token;
            // Parse the character data with a separate strtok context
            char* num = strtok_r(new_Char, ".", &saveptr2);
            if (num) msg.ch.id.client_id = atoi(num);
            num = strtok_r(NULL, ".", &saveptr2);
            if (num) msg.ch.id.opCounter = atoi(num);
            num = strtok_r(NULL, ".", &saveptr2);
            if (num) msg.ch.at.id_added_to.client_id = atoi(num);
            num = strtok_r(NULL, ".", &saveptr2);
            if (num) msg.ch.at.id_added_to.opCounter = atoi(num);
            num = strtok_r(NULL, ".", &saveptr2);
            if (num) msg.ch.at.pos_in_id = atoi(num);
            num = strtok_r(NULL, ".", &saveptr2);
            if (num) msg.ch.at.lampertClock = atoi(num);
            num = strtok_r(NULL, ".", &saveptr2);
            if (num) msg.ch.value = (char)atoi(num);  // Convert to number first
        }

        token = strtok_r(NULL, "/", &saveptr1);
        if (token) msg.curosor_off = atoi(token);
        
        token = strtok_r(NULL, "//", &saveptr1);
        if (token) msg.file = strdup(token);
        
        token = strtok_r(NULL, "/", &saveptr1);
        if (token) msg.buffer = strdup(token);
    }

    free(ch_cpy);
    return msg;
}

char* transform_for_Send(char* op, Char ch, int off, char* file, char* buff) {
    // First get the Char string representation
    char* char_str = Char_to_char(ch);
    if (!char_str) {
        return NULL;
    }

    // Calculate required buffer size more precisely
    size_t op_len = strlen(op);
    size_t char_str_len = strlen(char_str);
    size_t file_len = strlen(file);
    size_t buff_len = strlen(buff);
    
    // Add space for separators (/, //) and null terminator
    // Format: "op/char_str/off/file//buff\0"
    size_t total_size = op_len + 1 +          // op/
                        char_str_len + 1 +     // char_str/
                        32 +                   // off (as string) + /
                        file_len + 2 +         // file//
                        buff_len + 1;          // buff\0

    char* result = malloc(total_size);
    if (!result) {
        free(char_str);
        return NULL;
    }

    // Use snprintf to safely format the string
    int written = snprintf(result, total_size, "%s/%s/%d/%s//%s",
                          op, char_str, off, file, buff);

    free(char_str);  // Clean up temporary string

    // Check if the formatting was successful
    if (written < 0 || written >= total_size) {
        free(result);
        return NULL;
    }

    return result;
}

char* getDir() {
    struct dirent *entry;
    DIR *dp = opendir(dirName);
    if (dp == NULL) {
        perror("opendir");
        return NULL;
    }

    // Allocate an initial buffer for the file names
    size_t buffer_size = 1024;
    char *file_list = malloc(buffer_size);
    if (!file_list) {
        perror("malloc");
        closedir(dp);
        return NULL;
    }
    file_list[0] = '\0';  // Initialize the string

    while ((entry = readdir(dp)) != NULL) {
        // Skip . and .. directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Calculate required space: filename + newline + null terminator
        size_t required_space = strlen(file_list) + strlen(entry->d_name) + 2;
        
        // Check if the buffer needs more space
        if (required_space > buffer_size) {
            buffer_size = required_space * 2; // Double the buffer size for next iteration
            char *temp = realloc(file_list, buffer_size);
            if (!temp) {
                perror("realloc");
                free(file_list);  // Ensure the previously allocated memory is freed
                closedir(dp);
                return NULL;
            }
            file_list = temp;  // Assign the new pointer
        }

        // Append the file name and newline to the buffer
        snprintf(file_list + strlen(file_list), buffer_size - strlen(file_list), "%s\n", entry->d_name);
    }

    closedir(dp);
    return file_list;
}

char* openFile(const char* filename){}

int send_to_client(int client_sock,char* re){
    printf("Sending request: \"%s\"\n", re);
    if (send(client_sock, re, strlen(re), 0) < 0) {
        perror("Failed to send request");
        return 1;
    }
    return 0;
}

void handle_Insert(Message msg){}
void handle_List(int i){
    Char c;
    init_Char(&c);
    send_to_client(Clients[i].client_fd,transform_for_Send("list",c,i,"i",getDir()));
}
void hanlde_Open(Message msg){}

void printChar(Char c){
    printf("%d.",c.id.client_id);
    printf("%d\n",c.id.opCounter);
    printf("%d.",c.at.id_added_to.client_id );
    printf("%d ",c.at.id_added_to.opCounter );
    printf("@%d",c.at.pos_in_id );
    printf(" %d\n",c.at.lampertClock);
    printf("%c\n",c.value);
}

void printMsg(Message msg){
    printf("%s\n",msg.operation);
    printChar(msg.ch);
    printf("%d\n",msg.curosor_off);
    printf("%s\n",msg.file);
    printf("%s\n",msg.buffer);
}

void *handle_request(void *arg){
    int i = (int)((long)arg);
    char *buffer = malloc(RECV_SIZE);
    memset(buffer, 0, RECV_SIZE);  // Ensure the buffer is cleared
    void *ret = NULL;

    ssize_t recv_len = recv(Clients[i].client_fd, buffer, RECV_SIZE, 0);
    if (recv_len > 0){
        printf("Received request: %s\n", buffer);
        Message newMsg = parseChar(buffer);
        printMsg(newMsg);
        printf("trying to handle req\n");
        if(strcmp(newMsg.operation,"insert")==0){
            //handle_Insert(newMsg);
        }
        else if(strcmp(newMsg.operation,"delete")==0){

        }
        else if(strcmp(newMsg.operation,"list")==0){
            printf("handling list");
            handle_List(i);
        }
        else if(strcmp(newMsg.operation,"save")==0){

        }
        else if(strcmp(newMsg.operation,"share")==0){

        }
        else if(strcmp(newMsg.operation,"create")==0){

        }
        else if(strcmp(newMsg.operation,"open")==0){
            //hanlde_Open(newMsg);
        }
        else if(strcmp(newMsg.operation,"cursor")==0){

        }
        else if(strcmp(newMsg.operation,"end")==0){

        }
        else{
            printf("Received request: %s\n", newMsg.operation);
            printf("not a rec. operation");
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
    // Set up signal handlers
    signal(SIGINT, handle_signal);   // Ctrl+C
    signal(SIGTERM, handle_signal);  // Termination request
    signal(SIGTSTP, handle_signal);  // Ctrl+Z
    //init socket returns file descriptor for the socket of the server
    server_fd = init_socket(PORT);
    // waiting to accepting connections from clients
    accept_con(server_fd);
    close(server_fd);
    return 0;
}

//     gcc text_editor_server.c -o text_editor_server
//     ./text_editor_server

