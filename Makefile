boost_lib     = /opt/boost_1_67_0/lib
boost_libs    =
boost_libs    += $(boost_lib)/libboost_system.a
boost_libs    += $(boost_lib)/libboost_program_options.a
boost_include = /opt/boost_1_67_0/include
json_include  = /opt/nlohmann_3_1_2

DEBUG         = -g -ggdb3
LDFLAGS       = -lpthread
CPPFLAGS      = -I. -I$(boost_include) -I$(json_include) \
                -std=c++17 $(DEBUG) -Wall # -Werror

DIST_NAME   = sellit

exes = client server menu_dump

common_objs = json_utils.o
common_objs += menu_parser.o

server_objs =
server_objs += group.o
server_objs += server_mgr.o
server_objs += server_session.o
server_objs += listener.o
server_objs += idx_mgr.o

client_objs =
client_objs += client_mgr_login.o
client_objs += client_mgr_sms.o
client_objs += client_mgr_cg.o
client_objs += client_mgr_accept_timer.o
client_objs += client_mgr_sim.o

exe_objs = $(addsuffix .o, $(exes))

lib_objs = $(server_objs) $(client_objs) $(common_objs)

SRCS =
SRCS += $(lib_objs:.o=.cpp)
SRCS += $(lib_objs:.o=.hpp)
SRCS += $(addsuffix .cpp, $(exes))
SRCS += user.hpp
SRCS += config.hpp
SRCS += client_session.hpp

AUX = Makefile

all: $(exes)

Makefile.dep:
	-$(CXX) -MM *.cpp > $@

-include Makefile.dep

client: % : %.o $(client_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(boost_libs)

server: % : %.o $(server_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(boost_libs)

menu_dump: % : %.o $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS)

.PHONY: clean
clean:
	rm -f $(exes) $(exe_objs) $(lib_objs) $(DIST_NAME).tar.gz Makefile.dep

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

