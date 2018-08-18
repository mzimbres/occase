DEBUG_FLAGS = -g -ggdb3
CPP_VERSION = c++17
OTHER_FLAGS = -Wall -Werror
BOOST_LIBS = /opt/boost_1_67_0/lib/libboost_system.a
BOOST_INCLUDE = /opt/boost_1_67_0/include
JSON_INCLUDE = /opt/nlohmann_3_1_2

CPPFLAGS = -I$(BOOST_INCLUDE) -I$(JSON_INCLUDE) \
           -std=$(CPP_VERSION) $(DEBUG_FLAGS) $(OTHER_FLAGS)

CPP=g++

HEADERS = user.hpp group.hpp config.hpp server_data.hpp \
          server_session.hpp client_session.hpp grow_only_vector.hpp

LIBS=-lpthread

OBJS=user.o server_data.o server_session.o client_session.o group.o

all: client server

%.o: %.cpp $(HEADERS)
	$(CPP) -c -o $@ $< $(CPPFLAGS) $(LIBS)

client: client.cpp $(OBJS)
	$(CPP) -o $@ $< $(CPPFLAGS) $(LIBS) $(OBJS) $(BOOST_LIBS)

server: server.cpp $(OBJS)
	$(CPP) -o $@ $< $(CPPFLAGS) $(LIBS) $(OBJS) $(BOOST_LIBS)

.PHONY: clean

clean:
	rm -f server client $(OBJS)

