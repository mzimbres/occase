CPP=g++
CPPDEBUG = -g -ggdb3
CPP_VERSION = c++17
BOOST_LIBS = /opt/boost_1_67_0/lib/libboost_system.a
BOOST_INCLUDE = /opt/boost_1_67_0/include
JSON_INCLUDE = /opt/nlohmann_3_1_2

LDFLAGS = -g -lpthread
CPPFLAGS = -I. -I$(BOOST_INCLUDE) -I$(JSON_INCLUDE) \
           -std=$(CPP_VERSION) $(CPPDEBUG) -Wall -Werror

common_objs = json_utils.o

server_objs = user.o group.o server_mgr.o server_session.o \
              listener.o server.o 

client_objs = client_session.o client_mgr.o client.o

objects = $(server_objs) $(client_objs) $(common_objs)

srcs = $(objects:.o=.c)

headers = user.hpp group.hpp config.hpp server_mgr.hpp \
          server_session.hpp client_session.hpp grow_only_vector.hpp \
          listener.hpp json_utils.hpp client_mgr.hpp

srcs += $(headers)

all: client server

%.o: %.cpp %.hpp
	$(CPP) -c -o $@ $< $(CPPFLAGS) $(LDFLAGS)

client:  $(client_objs) $(common_objs)
	$(CPP) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(BOOST_LIBS)

server: $(server_objs) $(common_objs)
	$(CPP) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(BOOST_LIBS)

.PHONY: clean

clean:
	rm -f server client $(objects)

