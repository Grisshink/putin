BUILD_MODE := RELEASE
LDFLAGS := -lm
CFLAGS := -Wall -Wextra
OBJFILES := putin.o miniaudio.o

ifeq ($(BUILD_MODE), RELEASE)
	CFLAGS += -O3 -s
else
	CFLAGS += -O0 -g
endif

all: putin
putin: $(OBJFILES)

putin.o: putin.c
	$(CC) $(CFLAGS) -c -o $@ $^

miniaudio.o: miniaudio.h
	$(CC) $(CFLAGS) -DMINIAUDIO_IMPLEMENTATION -x c -c -o $@ $^

clean:
	rm -f putin $(OBJFILES)
