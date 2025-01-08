#ifndef TEXT_EDITOR_H
#define TEXT_EDITOR_H

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
//#include <ncurses.h>
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
#define ENTER '\n'
#define CTRL_S 19
#define ESC 27
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE (1 << 20)

typedef struct{
    int client_id;
    int opCounter;
}Identifier;

typedef struct{
    Identifier id_added_to;
    int pos_in_id;
    int lampertClock;
}Position;

typedef struct Char{
    Identifier id;
    Position at;
    char value;
}Char;

void init_Char(Char* c) {
    c->value = 'a';  
    c->id.client_id = 0;
    c->id.opCounter = 0;
    c->at.id_added_to.client_id = 0;
    c->at.id_added_to.opCounter = 0;
    c->at.pos_in_id = 0;
    c->at.lampertClock = 0;
}

typedef struct Node{
    Char ch;
    struct Node* next;
    int tombstone;
}Node;

typedef struct{
    Node *head;
}CRDT_Text;

typedef struct{
    char* operation;
    Char ch;
    int curosor_off;
    char* file;
    char* buffer;
}Message;


const char* c= "BBBBBBBBBBBBBBBBBBBBBBBBBBB\nBMB---------------------BB\nBBB---------------------BBB\nBBB---------------------BBB\nBBB---------------------BBB\nBBB---------------------BBB\nBBB---------------------BBB\nBBBBBBBBBBBBBBBBBBBBBBBBBBB\nBBBBB++++++++++++++++BBBBBB\nBBBBB++BBBBB+++++++++BBBBBB\nBBBBB++BBBBB+++++++++BBBBBB\nBBBBB++BBBBB+++++++++BBBBBB\nBBBBB++++++++++++++++BBBBBB";


#endif  