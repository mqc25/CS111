files = lab0.c *.png README Makefile

.SILENT:

CC= gcc
CFLAGS = -g -Wall -Wextra

lab0:	lab0.o 
	$(CC) $(CFLAGS) lab0.o -o lab0

lab0.o: lab0.c
	$(CC) -c lab0.c

check:  lab0 allTest
	if [ -e log.txt ]; then \
	   echo "Failure occures."; \
	   cat log.txt; \
	else \
	   echo "All tests passed."; \
	fi; \
	rm -f log.txt 
clean:
	rm -f lab0 *.o *.txt *.tar.gz
dist: 
	tar -czf lab0-804844568.tar.gz $(files)

catch:
	./lab0 -sc 2> /dev/null; \
	if [ $$? -ne 4  ]; then \
	    echo "Catch test failed." >> log.txt; \
	fi 

basicIO:
	echo "Test." > input.txt; \
	./lab0 -i input.txt -o output.txt 2> /dev/null; \
	if [ $$? -ne 0 ]; then \
	   echo "Basic I/O failed." >> log.txt; \
	elif !(diff input.txt output.txt) ; then \
	   echo "Content of I/O not matched." >> log.txt; \
	fi; \
	rm -f input.txt output.txt 

fOpen:
	echo "Test." > input.txt; \
	chmod 333 input.txt; \
	./lab0 -i input.txt &>>/dev/null; \
	if [ $$? -ne 2 ]; then \
	   echo "Failed to catch non-readable input file." >> log.txt; \
	fi; \
	rm -f input.txt	
fWrite:
	echo "Test." > input.txt; \
	echo "Test." > output.txt; \
	chmod 555 output.txt; \
	./lab0 -i input.txt -o output.txt 2>/dev/null; \
	if [ $$? -ne 3 ]; then \
	   echo "Failed to catch non-writable output file." >> log.txt; \
	fi; \
	rm -f input.txt output.txt

allTest: catch basicIO fOpen fWrite
