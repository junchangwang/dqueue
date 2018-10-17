TESTS = wfqueue wfqueue0 lcrq ccqueue msqueue dqueue ltqueue faa delay

CC = gcc
CFLAGS = -g -Wall -O3 -pthread -D_GNU_SOURCE -std=c99
LDLIBS = -lpthread -lm

ifeq (${VERIFY}, 1)
	CFLAGS += -DVERIFY
endif

ifeq (${SANITIZE}, 1)
	CFLAGS += -fsanitize=address -fno-omit-frame-pointer
	LDLIBS += -lasan
	LDFLAGS = -fsanitize=address
endif

ifdef JEMALLOC_PATH
	LDFLAGS += -L${JEMALLOC_PATH}/lib -Wl,-rpath,${JEMALLOC_PATH}/lib
	LDLIBS += -ljemalloc
endif

all: $(TESTS)

wfqueue0: CFLAGS += -DMAX_PATIENCE=0
wfqueue0.o: wfqueue.c
	$(CC) $(CFLAGS) -c -o $@ $^

haswell: CFLAGS += -DGUADALUPE_COMPACT
haswell: all

mic: CC = /usr/linux-k1om-4.7/bin/x86_64-k1om-linux-gcc
mic: CFLAGS += -DGUADALUPE_MIC_COMPACT -DLOGN_OPS=6
mic biou: $(filter-out lcrq,$(TESTS))

biou: CFLAGS += -DBIOU_COMPACT

wfqueue wfqueue0: CFLAGS += -DWFQUEUE
lcrq: CFLAGS += -DLCRQ
ccqueue: CFLAGS += -DCCQUEUE
msqueue: CFLAGS += -DMSQUEUE
dqueue: CFLAGS += -DDQUEUE
ltqueue: CFLAGS += -DLTQUEUE
faa: CFLAGS += -DFAAQ
delay: CFLAGS += -DDELAY

$(TESTS): harness.o
ifeq (${R730}, 1)
CFLAGS += -DCPU_MAP_R730
endif
ifeq (${S24}, 1)
CFLAGS += -DCPU_MAP_S24
else ifeq (${S56}, 1)
CFLAGS += -DCPU_MAP_S56
endif
ifeq (${HALFHALF}, 1)
$(TESTS): halfhalf.o
else ifeq (${PAIRWISE}, 1)
$(TESTS): pairwise.o
else
$(TESTS): mpsc.o
endif
ifeq (${WORKLOAD}, 1)
CFLAGS += -DWORKLOAD
endif

msqueue lcrq: hzdptr.o xxhash.o

clean:
	rm -fr $(TESTS) *.o log-*
