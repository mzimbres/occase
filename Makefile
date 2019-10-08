pkg_name = occase
prefix = /usr/local
datarootdir = $(prefix)/share
datadir = $(datarootdir)
docdir = $(datadir)/doc/$(pkg_name)
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
binprefix =
srcdir = .
confdir = /etc/$(pkg_name)
systemddir = /etc/systemd/system

servername = $(pkg_name)-db
toolname = $(pkg_name)-tool
monitorname = $(pkg_name)-monitor
loadtoolname = $(pkg_name)-load-tool

boost_inc_dir = /opt/boost_1_70_0/include
boost_lib_dir = /opt/boost_1_70_0/lib

ext_libs =
ext_libs += $(boost_lib_dir)/libboost_program_options.a

LDFLAGS = -lpthread
LDFLAGS += -lfmt
LDFLAGS += -lsodium

CPPFLAGS = -std=c++17
CPPFLAGS += -I. -I$(srcdir)/src -I$(boost_inc_dir)
CPPFLAGS += -Wall -Werror=format-security \
	    -Werror=implicit-function-declaration
CPPFLAGS += $(pkg-config --cflags libsodium)
CPPFLAGS += -g #-O2

VPATH = $(srcdir)/src

exes =
exes += publish_tests
exes += db
exes += menu_tool
exes += aedis
exes += simulation
exes += imgserver
exes += img_key_gen

common_objs += menu.o
common_objs += system.o
common_objs += logger.o
common_objs += json_utils.o
common_objs += crypto.o

db_objs =
db_objs += worker.o
db_objs += worker_session.o
db_objs += redis.o
db_objs += stats_server.o
db_objs += release.o
db_objs += http_session.o
db_objs += utils.o

imgserver_objs =
imgserver_objs += img_session.o

client_objs =
client_objs += test_clients.o

aedis_objs =
aedis_objs += redis_session.o

menu_dump_objs =
menu_dump_objs += fipe.o

exe_objs = $(addsuffix .o, $(exes))

lib_objs =
lib_objs += $(db_objs)
lib_objs += $(imgserver_objs)
lib_objs += $(client_objs)
lib_objs += $(aedis_objs)
lib_objs += $(menu_dump_objs)
lib_objs += $(common_objs)

srcs =
srcs += $(lib_objs:.o=.cpp)
srcs += $(lib_objs:.o=.hpp)
srcs += $(addsuffix .cpp, $(exes))
srcs += net.hpp
srcs += test_clients.hpp
srcs += session_launcher.hpp
srcs += async_read_resp.hpp

aux = Makefile

all: release_hdr $(exes) load-tool

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

db: % : %.o $(db_objs) $(common_objs) $(aedis_objs) 
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs) -DBOOST_ASIO_CONCURRENCY_HINT_1=BOOST_ASIO_CONCURRENCY_HINT_UNSAFE

imgserver: % : %.o $(imgserver_objs) $(common_objs) $(aedis_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs) -DBOOST_ASIO_CONCURRENCY_HINT_1=BOOST_ASIO_CONCURRENCY_HINT_UNSAFE

menu_tool: % : %.o $(menu_dump_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(ext_libs) $(LDFLAGS)

aedis: % : %.o $(aedis_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs)

img_key_gen: % : %.o  $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS)

load-tool: load-tool.sh.in
	sed s/toolname/$(toolname)/ < $^ > $@
	chmod +x $@

install: all
	install -D db $(DESTDIR)$(bindir)/$(binprefix)$(servername)
	install -D menu_tool $(DESTDIR)$(bindir)/$(binprefix)$(toolname)
	install -D monitor.sh $(DESTDIR)$(bindir)/$(binprefix)$(monitorname)
	install -D load-tool $(DESTDIR)$(bindir)/$(binprefix)$(loadtoolname)
	install -D $(DESTDIR)$(srcdir)/doc/management.txt $(docdir)/management.txt
	install -D $(DESTDIR)$(srcdir)/doc/intro.txt $(docdir)/intro.txt
	install -D $(DESTDIR)$(srcdir)/doc/posts.txt $(docdir)/posts.txt
	install -D $(DESTDIR)$(srcdir)/$(servername).conf $(confdir)/$(servername).conf
	install -D $(DESTDIR)$(srcdir)/$(servername).service $(systemddir)/$(servername).service

uninstall:
	rm -f $(DESDIR)$(bindir)/$(binprefix)$(servername)
	rm -f $(DESDIR)$(bindir)/$(binprefix)$(toolname)
	rm -f $(DESDIR)$(bindir)/$(binprefix)$(monitorname)
	rm -f $(DESDIR)$(bindir)/$(binprefix)$(loadtoolname)
	rm -f $(DESDIR)$(docdir)/management.txt
	rm -f $(DESDIR)$(docdir)/intro.txt
	rm -f $(DESDIR)$(docdir)/posts.txt
	rm -f $(DESDIR)$(confdir)/$(servername).conf
	rm -f $(DESDIR)$(systemddir)/$(servername).service
	rmdir $(DESDIR)$(docdir)

.PHONY: clean
clean:
	rm -f $(exes) $(exe_objs) $(lib_objs) $(pkg_name).tar.gz Makefile.dep release.cpp release.hpp load-tool

$(pkg_name).tar.gz:
	git archive --format=tar.gz --prefix=$(pkg_name)/ HEAD > $(pkg_name).tar.gz

.PHONY: dist
dist: $(pkg_name).tar.gz

backup_emails = laetitiapozwolski@yahoo.fr mzimbres@gmail.com bobkahnn@gmail.com coolcatlookingforakitty@gmail.com

.PHONY: backup
backup: $(pkg_name).tar.gz
	echo "Backup" | mutt -s "Backup" -a $< -- $(backup_emails)

