#ifndef TEXT_EDITOR_H
#define TEXT_EDITOR_H

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h> 


#define RECV_SIZE (1 << 20)
#define SEND_SIZE (1 << 20) 
#define PORT 8080
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE (1 << 20)

// unique identity of operation
typedef struct{
    int client_id;
    int opCounter;
}Identifier;
// position and time point relative to id 
typedef struct{
    Identifier id_added_to;
    int pos_in_id;
    int lampertClock;
}Position;
// char with id and pos
typedef struct Char{
    Identifier id;
    Position at;
    char value;
}Char;
//  to make an empty Char lead to problems other wise 
void init_Char(Char* c) {
    c->value = 'a';  
    c->id.client_id = 0;
    c->id.opCounter = 0;
    c->at.id_added_to.client_id = 0;
    c->at.id_added_to.opCounter = 0;
    c->at.pos_in_id = 0;
    c->at.lampertClock = 0;
}
// a single character in the text
typedef struct Node{
    Char ch;
    struct Node* next;
    int tombstone;
}Node;
// entire text
typedef struct{
    Node *head;
}CRDT_Text;
// saved protocol msg OPERATION/CHAR/INT/FILE/BUFFER
typedef struct{
    char* operation;
    Char ch;
    int int_arg;
    char* file;
    char* buffer;
}Message;



#endif  
