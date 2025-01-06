#include "text_editor.h"

int current_OP_Counter = 1;
int lampertClock = 0;
int client_id = 0;

int cursor_x =0;
int cursor_y =0;

CRDT_Text* currentText;

void freeCRDTText(CRDT_Text* text) {
}

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
    //free buffer and close file
    free(buffer);
    fclose(file);
}

int getWindowSize_X(){
    return 0;
}
int getWindowSize_Y(){
    return 0;
}

int getTotalPosInText(int x, int y){
    return (y*getWindowSize_Y())+x;
}


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

void printChar(Char c){
    printf("%d.",c.id.client_id);
    printf("%d\n",c.id.opCounter);
    printf("%d.",c.at.id_added_to.client_id );
    printf("%d ",c.at.id_added_to.opCounter );
    printf("@%d",c.at.pos_in_id );
    printf(" %d\n",c.at.lampertClock);
    printf("%d",c.value);
}

int main(){
    openFile("test");
    // Print the linked list
    Node* current = currentText->head;
    while (current != NULL) {
        printf("%c", current->ch.value);
        current = current->next;
    }
    printf("\n");
    Char c;
    c.at.id_added_to.client_id = 0;
    c.at.id_added_to.opCounter = 0;
    c.at.lampertClock = 0;
    c.at.pos_in_id = 0;
    c.id.client_id =0;
    c.id.opCounter =1;
    c.value ='C';
    insert(currentText,c);
    current = currentText->head;
    while (current != NULL) {
        printf("%c", current->ch.value);
        current = current->next;
    }
    printf("\n");
    Char b;
    /*
    b.at.id_added_to.client_id = 0;
    b.at.id_added_to.opCounter = 1;
    b.at.lampertClock = 1;
    b.at.pos_in_id = 0;
    */
    b.id.client_id =0;
    b.id.opCounter =2;
    b.value ='B';
    b.at = getPosition(0);
    delete(currentText,c);
    insert(currentText,b);
    current = currentText->head;
    while (current != NULL) {
        if(current->tombstone==0){
            printf("%c", current->ch.value);
        }
        current = current->next;
    }
    printf("\n");
    printChar(c);
    printf("\n");
    printChar(b);
    printf("\n");
}