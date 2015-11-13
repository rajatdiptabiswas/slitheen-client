CFLAGS=-g -Wall

TARGETS= testget

all: $(TARGETS)

rclient.o ptwist168.o testget.o:: ptwist.h

testget: testget.o rclient.o ptwist168.o ptwist.h
	gcc -g -o $@ $^ -I/home/cbocovic/Documents/openssl/run/include/openssl libssl.a libcrypto.a -ldl

clean:
	-rm *.o

veryclean: clean
	-rm $(TARGETS)
