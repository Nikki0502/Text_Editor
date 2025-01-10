CC = gcc
GTKFLAGS = `pkg-config --cflags --libs gtk+-3.0`

SERVER_TARGET = text_editor_server
CLIENT_TARGET = text_editor

all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(SERVER_TARGET): text_editor_server.c
	$(CC) -o $(SERVER_TARGET) text_editor_server.c

$(CLIENT_TARGET): text_editor.c
	$(CC) -o $(CLIENT_TARGET) text_editor.c $(GTKFLAGS)

.PHONY: clean
clean:
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET)