.PHONY: samples test

CFLAGS=-g -march=native -Ofast
LDFLAGS=-lm

all: derasterize test

derasterize:
	$(CC) $(CFLAGS) derasterize.c -o derasterize $(LDFLAGS)

test:
	chmod +x derasterize.c
	./derasterize.c -y12 -x30 ./samples/snake.jpg | ./tally.sh

samples:
	for file in samples/* ; do ./derasterize.c -y20 -x70 $$file > $$file.uaart ; done

clean:
	rm derasterize

mrproper:
	rm samples/*uaart
