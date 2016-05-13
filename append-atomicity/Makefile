all: will-it-blend
	./validate

will-it-blend: will-it-blend.c
	cc -pthread -ggdb -o $@ $^

clean:
	rm -f will-it-blend
	rm -f test

.PHONY = all clean
