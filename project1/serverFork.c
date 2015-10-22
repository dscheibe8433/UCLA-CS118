/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <sys/stat.h>


void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void dostuff(int); /* function prototype */
void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, pid;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;
     struct sigaction sa;          // for signal SIGCHLD

     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
     listen(sockfd,5);
     
     clilen = sizeof(cli_addr);
     
     /****** Kill Zombie Processes ******/
     sa.sa_handler = sigchld_handler; // reap all dead processes
     sigemptyset(&sa.sa_mask);
     sa.sa_flags = SA_RESTART;
     if (sigaction(SIGCHLD, &sa, NULL) == -1) {
         perror("sigaction");
         exit(1);
     }
     /*********************************/
     
     while (1) {
         newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
         
         if (newsockfd < 0) 
             error("ERROR on accept");
         
         pid = fork(); //create a new process
         if (pid < 0)
             error("ERROR on fork");
         
         if (pid == 0)  { // fork() returns a value of 0 to the child process
             close(sockfd);
             dostuff(newsockfd);
             exit(0);
         }
         else //returns the process ID of the child process to the parent
             close(newsockfd); // parent doesn't need this 
     } /* end of while */
     return 0; /* we never get here */
}

int is_valid_file(const char *filename)
{
  FILE *fp = fopen (filename, "r");
  if (fp!=NULL) {
    fclose(fp);
    return 1;
  }
  else {
    fclose(fp);
    return 0;
  }
}

int get_filesize(char* filename) {
  if (!(is_valid_file(filename)))
    return -1;
  FILE* f = fopen(filename, "r");
  fseek(f, 0L, SEEK_END);
  int filesize = (int) ftell(f);
  fseek(f, 0L, SEEK_SET);
  fclose(f);
  return filesize;
}

char* parse_request(char* req) {
  if (strlen(req) < 6 || req[0] != 'G' || req[1] != 'E' || req[2] != 'T' || req[3] != ' ' || req[4] != '/')
    return NULL;
  int i = 5;
  int size = 3;
  while(req[i] != '\0' && req[i] != ' ') {
    size++;
    i++;
  }
  char* ret_str;
  if (size == 3) {
    ret_str = (char *) malloc(sizeof(char) * 13);
    ret_str = "./index.html";
  }
  else {
    ret_str = (char *) malloc(sizeof(char) * size);
    i = 5;
    ret_str[0] = '.';
    ret_str[1] = '/';
    while(req[i] != '\0' && req[i] != ' ') {
      ret_str[i-3] = req[i];
      i++;
    }
    ret_str[size] = '\0';
}
  printf("File: %s", ret_str);
  return ret_str;
}

/******** DOSTUFF() *********************
 There is a separate instance of this function 
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/
void dostuff (int sock)
{
  int n;
  char buffer[256];
      
  bzero(buffer,256);
  n = read(sock,buffer,255);
  if (n < 0) error("ERROR reading from socket");

  char* filename = parse_request(buffer);

  printf("Here is the message: %s\n",buffer);
  
  int f_size = get_filesize(filename);
  if (f_size == -1) {
    printf("Could not find file %s\n", filename);
    write(sock, "HTTP/1.1 404 Not Found\n", 23);
    write(sock, "Connection: close\n\n", 19);
    error("ERROR: Could not find file");
  }
  n = write(sock, "HTTP/1.1 200 OK\n", 16);
  if (n < 0) error("ERROR writing to socket");

  printf("Transfering file %s with length %d\n", filename, f_size);

  FILE* f = fopen(filename, "r");
  char* file_data = (char *) malloc(sizeof(char) * f_size);
  fread(file_data, 1, f_size, f);

  n = write(sock, "Connection: close\n\n", 19);
  if (n < 0) error("ERROR writing to socket");

  n = write(sock, file_data, f_size);
  if (n < 0) error("ERROR writing to socket");

  printf("Done transfering file\n");
  
  free(file_data);
  fclose(f);
  return;
}
