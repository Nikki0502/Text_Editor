#ifndef TEXT_EDITOR_H
#define TEXT_EDITOR_H

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <ncurses.h>
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

typedef struct{
    int client_id;
    int opCounter;
}Identifier;

typedef struct{
    Identifier id_added_to;
    int pos_in_id;
    int lampertClock;
}Position;

typedef struct{
    Identifier id;
    Position at;
    char value;
}Char;

typedef struct Node{
    Char ch;
    struct Node* next;
    int tombstone;
}Node;

typedef struct{
    Node *head;
}CRDT_Text;


#endif