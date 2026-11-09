ifneq ($(KERNELRELEASE),)
# kbuild part of makefile
ccflags-y := -I$(src)/inc
ccflags-y += -g		# add debugging info
obj-m := hello1.o hello2.o
else
# normal makefile
KDIR ?= /lib/modules/$(shell uname -r)/build

# CROSS_COMPILE may start with "ccache", binutils are called without it
TOOLS_PREFIX := $(lastword $(CROSS_COMPILE))
STRIP   := $(TOOLS_PREFIX)strip
OBJDUMP := $(TOOLS_PREFIX)objdump

MODULES := hello1 hello2

# Keep a copy with debugging info for gdb/objdump and strip only
# debugging info from the .ko that goes to the target. A .ko that is
# already stripped (nothing was rebuilt) is not copied again.
default:
	$(MAKE) -C $(KDIR) M=$$PWD
	for m in $(MODULES); do \
		if $(OBJDUMP) -h $$m.ko | grep -q '\.debug_info'; then \
			cp $$m.ko $$m.ko.unstripped && \
			$(STRIP) -g $$m.ko || exit 1; \
		fi; \
	done

# disassembly mixed with the source code
dis: default
	for m in $(MODULES); do \
		$(OBJDUMP) -dS $$m.ko.unstripped > $$m.dis || exit 1; \
	done

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean
	rm -f *.ko.unstripped *.dis

checkpatch:
	$(KDIR)/scripts/checkpatch.pl --no-tree -f hello1.c hello2.c inc/hello1.h

# just use "make hello1.s" instead of objdump
%.s %.i: %.c
	$(MAKE) -C $(KDIR) M=$$PWD $@

.PHONY: default dis clean checkpatch
endif
