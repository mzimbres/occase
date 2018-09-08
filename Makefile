boost_libs    = /opt/boost_1_67_0/lib/libboost_system.a
boost_include = /opt/boost_1_67_0/include
json_include  = /opt/nlohmann_3_1_2

CPP           = g++
DEBUG         = -g -ggdb3
LDFLAGS       = -g -lpthread
CPPFLAGS      = -I. -I$(boost_include) -I$(json_include) \
                -std=c++17 $(DEBUG) -Wall -Werror

DIST_NAME    = sellit

common_objs = json_utils.o
server_objs = user.o group.o server_mgr.o server_session.o \
              listener.o server.o 
client_objs = client_mgr.o client_mgr_login.o client_mgr_sms.o \
              client_mgr_cg.o client.o client_mgr_accept_timer.o
objects     = $(server_objs) $(client_objs) $(common_objs)

SRCS        = $(objects:.o=.cpp)

headers     = user.hpp group.hpp config.hpp server_mgr.hpp \
              server_session.hpp client_session.hpp grow_only_vector.hpp \
              listener.hpp json_utils.hpp client_mgr.hpp \
              client_mgr_login.hpp client_mgr_sms.hpp \
              client_mgr_accept_timer.hpp client_mgr_cg.hpp

SRCS += $(headers)

AUX = Makefile

all: client server

%.o: %.cpp $(headers)
	$(CPP) -c -o $@ $< $(CPPFLAGS) $(LDFLAGS)

client:  $(client_objs) $(common_objs)
	$(CPP) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(boost_libs)

server: $(server_objs) $(common_objs)
	$(CPP) -o $@ $(server_objs) $(common_objs) $(CPPFLAGS) $(LDFLAGS) $(boost_libs)

.PHONY: clean
clean:
	rm -f server client $(objects) $(DIST_NAME).tar.gz

$(DIST_NAME).tar.gz: $(SRCS) $(AUX)
	rm -f $@
	mkdir $(DIST_NAME)
	ln $^ $(DIST_NAME)
	tar chzf $@ $(DIST_NAME)
	rm -rf $(DIST_NAME)

.PHONY: dist
dist: $(DIST_NAME).tar.gz

.PHONY: backup
backup: $(DIST_NAME).tar.gz
	echo "Backup" | mutt -s "Backup" -a $< -- mzimbres@gmail.com bobkahnn@gmail.com coolcatlookingforakitty@gmail.com

