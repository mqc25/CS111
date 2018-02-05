#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <netdb.h>
#include <netinet/in.h>
#include "zlib.h"

#define SIZE 4096

unsigned char* compBuf;
unsigned char buffer[SIZE];
int pid =-1;
int sockfd, newsockfd;
int compFlag = 0;
z_stream toShell;
z_stream toOut;



void printError(char* a){
	int err = errno;
	fprintf(stderr, strerror(err));
	fprintf(stderr, a);
}

void endProg()
{
	if((pid!= -1)  && (pid!= 0)){
		int status;
		pid_t p;
		if((p = waitpid(pid,&status,0)) == -1){
			printError("waitpid fail.\n");
		}
		else {
			fprintf(stderr,"SHELL SIGNAL=%d STATUS=%d", WTERMSIG(status), WEXITSTATUS(status));
		}
	}
	close(newsockfd);
	close(sockfd);
}


void signalHandler(int sig){
	if(sig == SIGPIPE){
		exit(0);
	}
}

int zp(int num){
	toShell.zalloc = Z_NULL;
	toShell.zfree = Z_NULL;
	toShell.opaque = Z_NULL;
	
	if(deflateInit(&toShell, Z_DEFAULT_COMPRESSION) != Z_OK){
		fprintf(stderr, "Fail to initialize deflate.\n");
		exit(1);
	}
	
	toShell.avail_in = num;
	toShell.next_in = buffer;
	toShell.avail_out = SIZE;
	toShell.next_out = compBuf;

	do{
		deflate(&toShell, Z_SYNC_FLUSH);
	}	while(toShell.avail_in > 0);
	num = SIZE - toShell.avail_out;
	deflateEnd(&toShell);
	return num;
}

int unzp(int num){
	toOut.zalloc = Z_NULL;
	toOut.zfree = Z_NULL;
	toOut.opaque = Z_NULL;
		
	if( inflateInit(&toOut) != Z_OK){
			fprintf(stderr, "Fail to initialize inflate.\n");
			exit(1);
		}
	
	toOut.avail_in = num;
	toOut.next_in = buffer;
	toOut.avail_out = SIZE;
	toOut.next_out = compBuf;

	do{
		inflate(&toOut, Z_SYNC_FLUSH);
	} while(toOut.avail_in > 0);
	
	num = SIZE - toOut.avail_out;
	inflateEnd(&toOut);
	return num;
}


int inToOut(int in, int out){
	int num;
	int done = 0;
	num = read(in, buffer, SIZE);
	if (num == -1){
		printError("Fail to read data.\n");
		exit(1);
	}
	else if (num == 0){
		return -1;
	}
	
	if (compFlag){
		if(in == newsockfd){
			num = unzp(num);
			memcpy(buffer,compBuf,num);
			inflateEnd(&toShell);
		}
	}
	
	if (in == newsockfd){
		int i;
		for (i = 0; i < num; i ++){
			char temp = *(buffer+i);
			
			if (temp == 0x03){
				kill(pid,SIGINT);
			}
			else if( temp == 0x04 ) {
				num = i;
				done = 2;
				break;
			}
		}
	}
	else if (compFlag == 1){
		
		num = zp(num);
		memcpy(buffer,compBuf,num);
		deflateEnd(&toOut);
	}
	
	if (write(out,buffer,num) < num){
		printError("Fail to write to output correctly.\n");
		exit(1);
	}
  
	return done;
}

int inToOutWithShell(int terminalIn, int terminalOut, int shellIn, int shellOut){
	struct pollfd checkInput[2]; // 0 terminal     1 shell
	
	
	checkInput[0].fd = terminalIn;
	checkInput[1].fd = shellOut;
	
	checkInput[0].events = POLLIN | POLLHUP | POLLERR;
	checkInput[1].events = POLLIN | POLLHUP | POLLERR;
	
	while(1){
		int pollReturn = poll(checkInput,2,0);
		if (pollReturn == -1){
			printError("poll() fail.\n");
			exit(1);
		}
		else if (pollReturn == 0) 
		{
			if (recv(terminalIn, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT) == 0){
				close(shellIn);
			}
			continue;
		}
		if(checkInput[0].revents & POLLIN){
			int num = inToOut(terminalIn,shellIn);
			if( num == 1){
				printError("Reading from terminal fail.\n");
				exit(1);
			}
			else if(num == -1){
				exit(0);
			}
			else if (num == 2){
				close(shellIn);
			}
		}
		
		if(checkInput[1].revents & POLLIN){
			int num = inToOut(shellOut,terminalOut);
			if( num == 1){
				printError("Reading from shell to stdout fail.\n");
				exit(1);
			}
			else if(num == -1){
				exit(0);
			}
			if( num == 2){
				exit(0);
			}
		}	
		if(checkInput[1].revents & (POLLHUP | POLLERR)){
			exit(0);
		}
		
	}
}

int main(int argc, char *argv[]){
	
	
	int opt;
	static struct option long_opts[] = {
		{"port", required_argument, NULL, 'p'},
		{"compress", no_argument, NULL, 'c'},
		{0, 0, 0, 0}
	};
	int portno = 0;
	while( (opt = getopt_long(argc, argv, "cp:", long_opts, NULL)) != -1){
		switch(opt){
		case 'p':
			portno = atoi(optarg);
			if(portno <= 1024){
				fprintf(stderr,"Error: Port number must be larger than 1024.\n");
				exit(1);
			}
			break;
		case 'c':
			compFlag = 1;
			break;
		case '?':
			fprintf(stderr, "Wrong argument format. Usage: %s -p=portNumber \n",argv[0]);
			exit(1);
		default:
			break;
		}
	}
	if(portno == 0){
		fprintf(stderr, "Wrong argument format. Usage: %s -p=portNumber \n",argv[0]);
		exit(1);
	}
	if(compFlag){
		compBuf = (unsigned char*)malloc(SIZE);
	}
	signal(SIGPIPE,signalHandler);
	atexit(endProg);
	
	
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;

	/* First call to socket() function */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
	  perror("ERROR opening socket");
	  exit(1);
	}
	
	/* Initialize socket structure */
	memset((char *) &serv_addr,0 ,sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	/* Now bind the host address using bind() call.*/
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
	  perror("ERROR on binding");
	  exit(1);
	}
	
	/* Now start listening for the clients, here process will
	  * go in sleep mode and will wait for the incoming connection
	*/

	listen(sockfd,5);
	clilen = sizeof(cli_addr);
	
	/* Accept actual connection from the client */
	newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
	
	if (newsockfd < 0) {
	  perror("ERROR on accept");
	  exit(1);
	}
	
	int childToParent[2];
	int parentToChild[2];
	if( pipe(childToParent) || pipe(parentToChild)){
			printError("Fail to create pipe properly.\n");
			exit(1);
	}
	
	pid = fork();
		
	if (pid == 0){   // child process only need 1 parentToChild, 0 childToParent
		close(sockfd);
		close(newsockfd);
		close(parentToChild[1]);
		close(childToParent[0]);
		dup2(parentToChild[0],0);
		dup2(childToParent[1],1);
		dup2(childToParent[1],2);
		close(parentToChild[0]);
		close(childToParent[1]);
		char* arg[2];
		arg[0] = "/bin/bash";
		arg[1] = NULL;
		if(execvp(arg[0],arg))
		{
			printError("Fail to transform child process.\n");
			exit(1);
		}
		
	}
	else if (pid == -1){
		printError("Fail to create new process.\n");
		exit(1);
	}
	else{
		close(parentToChild[0]);
		close(childToParent[1]);
		if( inToOutWithShell(newsockfd,newsockfd,parentToChild[1],childToParent[0]) == 1){
			exit(1);
		}
		
	}
	
	exit(0);
}