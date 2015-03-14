all:
	gcc -Wall -o simple-shell simple-shell.c -I.
clean:
	$(RM) simple-shell