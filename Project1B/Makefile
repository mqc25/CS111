.SILENT:

files = lab1b-client.c lab1b-server.c Makefile README

default: lab1b-client lab1b-server

lab1b-client: 
	gcc  -o lab1b-client -Wall -Wextra -g -lz lab1b-client.c

lab1b-server:
	gcc  -o lab1b-server -Wall -Wextra -g -lz lab1b-server.c

clean:
	rm -rf lab1b-client lab1b-server *.tar.gz

dist:
	tar -zcvf lab1b-804844568.tar.gz $(files) 
