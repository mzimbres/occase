DEBUG_FLAGS=-g -ggdb3
CPP_VERSION=c++17
OTHER_FLAGS=-Wall -Werror
BOOST_LIB=/opt/boost_1_67_0/lib
BOOST_INCLUDE=/opt/boost_1_67_0/include
JSON_INCLUDE=/opt/nlohmann_3_1_2

CPPFLAGS=-I$(BOOST_INCLUDE) -I$(JSON_INCLUDE) -L$(BOOST_LIB) \
         -std=$(CPP_VERSION) $(DEBUG_FLAGS) $(OTHER_FLAGS)

CPP=g++

DEPS=user.hpp config.hpp

LIBS=-lpthread -lboost_system

OBJS=user.o

%.o: %.cpp $(DEPS)
	$(CPP) -c -o $@ $< $(CPPFLAGS) $(LIBS)

server: server.cpp $(OBJS)
	$(CPP) -o $@ $< $(CPPFLAGS) $(LIBS) user.o

client: client.cpp
	$(CPP) -o $@ $< $(CPPFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f server client $(OBJS)

