SRCS = avs2pipemod.c actions.c wave.c common.c

OBJS = $(SRCS:%.c=%.o)
DOBJS = $(SRCS:%.c=%.d)

CC = $(CROSS)gcc
LD = $(CROSS)gcc

CFLAGS = -Wall -std=gnu99 -I. -g0 -Os -ffast-math -fexcess-precision=fast
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
	$(RM) *.exe *.o *.d .depend .dbgdep

%.o: %.c .depend
	$(CC) $(CFLAGS) $(XCFLAGS) -c $< -o $@

%.d: %.c .dbgdep
	$(CC) $(DCFLAGS) -c $< -o $@

ifneq ($(wildcard .depend),)
include .depend
endif
ifneq ($(wildcard  .dbgdep),)
include .dbgdep
endif

.depend:
	@$(RM) .depend
	@$(foreach SRC, $(SRCS), $(CC) $(SRC) $(CFLAGS) -MT $(SRC:%.c=%.o) -MM >> .depend;)

.dbgdep:
	@$(RM) .dbgdep
	@$(foreach SRC, $(SRCS), $(CC) $(SRC) $(CFLAGS) -MT $(SRC:%.c=%.d) -MM >> .dbgdep;)