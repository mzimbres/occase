DEBUG_FLAGS=-g -ggdb3
CPP_VERSION=c++17
OTHER_FLAGS=-Wall -Werror
BOOST_LIB=/opt/boost_1_67_0/lib
BOOST_INCLUDE=/opt/boost_1_67_0/include
JSON_INCLUDE=/opt/nlohmann_3_1_2

CPPFLAGS=-I$(BOOST_INCLUDE) -I$(JSON_INCLUDE) -L$(BOOST_LIB) \
         -std=$(CPP_VERSION) $(DEBUG_FLAGS) $(OTHER_FLAGS)

CPP=g++

LIBS=-lpthread -lboost_system

server: server.cpp
	$(CPP) -o $@ $< $(CPPFLAGS) $(LIBS)

client: client.cpp
	$(CPP) -o $@ $< $(CPPFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f server client

