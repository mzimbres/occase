pkg_name = menu-chat
prefix = /usr/local
datarootdir = $(prefix)/share
datadir = $(datarootdir)
docdir = $(datadir)/doc/$(pkg_name)
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
binprefix =
srcdir = .
confdir = /etc/menu-chat
systemddir = /etc/systemd/system

server_name = menu-chat-server
tool_name = menu-chat-tool

boost_inc_dir = /opt/boost_1_70_0/include
boost_lib_dir = /opt/boost_1_70_0/lib

ext_libs =
ext_libs += $(boost_lib_dir)/libboost_program_options.a

LDFLAGS  = -lpthread -lfmt
CPPFLAGS = -I. -I$(srcdir)/src -I$(boost_inc_dir) \
	   -std=c++17 -Wall -O2 \
           -Werror=format-security \
	   -Werror=implicit-function-declaration

VPATH = $(srcdir)/src

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

all: release_hdr $(exes)

.PHONY: release_hdr
release_hdr:
	$(srcdir)/mkreleasehdr.sh $(srcdir) > /dev/null 2>&1

Makefile.dep:
	-$(CXX) -MM $(srcdir)/src/*.cpp > $@

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

install: all
	install server $(bindir)/$(binprefix)$(server_name)
	install menu_dump $(bindir)/$(binprefix)$(tool_name)
	install -D $(srcdir)/doc/development.txt $(docdir)/development.txt
	install -D $(srcdir)/doc/intro.txt $(docdir)/intro.txt
	install -D $(srcdir)/doc/posts.txt $(docdir)/posts.txt
	install -D $(srcdir)/menu-chat-server.conf $(confdir)/$(server_name).conf
	install -D $(srcdir)/menu-chat-server.service $(systemddir)/$(server_name).service

uninstall: all
	rm -f $(bindir)/$(binprefix)menu-chat-server
	rm -f $(bindir)/$(binprefix)menu-chat-tool
	rm -f $(docdir)/development.txt
	rm -f $(docdir)/intro.txt
	rm -f $(docdir)/posts.txt
	rm -f $(confdir)/$(server_name).conf
	rm -f $(systemddir)/$(server_name).service
	rmdir $(docdir)

.PHONY: clean
clean:
	rm -f $(exes) $(exe_objs) $(lib_objs) $(pkg_name).tar.gz Makefile.dep release.cpp release.hpp

$(pkg_name).tar.gz: $(srcs) $(aux)
	git archive --format=tar.gz --prefix=$(pkg_name)/ HEAD > $(pkg_name).tar.gz

.PHONY: dist
dist: $(pkg_name).tar.gz

backup_emails = laetitiapozwolski@yahoo.fr mzimbres@gmail.com bobkahnn@gmail.com coolcatlookingforakitty@gmail.com

.PHONY: backup
backup: $(pkg_name).tar.gz
	echo "Backup" | mutt -s "Backup" -a $< -- $(backup_emails)

