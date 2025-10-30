# --- Compiler settings ---
CXX       := g++
CXXFLAGS  := -std=c++17 -Wall -Wextra -pthread
LDFLAGS   := -pthread

# --- Source files ---
SERVER_SRCS := server.cpp user_manager.cpp question_bank.cpp logger.cpp stats.cpp
CLIENT_SRCS := client.cpp

SERVER_OBJS := $(SERVER_SRCS:.cpp=.o)
CLIENT_OBJS := $(CLIENT_SRCS:.cpp=.o)

# --- Default target ---
all: server client

# --- Build server ---
server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(SERVER_OBJS) $(LDFLAGS)

# --- Build client ---
client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(CLIENT_OBJS) $(LDFLAGS)

# --- Generic rule for .cpp -> .o ---
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# --- Clean up ---
clean:
	rm -f *.o server client

# --- Convenience target to rebuild everything ---
rebuild: clean all
