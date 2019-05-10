# Install
BIN = zahnrad

# Flags
CFLAGS += -std=c89 -pedantic -O2
CFLAGS += -I/Users/greg/net/microwindows/src/include -L/Users/greg/net/microwindows/src/lib

SRC = main.c
OBJ = $(SRC:.c=.o)

$(BIN):
	@mkdir -p bin
	rm -f bin/$(BIN) $(OBJS)
	$(CC) $(SRC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -o bin/$(BIN) -lnano-X -lm
