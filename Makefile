CFLAGS=-g -Wall

TARGETS= testget

all: $(TARGETS)

rclient.o ptwist168.o testget.o:: ptwist.h

testget: testget.o rclient.o ptwist168.o ptwist.h 
	gcc -g -o $@ $^ -L../../../openssl/run/lib -lssl -lcrypto -ldl

clean:
	-rm *.o

veryclean: clean
	-rm $(TARGETS)
