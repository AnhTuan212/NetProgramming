CC       := gcc
CFLAGS   := -std=c11 -Wall -Wextra -pthread -g
LDFLAGS  := -pthread -lsqlite3 -lm

SERVER_SRCS := server.c user_manager.c question_bank.c logger.c db_init.c db_queries.c db_migration.c stats.c
CLIENT_SRCS := client.c

SERVER_OBJS := $(SERVER_SRCS:.c=.o)
CLIENT_OBJS := $(CLIENT_SRCS:.c=.o)

all: server client

server: $(SERVER_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

client: $(CLIENT_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o server client leaderboard_output.txt

cleanup-data:
	rm -f data/questions.txt data/rooms.txt data/results.txt data/users.txt
	@echo "✓ Text files removed (keeping logs.txt for audit trail)"

rebuild: clean all

.PHONY: all clean rebuild cleanup-data