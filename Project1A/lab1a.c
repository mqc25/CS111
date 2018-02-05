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
#define SIZE 256

struct termios saved_attributes;
char* buffer;
char* msg[2];
int pid = -1;
int shellMode= 0;

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

  free(buffer);
  free(msg[0]);
  free(msg[1]);
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


int inToOut(int in, int out, int shell){
	int num;
	int done = 0;
	num = read(in, buffer, SIZE);
	if (num == -1){
		printError("Fail to read data.\n");
		exit(1);
	}
	
	else if (num == 0){
		return 0;
	}
	
	int i = 0;
	int r = 0;
	int s = 0;
	for (i = 0; i < num; i ++){
		char temp = *(buffer+i);
		if( temp == '\r' || temp == '\n' ){
			*((msg[0]) + r++) = '\r';
			*((msg[0]) + r++) = '\n';
			*((msg[1]) + s++) = '\n';
		}
		else if (shellMode && temp == 0x03){
			kill(pid,SIGINT);
		}
		else if( temp == 0x04 ) {
			done = 1;
			break;
		}
		else{
			*(msg[0] + r++) = temp;
			*(msg[1] + s++) = temp;
		}
	}

	if (write(out, msg[0], r) < r ){
		printError("Fail to write to stdout correctly.\n");
		exit(1);
	}
	if (shellMode && write(shell,msg[1],s) < s){
		printError("Fail to write to shell correctly.\n");
		exit(1);
	}
	if(done == 1 ){
		return 2;
	}
  
	return 0;
}

int shellToOut(int in, int out){
	int num;
	int done = 0;
	num = read(in, buffer, SIZE);
	if (num == -1){
		printError("Fail to read from shell.\n");
		exit(1);
	}
	else if (num == 0){
		return 0;
	}
	int i = 0;
	int r = 0;
	for (i = 0; i < num; i ++){
		char temp = *(buffer+i);
		if( temp == '\n' ){
			*((msg[0]) + r++) = '\r';
			*((msg[0]) + r++) = '\n';
			num++;
		}
		else if(temp == 0x04){
			done = 2;
			break;
		}
		else{
			*(msg[0] + r++) = temp;
		}
	}

	if (write(out, msg[0], num) < num ){
		printError("Fail to write to stdout correctly.\n");
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
		else if (pollReturn == 0) continue;
		
		if(checkInput[0].revents & POLLIN){
			int num = inToOut(terminalIn,terminalOut,shellIn);
			if( num == 1){
				printError("Reading from terminal fail.\n");
				exit(1);
			}
			else if (num == 2){
				close(shellIn);
			}
		}
		
		if(checkInput[1].revents & POLLIN){
			int num = shellToOut(shellOut,terminalOut);
			if( num == 1){
				printError("Reading from shell to stdout fail.\n");
				exit(1);
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
	buffer = (char*)malloc(SIZE * sizeof(char));
	msg[0] = (char*)malloc(2*SIZE*sizeof(char));
	msg[1] = (char*)malloc(SIZE*sizeof(char));
	
	int opt;
	int childToParent[2];
	int parentToChild[2];
	static struct option long_opts[] = {
		{"shell", no_argument, NULL, 's'},
		{0, 0, 0, 0}
	};

	while( (opt = getopt_long(argc, argv, "s", long_opts, NULL)) != -1){
		switch(opt){
		case 's':
			shellMode = 1;
			signal(SIGINT,signalHandler);
			signal(SIGPIPE,signalHandler);
			break;
		case '?':
			fprintf(stderr, "Wrong argument format. Usage: %s [-s] \n",argv[0]);
			exit(1);
		default:
			break;
		}
	}
	set_input_mode();
	if(shellMode){
		if( pipe(childToParent) || pipe(parentToChild)){
			printError("Fail to create pipe properly.\n");
			exit(1);
		}
		pid = fork();
		
		if (pid == 0){   // child process only need 1 parentToChild, 0 childToParent
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
			if( inToOutWithShell(0,1,parentToChild[1],childToParent[0]) == 1){
				exit(1);
			}
			
		}
	}
	else { 
		while(inToOut(0,1,-1)==0); 
	}
	return 0;
}

