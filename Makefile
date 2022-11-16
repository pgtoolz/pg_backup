# pg_probackup build system
#
# You can build pg_probackup in different ways:
#
# 1. in source tree using PGXS (with already installed PG and existing PG sources)
# git clone https://github.com/postgrespro/pg_probackup pg_probackup
# cd pg_probackup
# make USE_PGXS=1 PG_CONFIG=<path_to_pg_config> top_srcdir=<path_to_PostgreSQL_source_tree>
#
# 2. out of source using PGXS
# git clone https://github.com/postgrespro/pg_probackup pg_probackup-src
# mkdir pg_probackup-build && cd pg_probackup-build
# make USE_PGXS=1 PG_CONFIG=<path_to_pg_config> top_srcdir=<path_to_PostgreSQL_source_tree> -f ../pg_probackup-src/Makefile
#
# 3. in PG source (without PGXS -- using only PG sources)
# git clone https://git.postgresql.org/git/postgresql.git postgresql
# git clone https://github.com/postgrespro/pg_probackup postgresql/contrib/pg_probackup
# cd postgresql
# ./configure ... && make
# make -C contrib/pg_probackup
#
# 4. out of PG source and without PGXS
# git clone https://git.postgresql.org/git/postgresql.git postgresql-src
# git clone https://github.com/postgrespro/pg_probackup postgresql-src/contrib/pg_probackup
# mkdir postgresql-build && cd postgresql-build
# ../postgresql-src/configure ... && make
# make -C contrib/pg_probackup
#
top_pbk_srcdir := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))

PROGRAM := pg_probackup

# pg_probackup sources
OBJS := src/utils/configuration.o src/utils/json.o src/utils/logger.o \
	src/utils/parray.o src/utils/pgut.o src/utils/thread.o src/utils/remote.o src/utils/file.o
OBJS += src/archive.o src/backup.o src/catalog.o src/checkdb.o src/configure.o src/data.o \
	src/delete.o src/dir.o src/fetch.o src/help.o src/init.o src/merge.o \
	src/parsexlog.o src/ptrack.o src/pg_probackup.o src/restore.o src/show.o src/stream.o \
	src/util.o src/validate.o src/datapagemap.o src/catchup.o \
	src/compatibility/pg-11.o

# sources borrowed from postgresql (paths are relative to pg top dir)
BORROWED_H_SRC := \
	src/bin/pg_basebackup/receivelog.h \
	src/bin/pg_basebackup/streamutil.h \
	src/bin/pg_basebackup/walmethods.h
BORROWED_C_SRC := \
	src/backend/access/transam/xlogreader.c \
	src/bin/pg_basebackup/receivelog.c \
	src/bin/pg_basebackup/streamutil.c \
	src/bin/pg_basebackup/walmethods.c

BORROW_DIR := src/borrowed
BORROWED_H := $(addprefix $(BORROW_DIR)/, $(notdir $(BORROWED_H_SRC)))
BORROWED_C := $(addprefix $(BORROW_DIR)/, $(notdir $(BORROWED_C_SRC)))
OBJS += $(patsubst %.c, %.o, $(BORROWED_C))
EXTRA_CLEAN := $(BORROWED_H) $(BORROWED_C) $(BORROW_DIR) borrowed.mk

OBJS += src/fu_util/impl/ft_impl.o src/fu_util/impl/fo_impl.o

# off-source build support
ifneq ($(abspath $(CURDIR))/, $(top_pbk_srcdir))
VPATH := $(top_pbk_srcdir)
endif

# standard PGXS stuff
# all OBJS must be defined above this
ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir=contrib/pg_probackup
top_builddir=../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

ifndef S3_DIR
  ifneq ("$(wildcard $(abspath $(top_pbk_srcdir))/../s3)", "")
    S3_DIR = $(abspath $(CURDIR))/../s3
  endif
endif

ifdef S3_DIR
  LDFLAGS += -lcurl
  CFLAGS += $(shell pkg-config --cflags libxml-2.0) -DPBCKP_S3=1
  LDFLAGS += $(shell pkg-config --libs libxml-2.0)
  OBJS += $(S3_DIR)/s3.o
endif

#
PG_CPPFLAGS = -I$(libpq_srcdir) ${PTHREAD_CFLAGS} -I$(top_pbk_srcdir)src -I$(BORROW_DIR)
PG_CPPFLAGS += -I$(top_pbk_srcdir)src/fu_util -Wno-declaration-after-statement
ifdef VPATH
PG_CPPFLAGS += -Isrc
endif
override CPPFLAGS := -DFRONTEND $(CPPFLAGS) $(PG_CPPFLAGS)
PG_LIBS_INTERNAL = $(libpq_pgport) ${PTHREAD_CFLAGS}

# additional dependencies on borrowed files
src/backup.o src/catchup.o src/pg_probackup.o: $(BORROW_DIR)/streamutil.h
ifdef S3_DIR
  src/backup.o src/catchup.o src/pg_probackup.o: $(S3_DIR)/s3.o
endif
src/stream.o $(BORROW_DIR)/receivelog.o $(BORROW_DIR)/streamutil.o $(BORROW_DIR)/walmethods.o: $(BORROW_DIR)/receivelog.h
$(BORROW_DIR)/receivelog.h: $(BORROW_DIR)/walmethods.h

# generate separate makefile to handle borrowed files
borrowed.mk: $(firstword $(MAKEFILE_LIST))
	$(file >$@,# This file is autogenerated. Do not edit!)
	$(foreach borrowed_file, $(BORROWED_H_SRC) $(BORROWED_C_SRC), \
		$(file >>$@,$(addprefix $(BORROW_DIR)/, $(notdir $(borrowed_file))): | $(CURDIR)/$(BORROW_DIR)/ $(realpath $(top_srcdir)/$(borrowed_file))) \
		$(file >>$@,$(shell echo "	"'$$(LN_S) -f $(realpath $(top_srcdir)/$(borrowed_file)) $$@')) \
	)
include borrowed.mk

# create needed directories for borrowed files and off-source build
OBJDIRS = $(addprefix $(CURDIR)/, $(sort $(dir $(OBJS))))
$(OBJS): | $(OBJDIRS)
$(OBJDIRS):
	mkdir -p $@

# packaging infrastructure
WORKDIR ?= $(CURDIR)
PBK_PKG_BUILDDIR = $(WORKDIR)/pkg-build/
PBK_GIT_REPO = https://github.com/postgrespro/pg_probackup

include $(top_pbk_srcdir)/packaging/Makefile.pkg
include $(top_pbk_srcdir)/packaging/Makefile.repo
include $(top_pbk_srcdir)/packaging/Makefile.test

