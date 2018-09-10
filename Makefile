CC=gcc
files=mem.c Makefile README.txt

project3: mem.c
	$(CC) -O2 -DNDEBUG -Wall -fPIC -shared mem.c -o libmyalloc.so 

debug: mem.c
	$(CC) -Wall -g -fPIC -shared mem.c -o libmyalloc.so

tar:
	tar cvzf project3.tgz $(files)

