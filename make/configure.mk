# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2018 Drew Richardson <drewrichardson@gmail.com>

include $(PROJROOT)make/quiet.mk
include build/make/env.mk

all: build/make/dr_config.mk build/make/flags.mk build/make/dr_config_h.mk

build/make/dr_config.mk: build/make/env.mk build/make/compiler.mk build/make/eext.mk build/make/oext.mk build/make/cstd.mk
	$(E_GEN)cat build/make/env.mk build/make/compiler.mk build/make/eext.mk build/make/oext.mk build/make/cstd.mk > $@

build/make/compiler.mk: build/make/env.mk $(PROJROOT)config/checks/ALWAYS_SUCCEEDS.c
	$(E_GEN) \
if rm -f build/make_obj/cl.test.obj && \
        $(CC) $(CPPFLAGS) $(CFLAGS) $(PROJROOT)config/checks/ALWAYS_SUCCEEDS.c /c /Fo: build/make_obj/cl.test.obj > /dev/null 2>&1 && \
        [ -f build/make_obj/cl.test.obj ]; then \
    printf "%s\n" \
        "OUTPUT_C=/c /Fo" \
        "OUTPUT_L=/Fe" \
        "LDLIB_PREFIX=" \
        "AEXT=.asm" \
        "LEXT=.lib"; \
else \
    if rm -f build/make_obj/cc.test.o && \
            $(CC) $(CPPFLAGS) $(CFLAGS) $(PROJROOT)config/checks/ALWAYS_SUCCEEDS.c -c -o build/make_obj/cc.test.o > /dev/null 2>&1 && \
            [ -f build/make_obj/cc.test.o ]; then \
        printf "%s\n" \
           "OUTPUT_C=-c -o " \
           "OUTPUT_L=-o " \
           "LDLIB_PREFIX=-l" \
           "AEXT=.S" \
           "LEXT="; \
    else \
        false; \
    fi \
fi > $@

build/make/eext.mk: build/make/env.mk $(PROJROOT)config/checks/ALWAYS_SUCCEEDS.c
	$(E_GEN) \
if cd build/make_obj && \
        rm -f a.out a.exe ALWAYS_SUCCEEDS.eext.exe && \
        cp ../../$(PROJROOT)config/checks/ALWAYS_SUCCEEDS.c ALWAYS_SUCCEEDS.eext.c && \
        $(CC) $(CPPFLAGS) $(CFLAGS) ALWAYS_SUCCEEDS.eext.c > /dev/null 2>&1 && \
        [ -f a.out ]; then \
    echo EEXT=; \
else \
    if [ -f a.exe -o -f ALWAYS_SUCCEEDS.eext.exe ]; then \
        echo EEXT=.exe; \
    else \
        false; \
    fi \
fi > $@

build/make/oext.mk: build/make/env.mk $(PROJROOT)config/checks/ALWAYS_SUCCEEDS.c
	$(E_GEN) \
if \
        cd build/make_obj && \
        rm -f ALWAYS_SUCCEEDS.oext.o ALWAYS_SUCCEEDS.oext.obj && \
        cp ../../$(PROJROOT)config/checks/ALWAYS_SUCCEEDS.c ALWAYS_SUCCEEDS.oext.c && \
        $(CC) $(CPPFLAGS) $(CFLAGS) ALWAYS_SUCCEEDS.oext.c -c > /dev/null 2>&1 && \
        [ -f ALWAYS_SUCCEEDS.oext.o ]; then \
    echo OEXT=.o; \
else \
    if [ -f ALWAYS_SUCCEEDS.oext.obj ]; then \
        echo OEXT=.obj; \
    else \
        false; \
    fi \
fi > $@

build/make/cstd.mk: build/make/env.mk $(PROJROOT)config/checks/ALWAYS_SUCCEEDS.c
	$(E_GEN) \
if cd build/make_obj && \
        cp ../../$(PROJROOT)config/checks/ALWAYS_SUCCEEDS.c ALWAYS_SUCCEEDS.cstd.c && \
        ! ($(CC) -std=gnu11 $(CPPFLAGS) $(CFLAGS) ALWAYS_SUCCEEDS.cstd.c -o stdgnu11 2>&1 || \
        echo error) | egrep -i 'error|warn' > /dev/null; then \
    echo CSTD=-std=gnu11; \
else \
    if ! ($(CC) -std=gnu99 $(CPPFLAGS) $(CFLAGS) ALWAYS_SUCCEEDS.cstd.c -o stdgnu99 2>&1 || \
            echo error) | egrep -i 'error|warn' > /dev/null; then \
        echo CSTD=-std=gnu99; \
    else \
        if ! ($(CC) -xc11 $(CPPFLAGS) $(CFLAGS) ALWAYS_SUCCEEDS.cstd.c -o xc11 2>&1 || \
                echo error) | egrep -i 'error|warn' > /dev/null; then \
            echo CSTD=-xc11; \
        else \
            if ! ($(CC) -xc99 $(CPPFLAGS) $(CFLAGS) ALWAYS_SUCCEEDS.cstd.c -o xc99 2>&1 || \
                    echo error) | egrep -i 'error|warn' > /dev/null; then \
                echo CSTD=-xc99; \
            else \
                echo CSTD=; \
            fi \
        fi \
    fi \
fi > $@

build/make/flags.mk:
	$(E_GEN). $(PROJROOT)make/flags.sh > $@

build/make/dr_config_h.mk:
	$(E_GEN). $(PROJROOT)make/dr_config_h.sh > $@
