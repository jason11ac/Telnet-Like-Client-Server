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
int rc; 
static int encrypt = 0; //Set to 1 if encryption flag is given as argument
int log_arg; //Set to 1 if log is given as argument
int fd; //File descriptor for log file
MCRYPT td; //MCRYPT descriptor for encrypting
MCRYPT td2; //MCRYPT descriptor for decrypting
char* IV; //IV for encrypting and decrypting
int keysize = 32; //Size of key used for encrypting
char* key; //The key, which reads from my.key

struct termios saved_attributes; //Terminal saved attributes

int encryption();

int connection(int port_num)
{
  char c;
  int sockfd, n;

  struct sockaddr_in serv_addr;
  struct hostent *server;

  sockfd = socket(AF_INET, SOCK_STREAM, 0); //Set up socket fd
  if (sockfd < 0)
    {
      perror("Error opening socket");
      return 1;
    }
  server = gethostbyname("localhost"); //Localhost server
  if (server == NULL)
    {
      perror("Error, no such hosts");
      return 1;
    }

  memset((void*) &serv_addr, '\0', sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  memcpy((void*) &serv_addr.sin_addr.s_addr, (void*) server->h_addr, server->h_length);
  serv_addr.sin_port = htons(port_num);

  //Connecting function
  if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
    {
      perror("Error connecting to socket");
      return 1;
    }

  return sockfd; //Return socket fd to main. Return 1 if error
  
}

void reset_input_mode (void)
{
  mcrypt_generic_deinit(td); //Deinitialize the encrypting fd
  mcrypt_module_close(td); //Close the module
  
  mcrypt_generic_deinit(td2);
  mcrypt_module_close(td2);
  
  tcsetattr (STDIN_FILENO, TCSANOW, &saved_attributes);
}


void set_input_mode (void)
{
  struct termios tattr;

  //Make sure stdin is a terminal
  if (!isatty (STDIN_FILENO))
    {
      fprintf (stderr, "Not a normal terminal.\n");
      exit (1);
    }

  // Save the terminal attributes so we can restore them later
  tcgetattr (STDIN_FILENO, &saved_attributes);
  atexit (reset_input_mode); //Automatically resets the terminal to default at exit

  // Set the new terminal mode
  tcgetattr (STDIN_FILENO, &tattr);
  tattr.c_lflag &= ~(ICANON|ECHO); //Clear ICANON and ECHO
  tattr.c_cc[VMIN] = 1;
  tattr.c_cc[VTIME] = 0;
  tcsetattr (STDIN_FILENO, TCSAFLUSH, &tattr);
}


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

  //Initiate mcrypt_generic function for encrypting
  i = mcrypt_generic_init(td, key, keysize, IV);
  if (i < 0)
    {
      mcrypt_perror(i);
      return 1;
    }

  //Initiate mcrypt_generic function for decrypting
  j = mcrypt_generic_init(td2, key, keysize, IV);
  if (j < 0)
    {
      mcrypt_perror(j);
      return 1;
    }
  
  return 0; //Return 0 on success. Return 1 if error occurs
}

//Thread reads from socket and writes to screen
void* thread_function(void* sockfd)
{
  char t; 
  char buf_enc; //encrypted buffer
  int* buffer = (int*) sockfd;

  char* recieved = "RECIEVED 1 bytes: "; //For the log
  int rec_len = 18; //Length of the string above
  
  while ((read(*buffer, &t, 1)) > 0)
    {
      buf_enc = t; //Log should show pre-decrypted bytes
      
      if (encrypt) //If --encrypt
	mdecrypt_generic (td2, &t, 1); //Decrypt t

        if (log_arg == 1) //If log argument is given
	{   
	  write(fd, recieved, rec_len); //Prefix formatting
	  write(fd, &buf_enc, 1); //Write the single recieved byte
	  write(fd, "\n", 1); //Write a newline for spacing  
	}
	
      write (1, &t, 1); //Socket to screen/stdout
    }

  exit(1); //If read returns 0, that means EOF from the server
}


//Main function
int main(int argc, char* argv[])
{
  int c, e, sockfd, portnum, input;
  
  set_input_mode (); 

  while(1) //Getopt for options
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

      if (optopt) //If no argument is given for port or log
	exit(1);

      if (c == -1) //No options given
	break;

      int pn; //used for port number
      
      switch(c)
	{
	case 'p': //port argument
	  pn = atoi(optarg); //Convert to integer
	  sockfd = connection(pn); //Run connection function
	  if (sockfd == 1) //Error in connection function
	    exit(10);
	  break;
	case 'l': //log argument
	  //Open the file provided
	  fd = open(optarg, O_CREAT|O_TRUNC|O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	  if (fd == -1) //Error with open
	    {
	      perror("Error opening file");
	      exit(10);
	    }
	  log_arg = 1;
	  break;
	case 0:  //c is 0 when encryption flag is given
	  if ((e = encryption()) != 0)
	    {
	      perror("Error in encryption function");
	      exit(10);
	    }
	  encrypt = 1;
	  break;
	default:
	  break;
	}
    }

  pthread_t thread1; //For pthread_create
  int m, n;

  //Create a thread that reads input from socket to stdout/display
  int tret = pthread_create(&thread1, NULL, &thread_function, (void*) &sockfd);
  if (tret)
    {
      perror("Error with creating thread");
      exit(10);
    }

  char mapped_char[2];
  mapped_char[0] = '\r'; //<cr>
  mapped_char[1] = '\n'; //<lf>
  
  char* sent = "SENT 1 bytes: "; //For the log
  int sent_len = 14; //Length of the string above

  char buf_dec; //Write to display should be readable
  
  while(read(0, &input, 1) > 0)
    {
      buf_dec = input; //Save pre-encrypted input
      
      if (encrypt) //if --encrypt
	mcrypt_generic (td, &input, 1); //Encrypt input
      
      if (input == '\004')
	{
	  close(sockfd); //Close the network connection
	  pthread_cancel(thread1); //Cancel the socket thread
	  if (pthread_join(thread1, NULL)) //Join just in case 
	      perror("Error joining threads");
	  exit(0); //Exit with a return code of 0
	}
      else if ((input == '\r') || (input == '\n')) //If <cr> or <lf>
	{
	  n = write(1, &mapped_char, 2); //Map and echo as <cr><lf>
	  m = write(sockfd, &mapped_char[1], 1); //Go to socket as just <lf>
	}
      else
	{
	  n = write(sockfd, &input, 1); //Writes to socket
	  m = write(1, &buf_dec, 1); //Writes to display/terminal
	  //Should be readable, pre-encrypted
	}

      //Write error checking
      if (n < 0)
	{
	  perror("Error writing to socket");
	  exit(10);
	}
      if (m < 0)
	{
	  perror("Error writing to terminal");
	  exit(10);
	}
      
      if (log_arg == 1) //Write to a log
	{
	  write(fd, sent, sent_len); //SENT prefix
	  if ((input == '\r') || (input == '\n')) //If <cr> or <lf>
	    write(fd, &mapped_char[1], 1);
	  else
	    write(fd, &input, 1);
	  
	  write(fd, "\n", 1);
	}
    }
  
  exit(0);
}




  
