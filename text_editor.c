#include "text_editor.h"

int current_OP_Counter = 1;
int lampertClock = 0;
int client_id = 0;

int cursor_x =0;
int cursor_y =0;

CRDT_Text* currentText;
char* currentFile;

void freeCRDTText(CRDT_Text* text) {
}
/*
 returns a converted char* from CRDT_TEXT
*/
char* crdtTextToChar(CRDT_Text* text){
    char* buffer = NULL;
    int length = 0;
    // get length of list
    Node* current = text->head;
    while (current != NULL) {
        if (!current->tombstone) { 
            length++;
        }
        current = current->next;
    }
    buffer = (char*)malloc((length + 1) * sizeof(char));
    if (buffer == NULL) {
        perror("Memory allocation failed for buffer");
        return NULL;
    }

    current = text->head;
    int i = 0;
    while (current != NULL) {
        if (!current->tombstone) { 
            buffer[i] = current->ch.value;
            i++;
        }
        current = current->next;
    }

    // null term the buffer
    buffer[i] = '\0';

    return buffer;
}
/*
 tranforms linked list into a a char* buffer and write into file
*/
void safeFile(){
    // open file
    FILE *file;
    file = fopen(currentFile,"w");
    if(file == NULL){
        perror("error during openFile");
        return;
    }

    char* buffer = crdtTextToChar(currentText);
    if (buffer == NULL) {
        perror("error during crdtTextToChar");
        fclose(file);
        return;
    }
    size_t bufferLength = strlen(buffer);  
    size_t bytesWritten = fwrite(buffer, sizeof(char), bufferLength, file);
    
    if (bytesWritten != bufferLength) {
        perror("Error during fwrite");
    }
   
    // close
    free(buffer);
    fclose(file);
}
/*
 for converting a char* to a CRDT_TEXT tpye linked lsit
*/
CRDT_Text* charToCRDT_TEXT(char* buffer){
    // init a CRDT Text
    CRDT_Text* text = (CRDT_Text*)malloc(sizeof(CRDT_Text));
    if(text==NULL){
        perror("fail malloc charToCRDT_TExt");
        return NULL;
    }
    // head auf NULL
    text->head = NULL;
    Node* tail = NULL;
    int pos_in_id = 0;

    // go trough buffer and give each char in buffer a node
    for (char* p = buffer; *p != '\0'; ++p) {
        Node* newNode = (Node*)malloc(sizeof(Node));
        if (newNode == NULL) {
            perror("fail malloc failed for Node in charToCRDT_TEXT");
            return NULL;
        }
        newNode->ch.value = *p; 

        newNode->ch.id.client_id = 0; // 0.0
        newNode->ch.id.opCounter = 0;

        newNode->ch.at.id_added_to.client_id = 0; // 0.0 @pos 0 
        newNode->ch.at.id_added_to.opCounter = 0;
        newNode->ch.at.pos_in_id = pos_in_id++; 
        newNode->ch.at.lampertClock = 0;

        newNode->tombstone = 0;

        newNode->next = NULL;

        if (text->head == NULL) {
            text->head = newNode;
        } else {
            tail->next = newNode;
        }
        tail = newNode; 
    }
    return text;
}
/*
 opens a given File with filename and converts it to linked list CRDT_Text, sets this as current_text
*/
void openFile(const char* filename){
    // open file in read mode
    FILE *file;
    file = fopen(filename,"r");
    if(file == NULL){
        perror("fail openeing File");
        return;
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
        return;
    }
    // read the file into buffer
    size_t bytesRead = fread(buffer, 1, fileSize, file);
    if (bytesRead != fileSize) {
        perror("fail to read file");
        free(buffer);
        fclose(file);
        return;
    }
    buffer[bytesRead] = '\0';// null terminate

    if(currentText!= NULL){
        // free current Text first
        freeCRDTText(currentText);
    }
    // convert char to CRDT_TEXT
    currentText = charToCRDT_TEXT(buffer);
    if(currentText == NULL){
        perror("fail charToCRDT in openFIle");
        return;
    }
    // save new file name in Editor
    currentFile = malloc(strlen(filename)+1);
    strcpy(currentFile,filename);
    //free buffer and close file
    free(buffer);
    fclose(file);
}
/*
 checks if file exists if not creates one else opens it
*/
void createFile(const char* filename){
    // check if file alr. ex.
    if(access(filename, F_OK) == 0){
        // go back to menu
        openFile(filename);
        return;
    }
    // creat file
    FILE *file;
    file =fopen(filename,"w");
    if(file == NULL){
        perror("creating file fopen");
        return;
    }
    fclose(file);
    // openfile
    openFile(filename);
}
/*
 gets the Postion to insert a char in posText FORM: should be inserted 0.1 @3 Clock
*/
Position getPosition(int posText){
    // create pos
    Position pos;
    // current lampertClock for new added
    pos.lampertClock=lampertClock;
    // incase empty
    pos.id_added_to.client_id=0;
    pos.id_added_to.opCounter=0;
    pos.pos_in_id=0;
    Node* current = currentText->head;
    // go till posText(cursor) and change client_id.opCounter @posText
    for(int i=0 ;i <= posText;i++){
        if(current == NULL){
            break;
        }
        pos.id_added_to.client_id=current->ch.id.client_id;
        pos.id_added_to.opCounter=current->ch.id.opCounter;
        current=current->next;
    }
    current = currentText->head;
    for(int i=0 ;i < posText;i++){
        if(current == NULL){
            break;
        }
        if(pos.id_added_to.client_id==current->ch.id.client_id&&pos.id_added_to.opCounter==current->ch.id.opCounter){
            pos.pos_in_id++;
        }
        current=current->next;
    }
    return pos;
}
/*
 makes out of keyboard input and cursor pos a Char 
*/
Char makeChar(char c, int posText){
    Char new_Char;
    new_Char.at = getPosition(posText);
    new_Char.id.client_id = client_id;
    new_Char.id.opCounter = current_OP_Counter;
    new_Char.value = c;
    return new_Char;
}
/*
 inserts a Char C, relative to Position in C.at, in text
*/
void insert(CRDT_Text* text, Char c){
    // Empty list check
    if (text->head == NULL) {
        // Create a new node and set it as the head of the list
        Node* newNode = (Node*)malloc(sizeof(Node));
        if (newNode == NULL) {
            perror("Error: malloc failed for newNode");
            return;
        }

        newNode->ch = c;
        newNode->next = NULL;
        newNode->tombstone = 0;
        text->head = newNode;
        return; // New node inserted at the head, exit the function
    }

    // Handle non-empty list
    int current_pos_in_id = 0;
    Node* current_node = text->head;
    Node* prev_node = NULL;

    // Find the correct position based on `pos_in_id`
    while (current_node != NULL){
        // check if same id
        if(current_node->ch.id.client_id==c.at.id_added_to.client_id && current_node->ch.id.opCounter==c.at.id_added_to.opCounter){
            // check if already in rithg pos
            if(current_pos_in_id==c.at.pos_in_id){
                break;
            }
            else{
                current_pos_in_id++;
            }
        }
        prev_node = current_node;
        current_node = current_node->next;
    }
    // coudnt insert bc pos@ was to large
    if(current_pos_in_id<c.at.pos_in_id){
        printf("%d\n",current_pos_in_id);
        printf("%d\n",c.at.pos_in_id);
        //perror("insert out of bounds");
        return;
    }

    // We now have the correct position, but we must also compare lampertClock if needed
    while (current_node != NULL && current_node->ch.at.lampertClock > c.at.lampertClock) {
        prev_node = current_node;
        current_node = current_node->next;
    }

    // Create a new node
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        perror("Error: malloc failed for newNode");
        return;
    }

    // Set the values for the new node
    newNode->ch = c;
    newNode->next = current_node;
    newNode->tombstone = 0;

    if (prev_node == NULL) {
        text->head = newNode;
    } else {
        prev_node->next = newNode;
    }
    
    // Increment the operational counters
    current_OP_Counter++;
    lampertClock++;
}
/*
 checks if to Chars are the same, if unique id are all equal
*/
bool isSameChar(Char c, Char b){
    if(c.id.client_id==b.id.client_id&&c.id.opCounter==b.id.opCounter){
        if(c.at.id_added_to.client_id==b.at.id_added_to.client_id&&c.at.id_added_to.opCounter==b.at.id_added_to.opCounter){
            if(c.at.pos_in_id==b.at.pos_in_id){
                if(c.at.lampertClock==b.at.lampertClock){
                    return true;
                }
            }

        }
    }
    return false;
}
/*
 gets the Char positioned as posText
*/
Char getChar(int posText){
    Node* current = currentText->head;
    int currentPos= 0;
    while(current!=NULL){
        if(currentPos==posText){
            return current->ch;
        }
        current = current->next;
    }
}
/*
 delets the Char c by finding it with its unique ids
*/
void delete(CRDT_Text* text, Char c){
    // empty text
    if(text->head==NULL){
        return;
    }
    //
    Node* current = text->head;
    while(current!=NULL){
        if(isSameChar(current->ch,c)){
            current->tombstone = 1;
            break;
        }
        current = current->next;
    }
    
}

/*
test func
*/
void printChar(Char c){
    printf("%d.",c.id.client_id);
    printf("%d\n",c.id.opCounter);
    printf("%d.",c.at.id_added_to.client_id );
    printf("%d ",c.at.id_added_to.opCounter );
    printf("@%d",c.at.pos_in_id );
    printf(" %d\n",c.at.lampertClock);
    printf("%d",c.value);
}

void display(){
    
}

int main(){

}


// gcc -o text_editor text_editor.c `pkg-config --cflags --libs gtk+-3.0`
