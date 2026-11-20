CC ?= arm-linux-gnueabihf-gcc
CFLAGS ?= -O2 -Wall -D_GNU_SOURCE -march=armv7-a -mfloat-abi=hard -mfpu=neon -I.
LDFLAGS ?= -ldl -lpthread -lm

TARGET = ../ragingthunder2
SRCS = main.c fake_jni.c bionic_loader.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
