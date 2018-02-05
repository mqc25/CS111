#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>

void printUsage(char* progname){
  fprintf(stdout, "Usage: %s [sc] -i input_file -o output_file\n", progname);
}

void aToB(FILE* a,FILE* b){
  
  char* buff = (char*)malloc(4096*sizeof(char)); // 4kB buffer
  int readnum = 0;
  while(1){
    if( (readnum = fread((void *) buff, sizeof(char), 4096, a)) < 4096){
      if(feof(a) != 0){
	if ( fwrite((void*) buff, 1,readnum ,b) != readnum){
	  int err = errno;
	  fprintf(stderr,"Cannot write probably to standard output. %s\n", strerror(err)); // error writing
	  exit(1);
	}
	exit(0); // job done - end of file
      }
      else{
	int err = errno;
	fprintf(stderr,"Cannot read from standard input. %s\n", strerror(err)); // error reading
	exit(1);
      }
    }   
   }
  free(buff);
}

int inputRedirection(char* filename){
  FILE* fs = fopen(filename,"r");
  if(fs != NULL){
    int save = dup(0);
    dup2(fileno(fs),0);
    close(fileno(fs));
    return save;
  }
  else{
    int err = errno;
    fprintf(stderr,"Error: File name argument of --input. File %s cannot be open. %s\n", filename, strerror(err));
    exit(2);
  }
}

int outputRedirection(char* filename){
  FILE *fs = fopen(filename,"w");
  if(fs != NULL){
    int save = dup(1);
    dup2(fileno(fs),1);
    close(fileno(fs));
    return save;
  }
  else{
    int err = errno;
    fprintf(stderr,"Error: File argument of --output. File %s cannot be created or opened. %s\n",filename, strerror(err));
    exit(3);
  }
}


void signal_handler(int signal_num)
{
  if(signal_num == SIGSEGV)
    {
      char* err = strerror(errno);
      char* msg = ". Able to catch SIGSEGV signal.\n";
      write(2,err,strlen(err));
      write(2,msg,strlen(msg));
      exit(4);
    }
}

int main(int argc,char **argv){

  static struct option long_opts[] = {
    {"input", required_argument, NULL, 'i'},
    {"output", required_argument, NULL, 'o'},
    {"segfault", no_argument, NULL, 's'},
    {"catch", no_argument, NULL, 'c'},
    {0, 0, 0, 0}
  };
  
  _Bool segFlag = 0;
  char* in = NULL;
  char* out = NULL;
  int opt;
  int err;
  while( (opt = getopt_long(argc, argv, "i:o:sc", long_opts, NULL)) != -1){
    switch(opt){
      case 'i':
	in = optarg;
	break;
      case 'o':
	out = optarg;
	break;
      case 's':
	segFlag = 1;
	break;
      case 'c':
	signal(SIGSEGV, signal_handler);
	break;
      default:
	err = errno;
	fprintf(stderr,"Invalid input argument. %s\n", strerror(err));
	printUsage(argv[0]);
	exit(1);

    }
  }
  if(segFlag == 1)
    {
      char *crash = NULL;
      opt = *crash;
    }
  int restoreIn = -1;
  int restoreOut = -1;
  if(in != NULL){
    restoreIn = inputRedirection(in); // redirect input file to stdin
  }
  if(out != NULL){
    restoreOut = outputRedirection(out); // redirect output file to stdout
  }
  aToB(stdin,stdout); //copy stdin to stdout
  
  if(in != NULL){
    dup2(restoreIn,0); // restore stdin
  }
  if( out != NULL){
    dup2(restoreOut,1); // restore stdout
  }
  return 0;
}
