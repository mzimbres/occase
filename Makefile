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
mms_name = $(pkg_name)-mms
menu_name = $(pkg_name)-menu
key_gen_name = $(pkg_name)-key-gen
db_monitor_name = $(pkg_name)-db-monitor
menu_gen_name = $(pkg_name)-menu-gen

boost_inc_dir = /opt/boost_1_71_0/include
boost_lib_dir = /opt/boost_1_71_0/lib

ext_libs =
ext_libs += $(boost_lib_dir)/libboost_program_options.a

LDFLAGS = -lpthread
LDFLAGS += -lfmt
LDFLAGS += -lsodium
LDFLAGS += -lssl
LDFLAGS += -lcrypto

CPPFLAGS += -std=c++17
CPPFLAGS += -I. -I$./src -I$(boost_inc_dir)
CPPFLAGS += $(pkg-config --cflags libsodium)
CPPFLAGS += $(CXXFLAGS)
CPPFLAGS += #-O2

VPATH = ./src

exes =
exes += $(db_name)
exes += $(menu_name)
exes += $(mms_name)
exes += $(key_gen_name)
exes += db_tests
exes += aedis
exes += simulation

common_objs += menu.o
common_objs += system.o
common_objs += logger.o
common_objs += post.o
common_objs += crypto.o

db_objs =
db_objs += redis.o
db_objs += utils.o
db_objs += net.o

mms_objs =
mms_objs += mms_session.o
mms_objs += net.o

client_objs =
client_objs += test_clients.o

aedis_objs =
aedis_objs += redis_session.o

menu_dump_objs =
menu_dump_objs += csv.o

exe_objs = $(addsuffix .o, $(exes))

lib_objs =
lib_objs += $(db_objs)
lib_objs += $(mms_objs)
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

all: $(exes)

#.PHONY: release_hdr
#release_hdr:
#	$(srcdir)/mkreleasehdr.sh $(srcdir) > /dev/null 2>&1

Makefile.dep:
	-$(CXX) -MM ./src/*.cpp > $@

-include Makefile.dep

simulation: % : %.o $(client_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs)

db_tests: % : %.o $(client_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs)

$(db_name): % : %.o $(db_objs) $(common_objs) $(aedis_objs) 
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs) -DBOOST_ASIO_CONCURRENCY_HINT_1=BOOST_ASIO_CONCURRENCY_HINT_UNSAFE

$(mms_name): % : %.o $(mms_objs) $(common_objs) $(aedis_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs) -DBOOST_ASIO_CONCURRENCY_HINT_1=BOOST_ASIO_CONCURRENCY_HINT_UNSAFE

$(menu_name): % : %.o $(menu_dump_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(ext_libs) -lfmt -lsodium

aedis: % : %.o $(aedis_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs)

$(key_gen_name): % : %.o  $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) -lfmt -lsodium

#load-tool: load-tool.sh.in
#	sed s/toolname/$(menu_name)/ < $^ > $@
#	chmod +x $@

install: all
	install -D $(db_name) --target-directory $(bin_final_dir)
	install -D $(mms_name) --target-directory $(bin_final_dir)
	install -D $(menu_name) --target-directory $(bin_final_dir)
	install -D $(key_gen_name) --target-directory $(bin_final_dir)
	install -D scripts/$(db_monitor_name) $(bin_final_dir)
	install -D scripts/$(menu_gen_name) $(bin_final_dir)
	install -D config/$(db_name).conf $(conf_final_dir)/$(db_name).conf
	install -D config/$(mms_name).conf $(conf_final_dir)/$(mms_name).conf
	install -D doc/management.txt $(doc_final_dir)/management.txt
	install -D doc/intro.txt $(doc_final_dir)/intro.txt
	install -D doc/posts.txt $(doc_final_dir)/posts.txt

uninstall:
	rm -f $(bin_final_dir)$(db_name)
	rm -f $(bin_final_dir)$(mms_name)
	rm -f $(bin_final_dir)$(menu_name)
	rm -f $(bin_final_dir)$(key_gen_name)
	rm -f scripts/$(bin_final_dir)$(db_monitor_name)
	rm -f scripts/$(bin_final_dir)$(menu_gen_name)
	rm -f $(conf_final_dir)/$(db_name).conf
	rm -f $(conf_final_dir)/$(mms_name).conf
	rm -f $(doc_final_dir)/management.txt
	rm -f $(doc_final_dir)/intro.txt
	rm -f $(doc_final_dir)/posts.txt
	rmdir $(DESDIR)$(docdir)

.PHONY: clean
clean:
	rm -f $(exes) $(exe_objs) $(lib_objs) $(tarball_name).tar.gz Makefile.dep Makefile.dep
	rm -rf tmp

$(tarball_name).tar.gz:
	git archive --format=tar.gz --prefix=$(tarball_dir)/ HEAD > $(tarball_name).tar.gz

.PHONY: dist
dist: $(tarball_name).tar.gz

.PHONY: deb
deb: dist
	rm -rf tmp; mkdir tmp; mv $(tarball_name).tar.gz tmp; cd tmp; \
	ln $(tarball_name).tar.gz occase_1.0.0.orig.tar.gz; \
	tar -xvvzf $(tarball_name).tar.gz; \
	cd $(tarball_dir)/debian; debuild --no-sign -j1

backup_emails = laetitiapozwolski@yahoo.fr mzimbres@gmail.com bobkahnn@gmail.com occase.app@gmail.com

.PHONY: backup
backup: $(tarball_name).tar.gz
	echo "Backup" | mutt -s "Backup" -a $< -- $(backup_emails)

