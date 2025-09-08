TOPDIR = $(shell pwd)

VERSION := 1.4

CROSS_COMPILE ?=
O ?= $(TOPDIR)

o := $(O)/

CC ?= $(CROSS_COMPILE)gcc
AR ?= $(CROSS_COMPILE)ar
INSTALL ?= install

# install directories
PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
INCLUDEDIR ?= $(PREFIX)/include
LIBDIR ?= $(PREFIX)/lib

ALL_TARGETS :=
CLEAN_TARGETS :=

IS_GIT := $(shell if [ -d .git ] ; then echo yes ; else echo no; fi)
ifeq ($(IS_GIT),yes)
VERSION := $(shell git describe --abbrev=8 --dirty --always --tags --long)
endif

# let the user override the default CFLAGS/LDFLAGS
CFLAGS ?= -O2
LDFLAGS ?=

MY_CFLAGS := $(CFLAGS)
MY_LDFLAGS := $(LDFLAGS)

MY_CFLAGS += -DVERSION=\"$(VERSION)\"

define compile_tgt
	@mkdir -p $(dir $@)
	$(CC) -MD -MT $@ -MF $(@:.o=.d) $(CPPFLAGS) $($(1)_CPPFLAGS) $(MY_CFLAGS) $($(1)_CFLAGS) -c -o $@ $<
endef

define link_tgt
	$(CC) $(MY_LDFLAGS) $($(1)_LDFLAGS) -o $@ $(filter-out %.a,$^) -L/ $(addprefix -l:,$(filter %.a,$^)) $(LIBS) $($(1)_LIBS)
endef

MY_CFLAGS += -Wall -W -Werror

ifeq ($(DEBUG),1)
	MY_CFLAGS += -g -O0
endif

ifeq ($(COVERAGE),1)
	MY_CFLAGS += --coverage
	MY_LDFLAGS += --coverage
endif

DEPS := $(shell find $(o) -name '*.d')

all: real-all

include mspm0flash.mk

real-all: $(ALL_TARGETS)

.PHONY: $(CLEAN_TARGETS) clean
clean: $(CLEAN_TARGETS)
	rm -f $(DEPS)

.PHONY: $(INSTALL_TARGETS) install
install: $(INSTALL_TARGETS)

-include $(DEPS)
