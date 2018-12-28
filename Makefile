boost_inc_dir = /opt/boost_1_67_0/include
boost_lib_dir = /opt/boost_1_67_0/lib

boost_libs    =
boost_libs    += $(boost_lib_dir)/libboost_system.a
boost_libs    += $(boost_lib_dir)/libboost_program_options.a

DEBUG         = -g -ggdb3
LDFLAGS       = -lpthread
CPPFLAGS      = -I. -I$(boost_inc_dir) -std=c++17 $(DEBUG) -Wall # -Werror

DIST_NAME   = sellit

exes = publish_tests server menu_dump aedis read_only_tests reg_users_tests

common_objs = json_utils.o
common_objs += menu.o

server_objs =
server_objs += server_mgr.o
server_objs += server_session.o
server_objs += listener.o
server_objs += channel.o
server_objs += redis.o

client_objs =
client_objs += client_mgr_register.o
client_objs += client_mgr_confirm_code.o
client_objs += client_mgr_pub.o
client_objs += client_mgr_gmsg_check.o
client_objs += client_mgr_user_msg.o

aedis_objs =
aedis_objs += redis_session.o
aedis_objs += resp.o

menu_dump_objs =
menu_dump_objs += fipe.o

exe_objs = $(addsuffix .o, $(exes))

lib_objs = $(server_objs) $(client_objs) $(aedis_objs) $(menu_dump_objs) $(common_objs)

srcs =
srcs += $(lib_objs:.o=.cpp)
srcs += $(lib_objs:.o=.hpp)
srcs += $(addsuffix .cpp, $(exes))
srcs += config.hpp
srcs += client_session.hpp
srcs += session_launcher.hpp
srcs += async_read_resp.hpp
srcs += client_mgr_accept_timer.hpp
srcs += acceptor_arena.hpp

aux = Makefile

all: $(exes)

Makefile.dep:
	-$(CXX) -MM *.cpp > $@

-include Makefile.dep

publish_tests: % : %.o $(client_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(boost_libs)

read_only_tests: % : %.o $(client_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(boost_libs)

reg_users_tests: % : %.o $(client_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(boost_libs)

server: % : %.o $(server_objs) $(common_objs) $(aedis_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(boost_libs)

menu_dump: % : %.o $(menu_dump_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(boost_libs)

aedis: % : %.o $(aedis_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(boost_libs)

.PHONY: clean
clean:
	rm -f $(exes) $(exe_objs) $(lib_objs) $(DIST_NAME).tar.gz Makefile.dep

$(DIST_NAME).tar.gz: $(srcs) $(aux)
	git archive --format=tar.gz --prefix=sellit/ HEAD > sellit.tar.gz

.PHONY: dist
dist: $(DIST_NAME).tar.gz

backup_emails = laetitiapozwolski@yahoo.fr mzimbres@gmail.com bobkahnn@gmail.com coolcatlookingforakitty@gmail.com

.PHONY: backup
backup: $(DIST_NAME).tar.gz
	echo "Backup" | mutt -s "Backup" -a $< -- $(backup_emails)

