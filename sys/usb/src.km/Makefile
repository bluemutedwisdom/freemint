#
# Makefile for usb
#

SHELL = /bin/sh
SUBDIRS = loader ucd udd

srcdir = .
top_srcdir = ../..
subdir = src.km
toscommon_srcdir = ./tos-common

default: help

include ./USBDEFS

include $(top_srcdir)/CONFIGVARS
include $(top_srcdir)/RULES
include $(top_srcdir)/PHONY


all-here: all-targets

# default overwrites

# default definitions
compile_all_dirs = .compile_*
GENFILES = $(compile_all_dirs) *.km
SGENFILES = crtinit.c delayasm.S entry.S setstack.S


help:
	@echo '#'
	@echo '# targets:'
	@echo '# --------'
	@echo '# - all'
	@echo '# - $(usbtargets)'
	@echo '#'
	@echo '# - clean'
	@echo '# - distclean'
	@echo '# - help'
	@echo '#'

all-targets:
	@set fnord $(MAKEFLAGS); amf=$$2; \
	for i in $(usbtargets); do \
		echo "Making $$i"; \
		($(MAKE) $$i) \
		|| case "$$amf" in *=*) exit 1;; *k*) fail=yes;; *) exit 1;; esac; \
	done && test -z "$$fail"

$(usbtargets):
	$(MAKE) buildusb usb=$@

#
# multi target stuff
#

ifneq ($(usb),)

compile_dir = .compile_$(usb)
usbtarget = _stmp_$(usb)
realtarget = $(usbtarget)

$(usbtarget): $(compile_dir)
	cd $(compile_dir); $(MAKE) all

$(compile_dir): Makefile.objs
	$(MKDIR) -p $@
	$(CP) $< $@/Makefile

# Link common TOS files
ifeq ($(usb),prg)
	ln -s $(toscommon_srcdir)/crtinit.c $(srcdir)/crtinit.c; \
	ln -s $(toscommon_srcdir)/delayasm.S $(srcdir)/delayasm.S; \
	ln -s $(toscommon_srcdir)/entry.S $(srcdir)/entry.S; \
	ln -s $(toscommon_srcdir)/setstack.S $(srcdir)/setstack.S
endif

else

realtarget =

endif

buildusb: $(realtarget)
