boost_inc_dir = /opt/boost_1_67_0/include
boost_lib_dir = /opt/boost_1_67_0/lib

ext_libs =
ext_libs += $(boost_lib_dir)/libboost_system.a
ext_libs += $(boost_lib_dir)/libboost_program_options.a

DEBUG    = -O2 #-g -ggdb3
LDFLAGS  = -lpthread -lfmt
CPPFLAGS = -I. -I$(boost_inc_dir) -std=c++17 $(DEBUG) -Wall# -Werror

DIST_NAME = menu_chat_server

exes =
exes += publish_tests
exes += server
exes += menu_dump
exes += aedis
exes += simulation

common_objs += menu.o
common_objs += utils.o
common_objs += logger.o

server_objs =
server_objs += worker.o
server_objs += worker_session.o
server_objs += listener.o
server_objs += redis.o
server_objs += stats_server.o
server_objs += release.o

client_objs =
client_objs += test_clients.o

aedis_objs =
aedis_objs += redis_session.o

menu_dump_objs =
menu_dump_objs += fipe.o

exe_objs = $(addsuffix .o, $(exes))

lib_objs =
lib_objs += $(server_objs)
lib_objs += $(client_objs)
lib_objs += $(aedis_objs)
lib_objs += $(menu_dump_objs)
lib_objs += $(common_objs)

srcs =
srcs += $(lib_objs:.o=.cpp)
srcs += $(lib_objs:.o=.hpp)
srcs += $(addsuffix .cpp, $(exes))
srcs += config.hpp
srcs += test_clients.hpp
srcs += session_launcher.hpp
srcs += async_read_resp.hpp

aux = Makefile

release_hdr := $(shell sh -c './mkreleasehdr.sh')

all: $(exes)

Makefile.dep:
	-$(CXX) -MM *.cpp > $@

-include Makefile.dep

simulation: % : %.o $(client_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs)

publish_tests: % : %.o $(client_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs)

server: % : %.o $(server_objs) $(common_objs) $(aedis_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs) -DBOOST_ASIO_CONCURRENCY_HINT_1=BOOST_ASIO_CONCURRENCY_HINT_UNSAFE_IO

menu_dump: % : %.o $(menu_dump_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) -lfmt $(ext_libs)

aedis: % : %.o $(aedis_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs)

.PHONY: clean
clean:
	rm -f $(exes) $(exe_objs) $(lib_objs) $(DIST_NAME).tar.gz Makefile.dep release.cpp release.hpp

$(DIST_NAME).tar.gz: $(srcs) $(aux)
	git archive --format=tar.gz --prefix=sellit/ HEAD > menu_chat_server.tar.gz

.PHONY: dist
dist: $(DIST_NAME).tar.gz

backup_emails = laetitiapozwolski@yahoo.fr mzimbres@gmail.com bobkahnn@gmail.com coolcatlookingforakitty@gmail.com

.PHONY: backup
backup: $(DIST_NAME).tar.gz
	echo "Backup" | mutt -s "Backup" -a $< -- $(backup_emails)

