SRCS = avs2pipemod.c actions.c wave.c common.c

OBJS = $(SRCS:%.c=%$(SUFFIX).o)
DOBJS = $(SRCS:%.c=%$(SUFFIX).d)

CC = $(CROSS)gcc
LD = $(CROSS)gcc

CFLAGS = -Wall -std=gnu99 -I. -g0 -Os -ffast-math
DCFLAGS = -Wall -std=gnu99 -I. -g3 -O0

LDFLAGS = -Wl,-s -L.
DLDFLAGS = -L.

TARGET = avs2pipemod$(SUFFIX).exe
DEBUG = avs2pipemod$(SUFFIX)_dbg.exe

.PHONY: all debug clean

all: $(TARGET)

debug: $(DEBUG)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) $(XLDFLAGS) -o $@ $^

$(DEBUG): $(DOBJS)
	$(LD) $(DLDFLAGS) -o $@ $^

clean:
	$(RM) *.exe *.o *.d .depend* .dbgdep*

%$(SUFFIX).o: %.c .depend$(SUFFIX)
	$(CC) $(CFLAGS) $(XCFLAGS) -c $< -o $@

%$(SUFFIX).d: %.c .dbgdep$(SUFFIX)
	$(CC) $(DCFLAGS) -c $< -o $@

ifneq ($(wildcard .depend$(SUFFIX)),)
include .depend$(SUFFIX)
endif
ifneq ($(wildcard  .dbgdep$(SUFFIX)),)
include .dbgdep$(SUFFIX)
endif

.depend$(SUFFIX):
	@$(RM) .depend
	@$(foreach SRC, $(SRCS), $(CC) $(SRC) $(CFLAGS) -MT $(SRC:%.c=%.o) -MM >> .depend$(SUFFIX);)

.dbgdep$(SUFFIX):
	@$(RM) .dbgdep
	@$(foreach SRC, $(SRCS), $(CC) $(SRC) $(CFLAGS) -MT $(SRC:%.c=%.d) -MM >> .dbgdep$(SUFFIX);)