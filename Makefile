CXX ?= g++
CXXFLAGS ?= -Wall -Wextra -O2 -std=c++17

TARGETS = server client

all: $(TARGETS)

server: server.cpp buffer_mgmt.cpp buffer_mgmt.h
	$(CXX) $(CXXFLAGS) -o server server.cpp buffer_mgmt.cpp

client: client.cpp buffer_mgmt.cpp buffer_mgmt.h
	$(CXX) $(CXXFLAGS) -o client client.cpp buffer_mgmt.cpp

clean:
	rm -f $(TARGETS)

.PHONY: all clean
