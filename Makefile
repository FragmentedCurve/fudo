CFLAGS= -std=c99 -pedantic -pedantic-errors -Wall -Werror -O2

fudo: fudo.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f fudo

.PHONY: clean
