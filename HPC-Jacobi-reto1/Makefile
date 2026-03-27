CC        = gcc
NOOPT     = -Wall -Wextra
BEST      = -O3 -Wall -march=native -funroll-loops -flto -ffast-math -fomit-frame-pointer
LDFLAGS   = -lm -lpthread
IFLAGS    = -Iinclude
BIN       = bin
LIB       = src/poisson.c

.PHONY: all clean

all: $(BIN)/serial_std $(BIN)/serial_opt $(BIN)/serial_cache $(BIN)/threads $(BIN)/processes

$(BIN):
	mkdir -p $(BIN)

$(BIN)/serial_std: src/serial_std.c $(LIB) | $(BIN)
	$(CC) $(NOOPT) $(IFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN)/serial_opt: src/serial_std.c $(LIB) | $(BIN)
	$(CC) $(BEST) $(IFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN)/serial_cache: src/serial_cache.c $(LIB) | $(BIN)
	$(CC) $(NOOPT) $(IFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN)/threads: src/threads.c $(LIB) | $(BIN)
	$(CC) $(NOOPT) $(IFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN)/processes: src/processes.c $(LIB) | $(BIN)
	$(CC) $(NOOPT) $(IFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BIN)