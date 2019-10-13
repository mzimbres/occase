pkg_name = occase
pkg_version = 1.0.0
pkg_revision = 1
tarball_name = $(pkg_name)-$(pkg_version)-$(pkg_revision)
tarball_dir = $(pkg_name)-$(pkg_version)
prefix = /usr
datarootdir = $(prefix)/share
datadir = $(datarootdir)
docdir = $(datadir)/doc/$(pkg_name)
bindir = $(prefix)/bin
srcdir = .
confdir = /etc/$(pkg_name)
systemddir = /lib/systemd/system

bin_final_dir = $(DESTDIR)$(bindir)/
doc_final_dir = $(DESTDIR)$(docdir)
conf_final_dir = $(DESTDIR)$(confdir)
service_final_dir = $(DESTDIR)$(systemddir)

db_name = $(pkg_name)-db
img_name = $(pkg_name)-img
toolname = $(pkg_name)-tool
monitorname = $(pkg_name)-monitor
loadtoolname = $(pkg_name)-load-tool

boost_inc_dir = /opt/boost_1_71_0/include
boost_lib_dir = /opt/boost_1_71_0/lib

ext_libs =
ext_libs += $(boost_lib_dir)/libboost_program_options.a

LDFLAGS = -lpthread
LDFLAGS += -lfmt
LDFLAGS += -lsodium
LDFLAGS += -lssl
LDFLAGS += -lcrypto

CPPFLAGS = -std=c++17
CPPFLAGS += -I. -I$./src -I$(boost_inc_dir)
CPPFLAGS += -Wall -Werror=format-security \
	    -Werror=implicit-function-declaration
CPPFLAGS += $(pkg-config --cflags libsodium)
CPPFLAGS += #-O2

VPATH = ./src

exes =
exes += publish_tests
exes += db
exes += menu_tool
exes += aedis
exes += simulation
exes += img
exes += img_key_gen

common_objs += menu.o
common_objs += system.o
common_objs += logger.o
common_objs += json_utils.o
common_objs += crypto.o

db_objs =
db_objs += redis.o
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

all: $(exes) load-tool

#.PHONY: release_hdr
#release_hdr:
#	$(srcdir)/mkreleasehdr.sh $(srcdir) > /dev/null 2>&1

Makefile.dep:
	-$(CXX) -MM ./src/*.cpp > $@

-include Makefile.dep

simulation: % : %.o $(client_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs)

publish_tests: % : %.o $(client_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs)

db: % : %.o $(db_objs) $(common_objs) $(aedis_objs) 
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs) -DBOOST_ASIO_CONCURRENCY_HINT_1=BOOST_ASIO_CONCURRENCY_HINT_UNSAFE

img: % : %.o $(imgserver_objs) $(common_objs) $(aedis_objs)
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
	install -D db $(bin_final_dir)$(db_name)
	install -D img $(bin_final_dir)$(img_name)
	install -D menu_tool $(bin_final_dir)$(toolname)
	install -D monitor.sh $(bin_final_dir)$(monitorname)
	install -D load-tool $(bin_final_dir)$(loadtoolname)
	install -D doc/management.txt $(doc_final_dir)/management.txt
	install -D doc/intro.txt $(doc_final_dir)/intro.txt
	install -D doc/posts.txt $(doc_final_dir)/posts.txt
	install -D config/$(db_name).conf $(conf_final_dir)/$(db_name).conf
	install -D config/$(img_name).conf $(conf_final_dir)/$(img_name).conf
	install -D config/$(db_name).service $(service_final_dir)/$(db_name).service
	install -D config/$(img_name).service $(service_final_dir)/$(img_name).service

uninstall:
	rm -f $(bin_final_dir)$(db_name)
	rm -f $(bin_final_dir)$(img_name)
	rm -f $(bin_final_dir)$(toolname)
	rm -f $(bin_final_dir)$(monitorname)
	rm -f $(bin_final_dir)$(loadtoolname)
	rm -f $(doc_final_dir)/management.txt
	rm -f $(doc_final_dir)/intro.txt
	rm -f $(doc_final_dir)/posts.txt
	rm -f $(conf_final_dir)/$(db_name).conf
	rm -f $(conf_final_dir)/$(img_name).conf
	rm -f $(service_final_dir)/$(db_name).service
	rm -f $(service_final_dir)/$(img_name).service
	rmdir $(DESDIR)$(docdir)

.PHONY: clean
clean:
	rm -f $(exes) $(exe_objs) $(lib_objs) $(pkg_name).tar.gz Makefile.dep load-tool

$(tarball_name).tar.gz:
	git archive --format=tar.gz --prefix=$(tarball_dir)/ HEAD > $(tarball_name).tar.gz

.PHONY: dist
dist: $(tarball_name).tar.gz

backup_emails = laetitiapozwolski@yahoo.fr mzimbres@gmail.com bobkahnn@gmail.com coolcatlookingforakitty@gmail.com

.PHONY: backup
backup: $(tarball_name).tar.gz
	echo "Backup" | mutt -s "Backup" -a $< -- $(backup_emails)

