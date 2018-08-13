DEBUG_FLAGS=-g -ggdb3
CPP_VERSION=c++17
OTHER_FLAGS=-Wall -Werror
BOOST_LIBS=/opt/boost_1_67_0/lib/libboost_system.a
BOOST_INCLUDE=/opt/boost_1_67_0/include
JSON_INCLUDE=/opt/nlohmann_3_1_2

CPPFLAGS=-I$(BOOST_INCLUDE) -I$(JSON_INCLUDE) \
         -std=$(CPP_VERSION) $(DEBUG_FLAGS) $(OTHER_FLAGS)

CPP=g++

SERVER_FILES=user.hpp config.hpp server_data.hpp server_session.hpp
CLIENT_FILES=client_session.hpp

LIBS=-lpthread

SERVER_OBJS=user.o server_data.o server_session.o 
CLIENT_OBJS=client_session.o

all: client server

%.o: %.cpp $(SERVER_FILES) $(CLIENT_FILES)
	$(CPP) -c -o $@ $< $(CPPFLAGS) $(LIBS)

client: client.cpp $(CLIENT_OBJS)
	$(CPP) -o $@ $< $(CPPFLAGS) $(LIBS) $(CLIENT_OBJS) $(BOOST_LIBS)

server: server.cpp $(SERVER_OBJS)
	$(CPP) -o $@ $< $(CPPFLAGS) $(LIBS) $(SERVER_OBJS) $(BOOST_LIBS)

.PHONY: clean

clean:
	rm -f server client $(SERVER_OBJS) $(CLIENT_OBJS)

