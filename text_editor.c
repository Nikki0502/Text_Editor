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
    Char c;
    Node* current = currentText->head;
    int currentPos = 0;
    while(current!=NULL){
        if(currentPos==posText){
            return c = current->ch;
        }
        current = current->next;
        currentPos++;
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
            break;
        }
        current = current->next;
    }
    
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

/* ========================== Display Functionality =============================== */

/*
 updates the displayed buffer to currentText, needs to be called when things change
 gtk_buffer is the buffer displayed
*/
void update_text_view(GtkTextBuffer *gtk_buffer) {
    // replase buffer displayed with currentText(needs to be \0)
    gtk_text_buffer_set_text(gtk_buffer, crdtTextToChar(currentText), -1); 
    Node * curr = currentText->head;
    while (curr != NULL)
    {   
        printChar(curr->ch);
        curr = curr->next;
    }
    
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
    // Add your server connection logic here
    g_print("Connect to Server clicked\n");
}
int cursor_offset;
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
        if (event->keyval == GDK_KEY_s) {
            g_print("Ctrl+S pressed\n");
            saveFile(currentFile);
            return TRUE; // Stop propagation
        }
        if (event->keyval == GDK_KEY_q) {
            g_print("Ctrl+Q pressed\n");
            gtk_main_quit();
            return TRUE; // Stop propagation
        }
    }
    else if (event->keyval == GDK_KEY_BackSpace){
        // delte char before cursor
        if(cursor_offset==0){
            return FALSE;
        }
        delete(currentText,getChar(cursor_offset-1));
        // update text_view
        update_text_view(data);
        //set cursor +1
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
        insert(currentText,makeChar(letter,cursor_offset));
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
        insert(currentText,makeChar(letter,cursor_offset));
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
        insert(currentText,makeChar(letter,cursor_offset));
        // update text_view
        update_text_view(data);
        //set cursor +1
        cursor_offset++;
        set_cursor_position(data);
        return true;
    }
    else{
        g_print("not impl. %c",(char)event->keyval);
        return FALSE;
    }

    //update_text_view(data);
    
    return FALSE;
}




int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    // create widgets 
    GtkWidget *window,*menu_bar,*file_menu,*file_menu_item,*open_item,*create_item,*connect_menu,*connect_item,*connect_menu_item;

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
    gtk_menu_shell_append(GTK_MENU_SHELL(connect_menu), connect_item);
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
    GtkTextBuffer *text_buffer;
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    text_view = gtk_text_view_new();
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window,TRUE,TRUE,0);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);
    text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);

    // clicking on menu these func exc.
    g_signal_connect(connect_item, "activate", G_CALLBACK(on_connect_server), NULL);
    g_signal_connect(open_item, "activate", G_CALLBACK(on_open_file), text_buffer);
    g_signal_connect(create_item, "activate", G_CALLBACK(on_create_file), text_buffer);
    // when key pressed
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), text_buffer);
    // on exit stop program
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // show all widgets
    gtk_widget_show_all(window);
    
    // start event loop
    gtk_main();

    return 0;
}

// gcc -o text_editor text_editor.c `pkg-config --cflags --libs gtk+-3.0`
