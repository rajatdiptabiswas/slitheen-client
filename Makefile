CFLAGS=-g -Wall

TARGETS= socks

all: $(TARGETS)

socks: socks5proxy.c
	gcc -o $@ $^ -lpthread -lssl -lcrypto

clean:
	-rm *.o

veryclean: clean
	-rm $(TARGETS)
