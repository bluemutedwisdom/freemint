#
# Makefile for storage
#

SHELL = /bin/sh
SUBDIRS = 

srcdir = .
top_srcdir = ../../../..
subdir = storage
toscommon_srcdir = ../../tos-common

default:

include ./STORAGEDEFS

include $(top_srcdir)/CONFIGVARS
include $(top_srcdir)/RULES
include $(top_srcdir)/PHONY


all-here: all-targets

# default overwrites

# default definitions
compile_all_dirs = .compile_*
GENFILES = $(compile_all_dirs) *.udd *.prg
SGENFILES = crtinit.c delayasm.S entry.S setstack.S

all-targets:
	@set fnord $(MAKEFLAGS); amf=$$2; \
	for i in $(storagetargets); do \
		echo "Making $$i"; \
		($(MAKE) $$i) \
		|| case "$$amf" in *=*) exit 1;; *k*) fail=yes;; *) exit 1;; esac; \
	done && test -z "$$fail"

$(storagetargets): 
	$(MAKE) buildstorage storage=$@

#
# multi target stuff
#

ifneq ($(storage),)

compile_dir = .compile_$(storage)
storagetarget = _stmp_$(storage)
realtarget = $(storagetarget)

$(storagetarget): $(compile_dir)
	cd $(compile_dir); $(MAKE) all

$(compile_dir): Makefile.objs
	$(MKDIR) -p $@
	$(CP) $< $@/Makefile

# Link common TOS files
ifeq ($(storage),prg)
	ln -s $(toscommon_srcdir)/crtinit.c $(srcdir)/crtinit.c; \
	ln -s $(toscommon_srcdir)/delayasm.S $(srcdir)/delayasm.S; \
	ln -s $(toscommon_srcdir)/entry.S $(srcdir)/entry.S; \
	ln -s $(toscommon_srcdir)/setstack.S $(srcdir)/setstack.S
endif

else

realtarget =

endif

buildstorage: $(realtarget)
