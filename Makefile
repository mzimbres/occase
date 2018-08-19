CPP=g++
DEBUG_FLAGS = -g -ggdb3
CPP_VERSION = c++17
OTHER_FLAGS = -Wall -Werror
BOOST_LIBS = /opt/boost_1_67_0/lib/libboost_system.a
BOOST_INCLUDE = /opt/boost_1_67_0/include
JSON_INCLUDE = /opt/nlohmann_3_1_2

LDFLAGS=-lpthread
CPPFLAGS = -I$(BOOST_INCLUDE) -I$(JSON_INCLUDE) \
           -std=$(CPP_VERSION) $(DEBUG_FLAGS) $(OTHER_FLAGS)


HEADERS = user.hpp group.hpp config.hpp server_data.hpp \
          server_session.hpp client_session.hpp grow_only_vector.hpp \
          listener.hpp json_utils.hpp

COMMON_OBJS = json_utils.o

SERVER_OBJS = user.o group.o server_data.o server_session.o listener.o

CLIENT_OBJS = client_session.o

OBJS = $(SERVER_OBJS) $(CLIENT_OBJS) $(COMMON_OBJS)

all: client server

%.o: %.cpp %.hpp
	$(CPP) -c -o $@ $< $(CPPFLAGS) $(LDFLAGS)

client: client.cpp $(CLIENT_OBJS) $(COMMON_OBJS)
	$(CPP) -o $@ $< $(CPPFLAGS) $(LDFLAGS) $(CLIENT_OBJS) $(COMMON_OBJS) $(BOOST_LIBS)

server: server.cpp $(SERVER_OBJS) $(COMMON_OBJS)
	$(CPP) -o $@ $< $(CPPFLAGS) $(LDFLAGS) $(SERVER_OBJS) $(COMMON_OBJS) $(BOOST_LIBS)

.PHONY: clean

clean:
	rm -f server client $(OBJS)

