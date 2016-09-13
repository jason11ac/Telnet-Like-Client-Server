#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <getopt.h>
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>
#include <string.h>
#include <mcrypt.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

//Global variables
int rc;  //Process ID of child process from fork
static int encrypt = 0; //Set to 1 if --encrypt flag is given
MCRYPT td; //MCRYPT descriptor for encrypting
MCRYPT td2; //MCRYPT descriptor for decrypting
char* IV; //IV for encrypting and decrypting
int keysize = 32; //Size of key used for encrypting
char* key; //The key, which reads from my.key


int encryption() //Encryption function for --encrypt
{
  int i, j;
  int o; //Open file descriptor
  int r; //Read file descriptor

  //Allocate array for 32 byte key
  key = (char*) calloc(keysize, sizeof(char));
  
  //my.key file holds 32 byte key for encryption
  char* file = "/u/eng/ugrad/alvarezc/my.key";
  if ((o = open(file, O_RDONLY)) == -1)
    {
      perror("Error opening my.key file");
      exit(10);
    }
  if ((r = read(o, key, keysize)) == -1)
    {
      perror("Error reading my.key file");
      exit(10);
    }

  //Open module for encrypting. Using twofish encryption
  td = mcrypt_module_open("twofish", NULL, "cfb", NULL);

  //Open module for decrypting 
  td2 = mcrypt_module_open("twofish", NULL, "cfb", NULL);
  
  if (td == MCRYPT_FAILED)
    return 1;
  if (td2 == MCRYPT_FAILED)
    return 1;

  //Allocate space/memory for the IV
  IV = malloc(mcrypt_enc_get_iv_size(td));

  //Create IV
  for (i = 0; i < mcrypt_enc_get_iv_size(td); i++)
    IV[i] = rand();

  //Initialize mcrypt_generic function for encrypting 
  i = mcrypt_generic_init(td, key, keysize, IV);
  if (i < 0)
    {
      mcrypt_perror(i);
      return 1;
    }

  //Initialize mcrypt_generic function for decrpyting
  j = mcrypt_generic_init(td2, key, keysize, IV);
  if (j < 0)
    {
      mcrypt_perror(j);
      return 1;
    }
  
  return 0; //Return 0 on success. Return 1 if error occurs
}

//Thread reads from shell and writes to socket
void* thread_function(void* fd)
{
  char t;
  char buf;

  int* input = (int*) fd;

  while ((read (*input, &t, 1)) > 0)
    {
      if (encrypt) //If --encrypt
	mcrypt_generic (td, &t, 1); //Encrypt t

      write (1, &t, 1);
    }

  if (kill(rc, SIGHUP) == -1)
    {
      perror("Error sending SIGHUP to shell");
      exit(1);
    }
  
  mcrypt_generic_deinit(td); //Deinitialize the encrypting fd
  mcrypt_module_close(td); //Close the module

  mcrypt_generic_deinit(td);
  mcrypt_module_close(td);
  
  exit(2); //If read returns 0, that means EOF from shell
}

void sphandler(int sig)
{
  exit(2); //Exit with a return code of 2
}


int main(int argc, char* argv[])
{
  
  int sockfd, new_sfd, port_num, clilen, n;

  int c; //For getopt
  int e; //For --encrypt option
  
  struct sockaddr_in serv_addr, cli_addr;
  
  while(1)
    {
            static struct option long_options[] =
	      {
		{"port",  required_argument, NULL, 'p'},
		{"log", required_argument, NULL, 'l'},
		{"encrypt", no_argument, &encrypt, 1},
		{0, 0, 0, 0}
	      };

	    int option_index = 0;
	    c = getopt_long(argc, argv, "p:l:", long_options, &option_index);

	    if (optopt)
	      exit(1);

	    if (c == -1)
	      break;

	    switch(c)
	      {
	      case 'p':
		port_num = atoi(optarg);
		if (!port_num)
		  {
		    perror("Error, No port provided");
		    exit(10);
		  }
		break;
	      case 'l':
		break;
	      case 0:  //c is 0 when flag is set
		if ((e = encryption()) != 0)
		  {
		    perror("Error in encrypting");
		    exit(10);
		  }
		encrypt = 1;
		break;
	      default:
		break;
	      }
    }

  if (argc < 2)
    {
      perror("Error, no port provided");
      exit(10);
    }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    {
      perror("Error opening socket");
      exit(10);
    }

  memset((char*) &serv_addr, '\0', sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port_num);
  if (bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
    {
      perror("Error on binding");
      exit(10);
    }

  listen(sockfd, 1);
  clilen = sizeof(cli_addr);
  new_sfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
  if (new_sfd < 0)
    {
      perror("Error on accepting");
      exit(10);
    }

  close(sockfd); //Close the sockfd, no longer needed

  int pipe_from_shell[2]; //Pipe from shell
  int pipe_to_shell[2]; //Pipe to shell

  if (pipe(pipe_from_shell) == -1)
    {
      perror("Error piping from shell");
      exit(1);
    }

  if (pipe(pipe_to_shell) == -1)
    {
      perror("Error piping to shell");
      exit(1);
    }

  pthread_t thread1; //Create thread for pthread_create below 
  
  rc = fork(); //Fork processes into child and parent
  
  if (rc == 0) //Child process
    {
      
      //Shell does not read from itself
      close(pipe_from_shell[0]);

      //Set writing from shell as stdout
      dup2(pipe_from_shell[1], 1);

      //Set writing from shell as stderr in case of error
      dup2(pipe_from_shell[1], 2);

      //Shell does not write to itself
      close(pipe_to_shell[1]);

      //Set pipe to shell input as stdin
      dup2(pipe_to_shell[0], 0);
      
      //Set up the bash shell
      execl("/bin/bash", "/bin/bash", NULL); 
    }
  else  //Parent process
    {

      signal(SIGPIPE, sphandler); //If EOF from the shell

      //Server does not write from shell
      close(pipe_from_shell[1]); 

      //Server does not read from itself
      close(pipe_to_shell[0]);
      
      //Redirect server stdin to socket
      dup2(new_sfd, 0);

      //Redirect server stdout to socket 
      dup2(new_sfd, 1);

      //Redirect server stderr to socket
      dup2(new_sfd, 2);

      //Close new_sfd as we no longer need it
      close(new_sfd);

      //Create a thread that reads from shell and writes to socket
      int tret = pthread_create(&thread1, NULL, &thread_function, (void*) &pipe_from_shell[0]);
      if (tret)
	{
	  perror("Error creating thread");
	  exit(10);
	}

      int n;
      char buffer;

      //Read from socket. new_sfd is now stdin (0)
      while((n = read(0, &buffer, 1)) > 0)
	{
	  if (encrypt) //If --encrypt
	    mdecrypt_generic (td2, &buffer, 1); //Decrypt buffer
	     
	  write (pipe_to_shell[1], &buffer, 1); //Write to the shell
	}

      if (n == 0 || n == -1) //Error reading or EOF
	{
	  pthread_cancel(thread1); //Cancel the other thread
	  if (pthread_join(thread1, NULL)) //Join just in case
	      perror("Error joining threads");
	  if (kill(rc, SIGHUP) == -1) //Kill the shell
	    {
	      perror("Error sending SIGHUP to the shell");
	      exit(1);
	    }
	  exit(1);
	}
    }

  exit(0);
}
