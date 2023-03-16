CFLAGS= -O2 -Wall

fudo: fudo.c
	$(CC) $(CFLAGS) -o $@ fudo.c

clean:
	rm -f fudo
