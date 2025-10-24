CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall

SRC = server.cpp client.cpp user_manager.cpp question_bank.cpp logger.cpp stats.cpp
OBJS = $(SRC:.cpp=.o)

all: server client 

server: server.o user_manager.o question_bank.o logger.o stats.o
	$(CXX) $(CXXFLAGS) -o server server.o user_manager.o question_bank.o logger.o stats.o -pthread

client: client.o
	$(CXX) $(CXXFLAGS) -o client client.o -pthread

clean:
	rm -f *.o server client
