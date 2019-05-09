# Install
BIN = zahnrad

# Flags
CFLAGS += -std=c89 -pedantic -O2
CFLAGS += -I/opt/X11/include -L/opt/X11/lib

SRC = main.c
OBJ = $(SRC:.c=.o)

$(BIN):
	@mkdir -p bin
	rm -f bin/$(BIN) $(OBJS)
	$(CC) $(SRC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -o bin/$(BIN) -lX11 -lm
