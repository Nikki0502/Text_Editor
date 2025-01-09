#include "text_editor.h"
#include <gtk/gtk.h>

int current_OP_Counter = 1;
int lampertClock = 0;
int client_id = 0;

int cursor_offset;

CRDT_Text* currentText;
char* currentFile;
GtkTextBuffer *text_buffer;

void freeCRDTText(CRDT_Text* text) {
    Node* tmp;
    while(text->head!=NULL){
        tmp = text->head;
        text->head = text->head->next;
        free(tmp);
    }
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
void saveFile(){
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
    pos.pos_in_id = 0;
    if(currentText->head==NULL){
        pos.id_added_to.client_id = 0;
        pos.id_added_to.opCounter = 0;
        return pos;
    }
    if(posText==0){
        pos.id_added_to.client_id = currentText->head->ch.id.client_id;
        pos.id_added_to.opCounter = currentText->head->ch.id.opCounter;
        return pos;
    }
    Node* current = currentText->head;
    int pos_with_tomb = 0;
    // go till posText(cursor) and change client_id.opCounter @posText
    for(int i=0 ;i < posText;i++){
        if(current == NULL){
            break;
        }
        if(current->tombstone==1){
            current=current->next;
            i--;
            pos_with_tomb++;
            continue;
        }
        pos.id_added_to.client_id=current->ch.id.client_id;
        pos.id_added_to.opCounter=current->ch.id.opCounter;
        current=current->next;
        pos_with_tomb++;
    }
    current = currentText->head;
    for(int i=0;i<pos_with_tomb;i++){
        if(current == NULL){
            break;
        }
        if(current->ch.id.client_id==pos.id_added_to.client_id&&current->ch.id.opCounter==pos.id_added_to.opCounter){
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
            g_print("Error: malloc failed for newNode");
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
        // check if already in rithg pos
        if(current_pos_in_id==c.at.pos_in_id){
            g_print("found pos");
            break;
        }
        if(current_node->ch.id.client_id==c.at.id_added_to.client_id && current_node->ch.id.opCounter==c.at.id_added_to.opCounter){
            
            current_pos_in_id++;
            
        }
        prev_node = current_node;
        current_node = current_node->next;
    }
    // coudnt insert bc pos@ was to large
    if(current_pos_in_id<c.at.pos_in_id){
        g_print("%d\n",current_pos_in_id);
        g_print("%d\n",c.at.pos_in_id);
        g_print("insert out of bounds");
        return;
    }

    // We now have the correct position, but we must also compare lampertClock if needed
    while ((current_node != NULL )&& (current_node->ch.at.lampertClock > c.at.lampertClock) && 
                (current_node->ch.at.id_added_to.client_id==c.at.id_added_to.client_id)&&(current_node->ch.at.id_added_to.opCounter==c.at.id_added_to.opCounter)) {
        prev_node = current_node;
        current_node = current_node->next;
    }

    // Create a new node
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        g_print("Error: malloc failed for newNode");
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
    
    // Increment clock
    if(c.at.lampertClock>lampertClock){
        lampertClock = c.at.lampertClock;
    }
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
    int currentPos = 0;
    while(current!=NULL){
        if(current->tombstone==0){
            if(currentPos==posText){
                if(current->ch.value=='\n'){
                    g_print("char got /n\n");
                }
                else{
                    g_print("char got %c\n",current->ch.value);
                }
                return current->ch;
            }
        currentPos++;
        }
        current = current->next;
    }
    perror("Char not found");
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
            return;
        }
        current = current->next;
    }
    g_print("didnt find char");
    
}

void printChar(Char c){
    g_print("%d.",c.id.client_id);
    g_print("%d\n",c.id.opCounter);
    g_print("%d.",c.at.id_added_to.client_id );
    g_print("%d ",c.at.id_added_to.opCounter );
    g_print("@%d",c.at.pos_in_id );
    g_print(" %d\n",c.at.lampertClock);
    g_print("%c\n",c.value);
}
/* ========================== Server Functionality =============================== */

/*
 updates the displayed buffer to currentText, needs to be called when things change
 gtk_buffer is the buffer displayed
*/
void update_text_view(GtkTextBuffer *gtk_buffer) {
    // Ensure we have valid input
    if (!gtk_buffer || !currentText) {
        g_warning("Invalid buffer or currentText");
        return;
    }

    // Get the text from CRDT
    char* text = crdtTextToChar(currentText);
    g_print("%s\n",text);
    if (!text) {
        g_warning("Failed to convert CRDT text");
        return;
    }

    // Validate UTF-8
    if (!g_utf8_validate(text, -1, NULL)) {
        g_warning("Invalid UTF-8 in text");
        free(text);
        return;
    }

    // Wrap the buffer modification in user action
    gtk_text_buffer_begin_user_action(gtk_buffer);
    
    // Clear existing content and set new text
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(gtk_buffer, &start, &end);
    gtk_text_buffer_delete(gtk_buffer, &start, &end);
    gtk_text_buffer_insert(gtk_buffer, &start, text, -1);
    
    gtk_text_buffer_end_user_action(gtk_buffer);

    free(text);
}

int socket_desc;
struct sockaddr_in server;
pthread_t listen_thread;
int stop_listening = 0;
int isConnected = 0;

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

void printMsg(Message msg){
    g_print("%s\n",msg.operation);
    printChar(msg.ch);
    g_print("%d\n",msg.int_arg);
    g_print("%s\n",msg.file);
    g_print("%s\n",msg.buffer);
}

int init_socket(){
    // Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        g_print("Failed to create socket");
        return 1;
    }
    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT); 

    return 0;
}

int connect_to_server(){
    // Connect to server
    g_print("Connecting to %s:%d\n", SERVER_IP, PORT);
    if (connect(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
        g_print("Failed to connect to server");
        return 1;
    }
    g_print("Connected\n");
    return 0;
}

int send_to_server(char* re){
    // Send request to server
    g_print("Sending request: \"%s\"\n", re);
    if (send(socket_desc, re, strlen(re), 0) < 0) {
        perror("Failed to send request");
        return 1;
    }
    return 0;
}

char* recv_from_server(){
    char* rec = malloc(BUFFER_SIZE);
    g_print("Waiting for reply...\n");
    // Receive and print response from server
    if (recv(socket_desc, rec, BUFFER_SIZE, 0) < 0) {
        perror("Failed to receive response");
        return NULL;
    }
        
    g_print("Received the following response:\n%s\n", rec);

    return rec;
}

void zeroOutMetadat(){
    char* current = crdtTextToChar(currentText);
    if(current==NULL){
        g_print("zeroing metdata fail");
        return;
    }
    currentText = charToCRDT_TEXT(current);
    free(current);
}

void handle_Insert(Message msg){
    g_print("handling insert");
    insert(currentText,msg.ch);
    update_text_view(text_buffer);
}
void handle_Delete(Message msg){
    g_print("handle_Delete");
    delete(currentText,msg.ch);
    update_text_view(text_buffer);
}
void handle_List(Message msg){
    g_print("handle_List");
    currentFile = NULL;
    client_id = msg.int_arg;
    currentText = charToCRDT_TEXT(msg.buffer);
    update_text_view(text_buffer);
}
void handle_Save(Message msg){
    g_print("handle_Save");

}
void handle_Share(Message msg){
    g_print("handle_Share");
    Char c;
    init_Char(&c);
    char* message = transform_for_Send("share",c,msg.int_arg,currentFile,crdtTextToChar(currentText));
    if(message==NULL){
        return;
    }
    send_to_server(message);
    free(message);
    zeroOutMetadat();
}
void handle_Create(Message msg){
    g_print("handle_Create");
    if(strcmp(msg.buffer,"FAIL")==0){
        g_print("not a valid name,%s\n",msg.file);
    }
    else{
        currentFile = msg.file;
        currentText = charToCRDT_TEXT(msg.buffer);
        update_text_view(text_buffer);
    }
}
void handle_Open(Message msg){
    g_print("handle_Open");
    currentText = charToCRDT_TEXT(msg.buffer);
    currentFile=msg.file;
    update_text_view(text_buffer);
}
void handle_Cursor(Message msg){
    g_print("handle_cursor");
}
void handle_Close(Message msg){
    g_print("handle_Close");
    // send per int if still file from server open
    if(msg.int_arg == 1){
        currentFile = NULL;
        freeCRDTText(currentText);
        update_text_view(text_buffer);
        gtk_text_buffer_set_text(text_buffer, c, -1);
    }
}
void handle_End(Message msg){
    g_print("handle_End");
    stop_listening = 1;
    isConnected = 0; 
    // send per int if still file from server open
    if(msg.int_arg == 1){
        currentFile = NULL;
        freeCRDTText(currentText);
        update_text_view(text_buffer);
        gtk_text_buffer_set_text(text_buffer, c, -1);
    }
}
void *listen_for_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    g_print("Listening for messages from the server...\n");

    while (!stop_listening){
        
        memset(buffer, 0, BUFFER_SIZE);
        int recv_size = recv(socket_desc, buffer, BUFFER_SIZE, 0);
        if (recv_size <= 0) {
            if (recv_size == 0) {
                g_print("Server disconnected\n");
                isConnected = 0;
            } else {
                g_printerr("Failed to receive message");
            }
            break;
        }

        g_print("Received message: %s\n", buffer);
        Message newMsg = parseChar(buffer);
        
        printMsg(newMsg);
        if(strcmp(newMsg.operation,"insert")==0){
            handle_Insert(newMsg);
        }
        else if(strcmp(newMsg.operation,"delete")==0){
            handle_Delete(newMsg);
        }
        else if(strcmp(newMsg.operation,"list")==0){
            handle_List(newMsg);
        }
        else if(strcmp(newMsg.operation,"save")==0){
            handle_Save(newMsg);
        }
        else if(strcmp(newMsg.operation,"share")==0){
            handle_Share(newMsg);
        }
        else if(strcmp(newMsg.operation,"create")==0){
            handle_Create(newMsg);
        }
        else if(strcmp(newMsg.operation,"open")==0){
            handle_Open(newMsg);
        }
        else if(strcmp(newMsg.operation,"cursor")==0){
            handle_Cursor(newMsg);
        }
        else if(strcmp(newMsg.operation,"close")==0){
            handle_Close(newMsg);
        }
        else if(strcmp(newMsg.operation,"end")==0){
            handle_End(newMsg);
        }
        else{
            g_print("not a rec. operation");
        }
        
    }

    close(socket_desc);
    pthread_exit(NULL);
    return NULL;
}



/*
opend a file trough dialog and passes filename to openFile, updateds textview at the end
*/
static void on_open_file(GtkWidget *widget, gpointer data) {
    g_print("Open File clicked\n");

    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Open File", 
        GTK_WINDOW(NULL), 
        GTK_FILE_CHOOSER_ACTION_OPEN, 
        "_Cancel", GTK_RESPONSE_CANCEL, 
        "_Open", GTK_RESPONSE_ACCEPT, 
        NULL
    );

    // shows dialog and gets the response
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));

    // choosen a file
    if (result == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if(isConnected==1){
            Char c;
            init_Char(&c);
            char* message = transform_for_Send("close",c,0,"i","i");
            
            if(message==NULL){
                g_print("close message null");
                return;
            }
            send_to_server(message);
            free(message);
        }
        openFile(filename);
        
        g_free(filename);
    }
    // canceled
    else{
        // destroy the dialog
        gtk_widget_destroy(dialog);
        return;
    }

    // destroy the dialog
    gtk_widget_destroy(dialog);
    
    // update the text_view
    update_text_view(GTK_TEXT_BUFFER(data));
    
}

static void on_create_file(GtkWidget *widget, gpointer data) {
    g_print("Create File clicked\n");

    // Create a dialog for entering the filename
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Create File",
        GTK_WINDOW(NULL),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Create", GTK_RESPONSE_ACCEPT,
        NULL
    );

    // Add a text entry widget to the dialog
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter filename...");
    gtk_box_pack_start(GTK_BOX(content_area), entry, TRUE, TRUE, 0);
    gtk_widget_show(entry);

    // Run the dialog and get the response
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));

    if (result == GTK_RESPONSE_ACCEPT) {
        // Retrieve the filename from the text entry
        const char *filename = gtk_entry_get_text(GTK_ENTRY(entry));

        if (filename != NULL && strlen(filename) > 0) {
            if(isConnected==1){
                Char c;
                init_Char(&c);
                char* message = transform_for_Send("close",c,0,"i","i");
                if(message==NULL){
                    g_print("close message null");
                    return;
                }
                send_to_server(message);
                free(message);
            }
            // Call the createFile function with the specified filename
            createFile(filename);
            
            // Update the text view (if needed)
            update_text_view(GTK_TEXT_BUFFER(data));
        } else {
            g_print("No filename provided.\n");
        }
    }

    // Destroy the dialog
    gtk_widget_destroy(dialog);

    
}

static void on_connect_server(GtkWidget *widget, gpointer data) {
    g_print("Connect server clicked\n");
    if(isConnected==1){
        g_print("isConnected == 1\n");
        return;
    }

    if (init_socket() != 0) {
        return;
    }

    if (connect_to_server() != 0) {
        return;
    }
    isConnected = 1;
    // Create a new thread to listen for incoming messages
    if (pthread_create(&listen_thread, NULL, listen_for_messages, NULL) != 0) {
        g_print("Failed to create listen thread\n");
        return;
    }

    g_print("Listen thread created successfully\n");
}

static void on_open_server(GtkWidget *widget, gpointer data) {
    g_print("on_open_server clicked\n");

    // check if connected

    if(isConnected == 0){
        return;
    }

    // Create a dialog for entering the filename
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Open File",
        GTK_WINDOW(NULL),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL
    );

    // Add a text entry widget to the dialog
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter filename...");
    gtk_box_pack_start(GTK_BOX(content_area), entry, TRUE, TRUE, 0);
    gtk_widget_show(entry);

    // Run the dialog and get the response
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));

    if (result == GTK_RESPONSE_ACCEPT) {
        // Retrieve the filename from the text entry
        const char *filename = gtk_entry_get_text(GTK_ENTRY(entry));

        if (filename != NULL && strlen(filename) > 0) {
            Char c;
            init_Char(&c);
            char* filename_copy = strdup(filename);
            if (filename_copy == NULL) {
                g_print("Failed to copy filename");
                return;
            }
            char* message = transform_for_Send("open",c,0,filename_copy,"i");
            if(message==NULL){
                g_print("open msg null");
                return;
            }
            send_to_server(message);
            free(message);
        } else {
            g_print("No filename provided.\n");
        }
    }

    // Destroy the dialog
    gtk_widget_destroy(dialog);
}

static void on_window_close() {
    // Stop the listening thread by setting the flag
    g_print("Window is closing, exiting program...\n");
    if(isConnected==1){
        Char c;
        init_Char(&c);
        char* message = transform_for_Send("end",c,0,"i","i");
        if(message==NULL){
            g_print("end on window close failed");
        }
        send_to_server(message);
        free(message);
    }

    // Wait for the listening thread to finish
    if (pthread_join(listen_thread, NULL) != 0) {
        g_print("Failed to join listen thread\n");
    }

    freeCRDTText(currentText);

    // Terminate the GTK main loop
    gtk_main_quit();  // This stops the GTK main loop
}

static void on_create_sever(GtkWidget *widget, gpointer data){
    g_print("on_open_server clicked\n");

    // check if connected

    if(isConnected == 0){
        return;
    }

    // Create a dialog for entering the filename
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Create File",
        GTK_WINDOW(NULL),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Create", GTK_RESPONSE_ACCEPT,
        NULL
    );

    // Add a text entry widget to the dialog
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter filename...");
    gtk_box_pack_start(GTK_BOX(content_area), entry, TRUE, TRUE, 0);
    gtk_widget_show(entry);

    // Run the dialog and get the response
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));

    if (result == GTK_RESPONSE_ACCEPT) {
        // Retrieve the filename from the text entry
        const char *filename = gtk_entry_get_text(GTK_ENTRY(entry));

        if (filename != NULL && strlen(filename) > 0) {
            Char c;
            init_Char(&c);
            char* filename_copy = strdup(filename);
            if (filename_copy == NULL) {
                g_print("Failed to copy filename");
                return;
            }
            char* message = transform_for_Send("create",c,0,filename_copy,"i");
            if(message==NULL){
                g_print("open msg null");
                return;
            }
            send_to_server(message);
            free(message);
        } else {
            g_print("No filename provided.\n");
        }
    }

    // Destroy the dialog
    gtk_widget_destroy(dialog);
}

static void on_get_sever(){
    Char c;
    init_Char(&c);
    char* message = transform_for_Send("list",c,0,"i","i");
    if(message==NULL){
        g_print("list message null");
        return;
    }
    send_to_server(message);
    free(message);
}

void get_cursor_position(GtkTextBuffer *buffer) {
    GtkTextIter iter;
    GtkTextMark *mark = gtk_text_buffer_get_insert(buffer); // Get the cursor (insert mark)
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);  // Get iter at the cursor

    gint cursor_off = gtk_text_iter_get_offset(&iter);   // Get the offset position
    cursor_offset = cursor_off;
    g_print("Cursor is at position: %d\n", cursor_offset);
}
void set_cursor_position(GtkTextBuffer *buffer){
    GtkTextIter iter;
    int text_length = gtk_text_buffer_get_char_count(buffer);

    // Clamp cursor_offset within valid bounds
    if (cursor_offset < 0) {
        cursor_offset = 0;
    } else if (cursor_offset > text_length) {
        cursor_offset = text_length;
    }
    gtk_text_buffer_get_iter_at_offset(buffer, &iter, cursor_offset);
    gtk_text_buffer_place_cursor(buffer, &iter);
}



gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data){
    if(currentFile == NULL){
        return FALSE;
    }
    get_cursor_position(data);
    g_print("Key pressed\n");
    // Check if the key is a printable letter
    if (event->state & GDK_CONTROL_MASK) {
        if (event->keyval == GDK_KEY_s && currentFile!=NULL) {
            g_print("Ctrl+S pressed\n");
            if(isConnected==1){
                Char c;
                init_Char(&c);
                char* message = transform_for_Send("save",c,0,currentFile,crdtTextToChar(currentText));
                if(message==NULL){
                    g_print("safe msg null");
                    return FALSE;
                }
                send_to_server(message);
                free(message);
            }
            else{
                saveFile(currentFile);
            }
            return TRUE; // Stop propagation
        }
        if (event->keyval == GDK_KEY_q) {
            g_print("Ctrl+Q pressed\n");
            on_window_close();
            return TRUE; // Stop propagation
        }
    }
    else if (event->keyval == GDK_KEY_BackSpace){
        // delte char before cursor
        if(cursor_offset==0){
            return FALSE;
        }
        Char c;
        c = getChar(cursor_offset-1);
        if(isConnected==1){
            char* message = transform_for_Send("delete",c,0,"i","i");
            if(message==NULL){
                g_print("delete msg null");
            }
            else{
                send_to_server(message);
                free(message);
            }
        }
        delete(currentText,c);
        current_OP_Counter++;
        // update text_view
        update_text_view(data);
        //set cursor 
        cursor_offset--;
        set_cursor_position(data);
        return true;
    }
    else if (event->keyval == GDK_KEY_Return){
        // get char
        char letter = (char)event->keyval;
        // testing
        g_print("Inserting letter: %c @ %d\n", letter, cursor_offset);
        // insert char at cursor 
        Char c = makeChar(letter,cursor_offset);
        if(isConnected==1){
            char* message = transform_for_Send("insert",c,0,"i","i");
            if(message==NULL){
                g_print("insert msg null");
            }
            else{
                send_to_server(message);
            }
        }
        insert(currentText,c);
        current_OP_Counter++;
        lampertClock++;
        // update text_view
        update_text_view(data);
        //set cursor +1
        cursor_offset++;
        set_cursor_position(data);
        return true;
    }
    else if (event->keyval == GDK_KEY_space){
        // get char
        char letter = (char)event->keyval;
        // testing
        g_print("Inserting letter: %c @ %d\n", letter, cursor_offset);
        // insert char at cursor 
        Char c = makeChar(letter,cursor_offset);
        if(isConnected==1){
            char* message = transform_for_Send("insert",c,0,"i","i");
            if(message==NULL){
                g_print("insert msg null");
            }
            else{
                send_to_server(message);
            }
        }
        insert(currentText,c);
        lampertClock++;
        current_OP_Counter++;
        // update text_view
        update_text_view(data);
        //set cursor +1
        cursor_offset++;
        set_cursor_position(data);
        return true;
    }
    else if ((event->keyval >= GDK_KEY_a && event->keyval <= GDK_KEY_z) || 
        (event->keyval >= GDK_KEY_A && event->keyval <= GDK_KEY_Z)) {
        // get char
        char letter = (char)event->keyval;
        // testing
        g_print("Inserting letter: %c @ %d\n", letter, cursor_offset);
        // insert char at cursor 
        Char c = makeChar(letter,cursor_offset);
        printChar(c);
        if(isConnected==1){
            char* message = transform_for_Send("insert",c,0,"i","i");
            if(message==NULL){
                g_print("insert msg null");
            }
            else{
                send_to_server(message);
            }
        }
        insert(currentText,c);
        current_OP_Counter++;
        lampertClock++;
        // update text_view
        update_text_view(data);
        //set cursor +1
        cursor_offset++;
        set_cursor_position(data);
        return true;
    }
    else{
        //g_print("not impl. %c",(char)event->keyval);
        printChar(getChar(cursor_offset));
        return FALSE;
    }

    //update_text_view(data);
    
    return FALSE;
}



int main(int argc, char *argv[]) {
    init_socket();
    gtk_init(&argc, &argv);
    // create widgets 
    GtkWidget *window,*menu_bar;
    GtkWidget *file_menu,*file_menu_item,*open_item,*create_item;
    GtkWidget *connect_menu,*connect_item,*connect_menu_item,*open_server_item,*create_server_item,*get_server_item;

    // the big overaraching window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "CRDT Text Editor");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    // create menu bar to choose action
    menu_bar = gtk_menu_bar_new();
    // FILE MENU
    file_menu = gtk_menu_new();
    file_menu_item = gtk_menu_item_new_with_label("File");
    open_item = gtk_menu_item_new_with_label("Open File");
    create_item = gtk_menu_item_new_with_label("Create File");
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), open_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), create_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_menu_item), file_menu);
    // CONNECNT MENU
    connect_menu = gtk_menu_new();
    connect_item = gtk_menu_item_new_with_label("Connect to Server");
    open_server_item = gtk_menu_item_new_with_label("Open File on Server");
    create_server_item = gtk_menu_item_new_with_label("Create File on Server");
    get_server_item = gtk_menu_item_new_with_label("List files on Server");
    gtk_menu_shell_append(GTK_MENU_SHELL(connect_menu), connect_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(connect_menu), get_server_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(connect_menu), open_server_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(connect_menu), create_server_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), file_menu_item);
    connect_menu_item = gtk_menu_item_new_with_label("Connect");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), connect_menu_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(connect_menu_item), connect_menu);
    gtk_widget_set_halign(menu_bar, GTK_ALIGN_START);
    gtk_widget_set_valign(menu_bar, GTK_ALIGN_CENTER);

   
    // Create a vbox to hold the menu bar and the text_view
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 0);
    //create text_view
    GtkWidget *scrolled_window;
    GtkWidget *text_view;
    
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    text_view = gtk_text_view_new();
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window,TRUE,TRUE,0);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);
    text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_buffer_set_text(text_buffer, c, -1);

    // clicking on menu these func exc.
    g_signal_connect(connect_item, "activate", G_CALLBACK(on_connect_server), NULL);
    g_signal_connect(get_server_item, "activate", G_CALLBACK(on_get_sever), NULL);
    g_signal_connect(create_server_item, "activate", G_CALLBACK(on_create_sever), text_buffer);
    g_signal_connect(open_server_item, "activate", G_CALLBACK(on_open_server), text_buffer);
    g_signal_connect(open_item, "activate", G_CALLBACK(on_open_file), text_buffer);
    g_signal_connect(create_item, "activate", G_CALLBACK(on_create_file), text_buffer);
    // when key pressed
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), text_buffer);
    // on exit stop program
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_close), NULL);

    // show all widgets
    gtk_widget_show_all(window);
    
    // start event loop
    gtk_main();

    return 0;
}

// gcc -o text_editor text_editor.c `pkg-config --cflags --libs gtk+-3.0`
