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

bin_final_dir = $(DESTDIR)$(bindir)
doc_final_dir = $(DESTDIR)$(docdir)
conf_final_dir = $(DESTDIR)$(confdir)
service_final_dir = $(DESTDIR)$(systemddir)

ext_libs =
ext_libs += /opt/boost_1_71_0/lib/libboost_program_options.a

LDFLAGS = -lpthread
LDFLAGS += -lfmt
LDFLAGS += -lsodium
LDFLAGS += -lssl
LDFLAGS += -lcrypto

CPPFLAGS += -std=c++17
CPPFLAGS += -I. -I$./src -I/opt/boost_1_71_0/include -I/opt/aedis-1.0.0
CPPFLAGS += $(pkg-config --cflags libsodium)
CPPFLAGS += $(CXXFLAGS)
CPPFLAGS += -g #-O2

VPATH = ./src

exes =
exes += occase-db
exes += occase-mms
exes += occase-notify
exes += occase-key-gen
exes += occase-sim
exes += db_tests
exes += notify-test

common_objs += system.o
common_objs += logger.o
common_objs += crypto.o

db_objs =
db_objs += redis.o
db_objs += net.o
db_objs += post.o

mms_objs =
mms_objs += mms_session.o
mms_objs += net.o

client_objs =
client_objs += test_clients.o
client_objs += post.o

notify_objs =
notify_objs += notifier.o
notify_objs += ntf_session.o
notify_objs += net.o
notify_objs += logger.o

exe_objs = $(addsuffix .o, $(exes))

lib_objs =
lib_objs += $(db_objs)
lib_objs += $(mms_objs)
lib_objs += $(client_objs)
lib_objs += $(common_objs)
lib_objs += $(notify_objs)

aux = Makefile

all: $(exes)

Makefile.dep:
	-$(CXX) -MM ./src/*.cpp > $@

-include Makefile.dep

occase-sim: % : %.o $(client_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs)

db_tests: % : %.o $(client_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs)

occase-db: % : %.o $(db_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs) -DBOOST_ASIO_CONCURRENCY_HINT_1=BOOST_ASIO_CONCURRENCY_HINT_UNSAFE

occase-mms: % : %.o $(mms_objs) $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs) -DBOOST_ASIO_CONCURRENCY_HINT_1=BOOST_ASIO_CONCURRENCY_HINT_UNSAFE

occase-notify: % : %.o $(notify_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs) -DBOOST_ASIO_CONCURRENCY_HINT_1=BOOST_ASIO_CONCURRENCY_HINT_UNSAFE

occase-key-gen: % : %.o  $(common_objs)
	$(CXX) -o $@ $^ $(CPPFLAGS) -lfmt -lsodium

notify-test: % : %.o $(common_objs) ntf_session.o
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS) $(ext_libs) -DBOOST_ASIO_CONCURRENCY_HINT_1=BOOST_ASIO_CONCURRENCY_HINT_UNSAFE

install: all
	install -D occase-db --target-directory $(bin_final_dir)
	install -D occase-mms --target-directory $(bin_final_dir)
	install -D occase-key-gen --target-directory $(bin_final_dir)
	install -D occase-sim --target-directory $(bin_final_dir)
	install -D scripts/occase-db-monitor $(bin_final_dir)
	install -D scripts/occase-tree-gen $(bin_final_dir)
	install -D config/occase-db.conf $(conf_final_dir)/occase-db.conf
	install -D config/occase-mms.conf $(conf_final_dir)/occase-mms.conf
	install -D doc/management.txt $(doc_final_dir)/management.txt
	install -D doc/intro.txt $(doc_final_dir)/intro.txt

uninstall:
	rm -f $(bin_final_dir)/occase-db
	rm -f $(bin_final_dir)/occase-mms
	rm -f $(bin_final_dir)/occase-key-gen
	rm -f $(bin_final_dir)/occase-sim
	rm -f scripts/$(bin_final_dir)/occase-db-monitor
	rm -f scripts/$(bin_final_dir)/occase-tree-gen
	rm -f $(conf_final_dir)/occase-db.conf
	rm -f $(conf_final_dir)/occase-mms.conf
	rm -f $(doc_final_dir)/management.txt
	rm -f $(doc_final_dir)/intro.txt
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

