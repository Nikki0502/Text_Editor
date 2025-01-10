CC = gcc
GTKFLAGS = `pkg-config --cflags --libs gtk+-3.0`
SERVER_TARGET = text_editor_server
CLIENT_TARGET = text_editor

all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(SERVER_TARGET): text_editor_server.c
	$(CC) -o $(SERVER_TARGET) text_editor_server.c

$(CLIENT_TARGET): text_editor.c
	$(CC) -o $(CLIENT_TARGET) text_editor.c $(GTKFLAGS)

.PHONY: clean run run-server run-client1 run-client2

clean:
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET)

run-server:
	./$(SERVER_TARGET) &

run-client1:
	./$(CLIENT_TARGET) &

run-client2:
	./$(CLIENT_TARGET) &

run: all
	./$(SERVER_TARGET) & \
	sleep 1 && \
	./$(CLIENT_TARGET) & \
	./$(CLIENT_TARGET) &