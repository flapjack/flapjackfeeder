# flapjackfeeder Makefile

HIREDIS = hiredis
SRC = src/flapjackfeeder.c
DEP = $(HIREDIS)/libhiredis.a

# fallback to gcc when $CC is not in $PATH.
CC := $(shell sh -c 'type $(CC) >/dev/null 2>/dev/null && echo $(CC) || echo gcc')
# for recursive make
ifeq ($(MAKE),)
	MAKE := make
endif
OPTIMIZATION ?= -O3
DEBUG ?= -g -ggdb
REAL_CFLAGS = $(OPTIMIZATION) -fPIC $(CFLAGS) $(DEBUG) $(ARCH) -DHAVE_CONFIG_H -DNSCORE -DVERSION='"$(FLAPJACKFEEDER_VERSION)"' 

# get version from ENV or git
ifndef VERSION 
	FLAPJACKFEEDER_VERSION := $(shell git describe --abbrev=7 --dirty --always --tags)
else
	FLAPJACKFEEDER_VERSION := $(VERSION)
endif

FF3 = flapjackfeeder3-$(FLAPJACKFEEDER_VERSION).o
FF4 = flapjackfeeder4-$(FLAPJACKFEEDER_VERSION).o

all: $(FF3) $(FF4)

# dep
$(DEP):
	if [ ! -d $(HIREDIS) ] ; then git clone https://github.com/redis/hiredis.git $(HIREDIS) ; fi
	# v0.12.1 is known to work, but I prefer not to pin it to an old version
	#git checkout v0.12.1
	$(MAKE) -C $(HIREDIS) static

# modules:
$(FF3): $(SRC) $(DEP)
	$(CC) $(REAL_CFLAGS) -o $@ $< -shared $(DEP)
	strip $@
$(FF4): $(SRC) $(DEP)
	$(CC) $(REAL_CFLAGS) -DHAVE_NAEMON_H -o $@ $< -shared $(DEP)
	strip $@

clean:
	rm -rf *.o $(HIREDIS)

.PHONY: all clean dep
