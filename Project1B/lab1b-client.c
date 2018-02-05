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
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include "zlib.h"
#define SIZE 4096

unsigned char* buffer;
unsigned char* compBuf;
unsigned char* msg0;
unsigned char* msg1;
struct termios saved_attributes;
int pid = -1;
FILE* file = NULL;
int compFlag;
z_stream toShell;
z_stream toOut;


void printError(char* a){
	int err = errno;
	fprintf(stderr, strerror(err));
	fprintf(stderr, a);
}

void signalHandler(int sig){
	if(sig == SIGPIPE){
		exit(0);
	}
}

void
reset_input_mode (void)
{
	if(file!= NULL){
		fclose(file);
	}
	
	free(buffer);
	//free(msg0);
	//free(msg1);
	
	tcsetattr (STDIN_FILENO, TCSANOW, &saved_attributes);
  
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
	
	
}

void
set_input_mode (void)
{
  struct termios tattr;

  tcgetattr (STDIN_FILENO, &saved_attributes);
  atexit (reset_input_mode);

  tcgetattr (STDIN_FILENO, &tattr);
  tattr.c_iflag = ISTRIP; 
  tattr.c_oflag = 0;
  tattr.c_lflag = 0;
  tattr.c_cc[VMIN] = 1;
  tattr.c_cc[VTIME] = 0;
  tcsetattr (STDIN_FILENO, TCSANOW, &tattr);
}

int zp(int num, unsigned char* buff){
	toShell.zalloc = Z_NULL;
	toShell.zfree = Z_NULL;
	toShell.opaque = Z_NULL;
	
	if(deflateInit(&toShell, Z_DEFAULT_COMPRESSION) != Z_OK){
		fprintf(stderr, "Fail to initialize deflate.\n");
		exit(1);
	}
	
	toShell.avail_in = num;
	toShell.next_in = buff;
	toShell.avail_out = SIZE;
	toShell.next_out = compBuf;

	do{
		deflate(&toShell, Z_SYNC_FLUSH);
	}	while(toShell.avail_in > 0);
	num = SIZE - toShell.avail_out;
	
	return num;
}

int unzp(int num, unsigned char* buff){
	toOut.zalloc = Z_NULL;
	toOut.zfree = Z_NULL;
	toOut.opaque = Z_NULL;
		
	if( inflateInit(&toOut) != Z_OK){
			fprintf(stderr, "Fail to initialize inflate.\n");
			exit(1);
		}
	
	toOut.avail_in = num;
	toOut.next_in = buff;
	toOut.avail_out = SIZE;
	toOut.next_out = compBuf;

	do{
		inflate(&toOut, Z_SYNC_FLUSH);
	} while(toOut.avail_in > 0);
	
	num = SIZE - toOut.avail_out;
	
	return num;
}
	


int inToOut(int in, int out){
	int num = read(in, buffer, SIZE);

	if (num == -1){
		if(in == 0){
			printError("Fail to read data from stdin.\n");
			exit(1);
		}
		else{
			printError("Fail to read data from socket.\n");
			exit(1);
		}
	}
	else if(num == 0){
		return -1;
	}
	
	if (in!=0 && file!= NULL){
			char send[128];
			char *nl = "\n";
			sprintf(send,"RECEIVED %d bytes: ",num);
			if(!(fwrite(send,1,strlen(send),file) && fwrite(buffer,1,num,file) && fwrite(nl,1,strlen(nl),file))){
				printError("Fail to write sending data to log.\n");
			}
			
	}
	
	int i = 0;
	int r = 0;
	int s = 0;
	char temp ='0';
	if(!in){
		for (i = 0; i < num; i ++){
			temp = *(buffer+i);
			if( temp == '\r' || temp == '\n' ){
				*((msg1) + r++) = '\r';
				*((msg1) + r++) = '\n';
				*((msg0) + s++) = '\n';
			}
			else{
				*(msg0 + r++) = temp;
				*(msg1 + s++) = temp;
			}
		}
	}
	else{
		if(compFlag){
			num = unzp(num, buffer);
			memcpy(buffer,compBuf,num);
			inflateEnd(&toOut);
		}
		for (i = 0; i < num; i ++){
			temp = *(buffer+i);
			if( temp == '\n' ){
				*((msg1) + r++) = '\n';
				*((msg0) + s++) = '\r';
				*((msg0) + s++) = '\n';
				num++;
			}
			else{
				*(msg0 + s++) = temp;
				*(msg1 + r++) = temp;
			}
		}
	}
	
	if (in == 0 && write(1, msg1, r) < r ){
			printError("Fail to write data to stdout.\n");
			exit(1);
	}
		
	if (compFlag && !in){
		s = zp(num, msg0);
		memcpy(msg0,compBuf,s);
		deflateEnd(&toShell);
	}
	
		
	if (write(out, msg0, s) < s ){
		if(in == 0){
			printError("Fail to write data to socket.\n");
			exit(1);
		}
		else{
			printError("Fail to write data to stdout.\n");
			exit(1);
		}
	}

	if(in == 0){
		
		if (file!= NULL){
			char send[128];
			char *nl = "\n";
			sprintf(send,"SENT %d bytes: ",s);
			if(!(fwrite(send,1,strlen(send),file) && fwrite(msg0,1,s,file) && fwrite(nl,1,strlen(nl),file))){
				printError("Fail to write sendind data to log.\n");
			}
		}
	}

  
	return 0;
}


int main(int argc, char *argv[]){
	
	int opt;
	static struct option long_opts[] = {
		{"log", required_argument, NULL, 'l'},
		{"compress", no_argument, NULL, 'c'},
		{"port", required_argument, NULL, 'p'},
		{"host", required_argument, NULL, 'h'},
		{0, 0, 0, 0}
	};
	compFlag = 0;
	int portno = -1;
	char* hostSession = (char*)malloc(SIZE);
	hostSession = strcpy(hostSession,"localhost");
	
	while( (opt = getopt_long(argc, argv, "l:cp:h:", long_opts, NULL)) != -1){
		switch(opt){
		case 'l':
			if((file = fopen(optarg,"w")) == NULL){
				printError("Fail to open log file.\n");
				exit(1);
			}
			break;
		case 'c':
			compFlag = 1;
			break;
		case 'h':
			hostSession = strcpy(hostSession,optarg);
			break;
		case 'p':
			portno = atoi(optarg);
			if(portno <= 1024){
				fprintf(stderr,"Port number must be larger than 1024.\n");
				exit(1);
			}
			break;
		case '?':
			fprintf(stderr, "Wrong argument format. Usage: %s --port=portNumber [-l=logFileName -c] \n",argv[0]);
			exit(1);
		default:
			break;
		}
	}
	if (portno == 0){
		fprintf(stderr, "Wrong argument format. Usage: %s --port=portNumber [-l=logFileName -c] \n",argv[0]);
		exit(1);
	}
	if(compFlag){
		compBuf = malloc(SIZE);
	}
	
	buffer = (unsigned char*) malloc(SIZE);
	msg0 = (unsigned char*) malloc(SIZE);
	msg1 = (unsigned char*) malloc(SIZE);
	int sockfd;
	struct sockaddr_in serv_addr;
	struct hostent *server;

	/* Create a socket point */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
	  perror("ERROR opening socket");
	  exit(1);
	}
	
	server = gethostbyname(hostSession);
	free(hostSession);
	if (server == NULL) {
	  fprintf(stderr,"ERROR, no such host\n");
	  exit(1);
	}

	memset((char *) &serv_addr ,0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(portno);

	/* Now connect to the server */
	if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
	  perror("ERROR connecting");
	  exit(1);
	}
	struct pollfd checkInput[2];
	
	checkInput[0].fd = 0;
	checkInput[1].fd = sockfd;
	
	checkInput[0].events = POLLIN | POLLHUP | POLLERR;
	checkInput[1].events = POLLIN | POLLHUP | POLLERR;

	
	set_input_mode();
	
	while(1){
		int pollReturn = poll(checkInput,2,0);
		if (pollReturn == -1){
			printError("poll() fail.\n");
			exit(1);
		}
		else if(checkInput[1].revents & (POLLHUP | POLLERR)){
			exit(0);
		}
		else if (pollReturn == 0) continue;
		
		if(checkInput[0].revents & POLLIN){
			int num = inToOut(0,sockfd);
			if( num == 1){
				printError("Reading from terminal fail.\n");
				exit(1);
			}
			
		}
		
		if(checkInput[1].revents & POLLIN){
			int num = inToOut(sockfd,1);
			if( num == 1){
				printError("Reading from socket fail.\n");
				exit(1);
			}
			if( num == -1){
				exit(0);
			}
		}
		
	}
	
	exit(0);
}

