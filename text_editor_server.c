#include "text_editor.h"
#include <signal.h>

#define MAX_CLIENTS 2

struct client{
    int used;
    int client_fd;
    socklen_t client_len;
    struct sockaddr_in client_addr;
    pthread_t tid;
    char* currentFile;
};

static struct client Clients[MAX_CLIENTS];
const char* dirName = "./texts";
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

static int server_fd = -1;  

/*
 for debugging
*/
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
    printf("%d\n",msg.int_arg);
    printf("%s\n",msg.file);
    printf("%s\n",msg.buffer);
}
/*
 for catching signals such as crtlz and cleaning the socket when exiting
*/
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

/* ========================== Files =============================== */
/*
 returns a list of files in dirName
*/
char* getDir() {
    pthread_mutex_lock(&file_mutex);
    struct dirent *entry;
    DIR *dp = opendir(dirName);
    if (dp == NULL) {
        perror("opendir");
        pthread_mutex_unlock(&file_mutex);
        return NULL;
    }

    // initial buffer for the file names
    size_t buffer_size = 1024;
    char *file_list = malloc(buffer_size);
    if (!file_list) {
        perror("malloc");
        closedir(dp);
        pthread_mutex_unlock(&file_mutex);
        return NULL;
    }
    file_list[0] = '\0';  // null ter the string

    while ((entry = readdir(dp)) != NULL) {
        // skip . and .. 
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // calc required space: filename + newline + null termi
        size_t required_space = strlen(file_list) + strlen(entry->d_name) + 2;
        
        // check if the buffer needs more space
        if (required_space > buffer_size) {
            buffer_size = required_space * 2; 
            char *temp = realloc(file_list, buffer_size);
            if (!temp) {
                perror("realloc");
                free(file_list);  
                closedir(dp);
                pthread_mutex_unlock(&file_mutex);
                return NULL;
            }
            file_list = temp;  
        }

        // append the file name and newline to the buffer
        snprintf(file_list + strlen(file_list), buffer_size - strlen(file_list), "%s\n", entry->d_name);
    }

    closedir(dp);
    pthread_mutex_unlock(&file_mutex);
    return file_list;
}
/*
 return the contens of the file
*/
char* openFile(char* filename){
    // open file in read mode
    pthread_mutex_lock(&file_mutex);
    char* fullpath = malloc(sizeof(char)*strlen(filename)+20);
    if (fullpath == NULL) {
        perror("malloc");
        pthread_mutex_unlock(&file_mutex);
        return NULL;
    }
    snprintf(fullpath,sizeof(char)*strlen(filename)+20,"texts/%s",filename);

    FILE *file;
    file = fopen(fullpath,"r");
    if(file == NULL){
        perror("fail openeing File");
        pthread_mutex_unlock(&file_mutex);
        return NULL;
    }
    // go to end of file and find out size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    rewind(file);
    // malloc buffer large enough for file
    char* buffer = malloc(fileSize+1); // +1 for \0
    if (buffer == NULL) {
        perror("fail malloc in openFile");
        fclose(file);
        pthread_mutex_unlock(&file_mutex);
        return NULL;
    }
    // read the file into buffer
    size_t bytesRead = fread(buffer, 1, fileSize, file);
    if (bytesRead != fileSize) {
        perror("fail to read file");
        free(buffer);
        fclose(file);
        pthread_mutex_unlock(&file_mutex);
        return NULL;
    }
    buffer[bytesRead] = '\0';// null terminate

    // close file
    fclose(file);
    free(fullpath);
    pthread_mutex_unlock(&file_mutex);
    return buffer;
}
/*
 creates new file as long as not alrd ex and open it
*/
char* createFile(char* filename){
    pthread_mutex_lock(&file_mutex);
    // check if file alr. ex.
    char* fullpath = malloc(sizeof(char)*strlen(filename)+20);
    if (fullpath == NULL) {
        perror("malloc");
        pthread_mutex_unlock(&file_mutex);
        return NULL;
    }
    snprintf(fullpath,sizeof(char)*strlen(filename)+20,"texts/%s",filename);
    if(access(fullpath, F_OK) == 0){
        free(fullpath);
        pthread_mutex_unlock(&file_mutex);
        return NULL;
    }
    // creat file
    FILE *file;
    file =fopen(fullpath,"w");
    if(file == NULL){
        free(fullpath);
        perror("creating file fopen");
        pthread_mutex_unlock(&file_mutex);
        return NULL;
    }
    fclose(file);
    free(fullpath);
    // openfile
    pthread_mutex_unlock(&file_mutex);
    return openFile(filename);
}
/*
 saves buffer into file
*/
int saveFile(char* filename,char* buffer){
    pthread_mutex_lock(&file_mutex);
    // open file
    char* fullpath = malloc(sizeof(char)*strlen(filename)+20);
    if (fullpath == NULL) {
        perror("malloc");
        pthread_mutex_unlock(&file_mutex);
        return 1;
    }
    snprintf(fullpath,sizeof(char)*strlen(filename)+20,"texts/%s",filename);
    FILE *file;
    file = fopen(fullpath,"w");
    if(file == NULL){
        perror("error during openFile");
        pthread_mutex_unlock(&file_mutex);
        return 1;
    }

    if (buffer == NULL) {
        perror("error during crdtTextToChar");
        fclose(file);
        pthread_mutex_unlock(&file_mutex);
        return 1;
    }
    size_t bufferLength = strlen(buffer);  
    size_t bytesWritten = fwrite(buffer, sizeof(char), bufferLength, file);
    
    if (bytesWritten != bufferLength) {
        perror("Error during fwrite");
        return 1;
    }
   
    // close
    free(fullpath);
    free(buffer);
    fclose(file);
    pthread_mutex_unlock(&file_mutex);
    return 0;
}
/* ========================== Server =============================== */
/*
 initializes the socket with port and addr
*/
int init_socket(int port){
    int server_fd;
    struct sockaddr_in address;
    // creat a socket with AF_INET = IPv4, SOCK_STREAM = con. based, no specific protocol
    if((server_fd = socket(AF_INET,SOCK_STREAM,0))<0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    // to reuse addr
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
/* 
 return a struct Char to a string divied by .`s
*/
char* Char_to_char(Char ch) {
    int buffer_size = 100;
    char* result = malloc(buffer_size * sizeof(char));
    if (result == NULL) {
        return NULL;
    }
    // unsigned char to prevent sign extension issues
    unsigned char uvalue = (unsigned char)ch.value;
    snprintf(result, buffer_size, "%d.%d.%d.%d.%d.%d.%u",
        ch.id.client_id, 
        ch.id.opCounter,
        ch.at.id_added_to.client_id,
        ch.at.id_added_to.opCounter,
        ch.at.pos_in_id,
        ch.at.lampertClock, 
        uvalue);  
    return result;
}
/*
 parse a string recv into a Message for easier handeling
*/
Message parseChar(char* ch) {
    Message msg = {0};  
    char* ch_cpy = strdup(ch);
    if (!ch_cpy) {
        printf("strdup error parseChar");
        return msg;
    }

    // divide with strtok alonge / , . and //

    char* saveptr1 = NULL;
    char* saveptr2 = NULL;
    char* token = strtok_r(ch_cpy, "/", &saveptr1);
    
    if (token) {
        msg.operation = strdup(token);
        token = strtok_r(NULL, "/", &saveptr1); 
        if (token) {
            char* new_Char = token;
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
            if (num) msg.ch.value = (char)atoi(num); 
        }

        token = strtok_r(NULL, "/", &saveptr1);
        if (token) msg.int_arg = atoi(token);
        
        token = strtok_r(NULL, "//", &saveptr1);
        if (token) msg.file = strdup(token);
        
        token = strtok_r(NULL, "/", &saveptr1);
        if (token) msg.buffer = strdup(token);
        else{msg.buffer=malloc(sizeof(char));msg.buffer[0]='\0';}
    }

    free(ch_cpy);
    return msg;
}
/*
 attributs for different parts of the Protokoll returns sendalbe string
*/
char* transform_for_Send(char* op, Char ch, int off, char* file, char* buff) {
    // struct char to Char
    char* char_str = Char_to_char(ch);
    if (!char_str) {
        return NULL;
    }

    // calc buffer size 
    size_t op_len = strlen(op);
    size_t char_str_len = strlen(char_str);
    size_t file_len = strlen(file);
    size_t buff_len = strlen(buff);
    size_t total_size = op_len + 1 +char_str_len + 1 + 32 + file_len + 2 +buff_len + 1;          

    char* result = malloc(total_size);
    if (!result) {
        free(char_str);
        return NULL;
    }
    // snprintf to format the string
    int written = snprintf(result, total_size, "%s/%s/%d/%s//%s",
                          op, char_str, off, file, buff);

    free(char_str); 

    // Check if the formatting was successful
    if (written < 0 || written >= total_size) {
        free(result);
        return NULL;
    }

    return result;
}
/*
 send a string ot specified client
*/
int send_to_client(int client_sock,char* re){
    printf("Sending request: \"%s\"\n", re);
    if (send(client_sock, re, strlen(re), 0) < 0) {
        perror("Failed to send request");
        return 1;
    }
    return 0;
}
/*
 send to all clients exept spec client that have the same open File
*/
void broadcast(int from_client,char* msg){
    pthread_mutex_lock(&clients_mutex);
    if(Clients[from_client].currentFile==NULL){
        printf("broadcast cleint doest have open file");
        pthread_mutex_unlock(&clients_mutex);
        return;
    }
    pthread_mutex_unlock(&clients_mutex);
    for(int i = 0; i < MAX_CLIENTS;i++){
        
        if(Clients[i].currentFile==NULL){
            continue;
        }
        if(strcmp(Clients[i].currentFile,Clients[from_client].currentFile)==0 && i !=from_client){
            send_to_client(Clients[i].client_fd,msg);
        }
    }
    
}
/* ========================== Protocoll handeling funcs =============================== */
/*
 sends Insertion to all clients also opened that file
*/
void handle_Insert(Message msg,int i){
    char* message = transform_for_Send("insert", msg.ch, 0, "i", "i");
    if (message == NULL) {
        printf("insert msg null"); 
        return;
    }
    broadcast(i,message);
    free(message);
}
/*
 sends deletion to all clients also opened that file
*/
void handle_Delete(Message msg,int i){
    char* message = transform_for_Send("delete", msg.ch, 0, "i", "i");
    if (message == NULL) {
        printf("insert msg null"); 
        return;
    }
    broadcast(i,message);
    free(message);
}
/*
 sends a list of dir back to the client 
*/
void handle_List(int i){
    Char c;
    init_Char(&c);
    char * dir = getDir();
    if (dir == NULL) {
        return;
    }
    char* message = transform_for_Send("list", c, i, "i", dir);
    if (message == NULL) {
        free(dir);  
        return;
    }
    send_to_client(Clients[i].client_fd,message);
    free(message);  
    free(dir);      
}
/*
 opens file and sends it back to client
*/
void hanlde_Open(Message msg, int i){
    char* file = openFile(msg.file);
    if(file==NULL){
        printf("open not a file");
        return;
    }
    Char c;
    init_Char(&c);
    char* message;
    pthread_mutex_lock(&clients_mutex);
    Clients[i].currentFile = msg.file;
    
    // in case this file is already in use than get copy from other client with "share"
    for(int j = 0 ; j<MAX_CLIENTS; j++){
        if(Clients[j].currentFile==NULL||Clients[j].used == 0){
            continue;
        }
        if((strcmp(Clients[i].currentFile,Clients[j].currentFile))==0 && i != j){
            message = transform_for_Send("share", c, i, msg.file, "i");
            if (message == NULL) {
                free(file); 
                pthread_mutex_unlock(&clients_mutex); 
                return;
            }
            pthread_mutex_unlock(&clients_mutex);
            send_to_client(Clients[j].client_fd,message);
            
            free(message);
            free(file);
            return;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    message = transform_for_Send("open", c, 0, msg.file , file);
    if (message == NULL) {
        free(file);  
        return;
    }
    send_to_client(Clients[i].client_fd,message);
    free(message);  
}
/*
 gets the content of a alr opened file from client and sends it to the other client wanting to acess it
*/
void handle_Share(Message msg){
    if(msg.int_arg<0 || msg.int_arg >= MAX_CLIENTS){
        printf("share wrong client id");
        return;
    }
    if(msg.buffer==NULL){
        printf("share not a file");
        return;
    }
    Char c;
    init_Char(&c);
    char* message = transform_for_Send("open", c, 0, msg.file, msg.buffer);
    if (message == NULL) {
        printf("massage share not a file");
        return;
    }
    send_to_client(Clients[msg.int_arg].client_fd,message);
    free(message); 
}
/*
 saves returned content back into file
*/
void handle_Save(Message msg, int i){
    if(msg.buffer==NULL||msg.file==NULL){
        printf("save msg null");
        return;
    }
    pthread_mutex_lock(&clients_mutex);
    if(strcmp(msg.file,Clients[i].currentFile)!=0){
        printf("save not current file");
        pthread_mutex_unlock(&clients_mutex);
        return;
    }
    pthread_mutex_unlock(&clients_mutex);
    if(saveFile(msg.file,msg.buffer)==1){
        printf("error saving file");
        return;
    }

}
/*
 creates a new file with given name and send it to client
*/
void handle_Create(Message msg,int i){
    char* file = createFile(msg.file);
    Char c;
    init_Char(&c);
    char* message;

    if(file==NULL){
        printf("coudlnt create either fail or already");
        message = transform_for_Send("create", c, 0, msg.file , "FAIL");
    }
    else{
        pthread_mutex_lock(&clients_mutex);
        Clients[i].currentFile = msg.file;
        pthread_mutex_unlock(&clients_mutex);
        message = transform_for_Send("create", c, 0, msg.file , file);
    }
    if (message == NULL) {
        free(file);  
        return;
    }
    
    send_to_client(Clients[i].client_fd,message);
    
    free(file);
    free(message);
}
/*

*/
void handle_Cursor(Message msg, int i){}
/*
 deletes client 
*/
void handle_End(int i){
    Char c;
    init_Char(&c);
    char* message = transform_for_Send("end", c, 0, "i", "i");
    if (message == NULL) {
        printf("msg fail end");
        return;
    }
    
    send_to_client(Clients[i].client_fd,message);
    free(message);
    pthread_mutex_lock(&clients_mutex);
    Clients[i].used = 0;
    Clients[i].currentFile = NULL;
    pthread_mutex_unlock(&clients_mutex);
}
/*
 closes current open file of client
*/
void handle_Close(int i){
    Char c;
    init_Char(&c);
    int is_closed = 0;
    pthread_mutex_lock(&clients_mutex);
    if(Clients[i].currentFile!=NULL){
        is_closed=1;
    }
    Clients[i].currentFile = NULL;
    char* message = transform_for_Send("close", c, is_closed, "i", "i");
    if (message == NULL) {
        printf("msg fail close");
        pthread_mutex_unlock(&clients_mutex);
        return;
    }
    pthread_mutex_unlock(&clients_mutex);
    send_to_client(Clients[i].client_fd,message);
    free(message);
}

/*
 recv request and handels them
*/
void *handle_request(void *arg){
    int i = (int)((long)arg);
    char *buffer = malloc(RECV_SIZE);
    memset(buffer, 0, RECV_SIZE);  // Ensure the buffer is cleared
    void *ret = NULL;
    pthread_mutex_lock(&clients_mutex);
    int client_fd = Clients[i].client_fd;
    pthread_mutex_unlock(&clients_mutex);
    ssize_t recv_len = recv(client_fd, buffer, RECV_SIZE, 0);
    if (recv_len > 0){
        printf("Received request: %s\n", buffer);
        Message newMsg = parseChar(buffer);
        printMsg(newMsg);
        printf("trying to handle req\n");
        if(strcmp(newMsg.operation,"insert")==0){
            printf("handling insert");
            handle_Insert(newMsg,i);
        }
        else if(strcmp(newMsg.operation,"delete")==0){
            printf("handling delete");
            handle_Delete(newMsg,i);
        }
        else if(strcmp(newMsg.operation,"list")==0){
            printf("handling list");
            handle_List(i);
        }
        else if(strcmp(newMsg.operation,"save")==0){
            printf("handling save");
            handle_Save(newMsg,i);
        }
        else if(strcmp(newMsg.operation,"share")==0){
            printf("handling share");
            handle_Share(newMsg);
        }
        else if(strcmp(newMsg.operation,"create")==0){
            printf("handling create");
            handle_Create(newMsg,i);
        }
        else if(strcmp(newMsg.operation,"open")==0){
            printf("handling open");
            hanlde_Open(newMsg,i);
        }
        else if(strcmp(newMsg.operation,"close")==0){
            printf("handling close");
            handle_Close(i);
        }
        else if(strcmp(newMsg.operation,"cursor")==0){
            printf("handling cursor");
            handle_Cursor(newMsg,i);
        }
        else if(strcmp(newMsg.operation,"end")==0){
            printf("handling end");
            handle_End(i);
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
/*
 is run by thread for each client
*/
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
/*
 accepts incoming clients
*/
void accept_con(int server_fd){
    while(1){
        // check which client is unused
        pthread_mutex_lock(&clients_mutex);
        for(int i=0; i<MAX_CLIENTS;i++){
        // safe information of client
            if(Clients[i].used == 0){
                Clients[i].used =1;
                pthread_mutex_unlock(&clients_mutex);
                Clients[i].client_len = sizeof(Clients[i].client_addr);
                // accept client
                if((Clients[i].client_fd = accept(server_fd,(struct sockaddr *)&Clients[i].client_addr,&Clients[i].client_len))<0){
                    perror("failed to accept");
                    continue;
                }
                printf("accepted connenction from %s : %d \n",inet_ntoa(Clients[i].client_addr.sin_addr),ntohs(Clients[i].client_addr.sin_port));
                // start thread for client
                pthread_create(&Clients[i].tid, NULL, client_handler, (void*)((long)i));
                pthread_detach(Clients[i].tid);
            }
        }
        pthread_mutex_unlock(&clients_mutex);
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

