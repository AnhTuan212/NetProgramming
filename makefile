# --- Compiler ---
CC       := gcc
CFLAGS   := -std=c11 -Wall -Wextra -pthread -g
LDFLAGS  := -pthread

# --- Sources ---
SERVER_SRCS := server.c user_manager.c question_bank.c logger.c
CLIENT_SRCS := client.c
STATS_OBJ   := stats.o

SERVER_OBJS := $(SERVER_SRCS:.c=.o)
CLIENT_OBJS := $(CLIENT_SRCS:.c=.o)

# --- Targets ---
all: server client

server: $(SERVER_OBJS) $(STATS_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

data_dir:
	mkdir -p data

clean:
	rm -f *.o server client

rebuild: clean all

.PHONY: all clean rebuild data_dir