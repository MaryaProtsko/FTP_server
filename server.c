#include "common.h"
#define SA struct sockaddr

void* thread_function(void* con)
{
    int connection = *(int*)con;
    int bytes_read;
    char buffer[BSIZE];
    Command *cmd = malloc(sizeof(Command));
    State *state = malloc(sizeof(State));
    int pid = fork();
    
    memset(buffer,0,BSIZE);

    if(pid<0){
      fprintf(stderr, "Cannot create child process.");
      exit(EXIT_FAILURE);
    }

    if(pid==0){
      char welcome[BSIZE] = "220 ";
      if(strlen(welcome_message)<BSIZE-4){
        strcat(welcome,welcome_message);
      }else{
        strcat(welcome, "Welcome to nice FTP service.");
      }

      /* Write welcome message */
      strcat(welcome,"\n");
      write(connection, welcome,strlen(welcome));

      /* Read commands from client */
      while (bytes_read = read(connection,buffer,BSIZE)){
        
        signal(SIGCHLD,my_wait);

        if(!(bytes_read>BSIZE)){
          /* TODO: output this to log */
          buffer[BSIZE-1] = '\0';
          printf("User %s sent command: %s\n",(state->username==0)?"unknown":state->username,buffer);
          parse_command(buffer,cmd);
          state->connection = connection;
          
          /* Ignore non-ascii char. Ignores telnet command */
          if(buffer[0]<=127 || buffer[0]>=0){
            response(cmd,state);
          }
          memset(buffer,0,BSIZE);
          memset(cmd,0,sizeof(cmd));
        }else{
          /* Read error */
          perror("server:read");
        }
      }
      printf("Client disconnected.\n");
      exit(0);
    }
}

/** 
 * Sets up server and handles incoming connections
 * @param port Server port
 */
void server(int port)
{
  int opt = 1;  
  int master_socket , addrlen , new_socket , client_socket[30]={0}, 
          max_clients = 30 , activity, i , valread , sd;  
  int max_sd;  
  struct sockaddr_in address;  
  //set of socket descriptors 
  fd_set readfds,fde; 

  master_socket = create_socket(port);

  while(1){
    //clear the socket set 
        FD_ZERO(&readfds); 
        FD_ZERO(&fde);
    //add master socket to set 
        FD_SET(master_socket, &readfds); 
        max_sd = master_socket; 
    //add child sockets to set 
        for ( i = 0 ; i < max_clients ; i++)  
        {  
            //socket descriptor 
            sd = client_socket[i];  
                 
            //if valid socket descriptor then add to read list 
            if(sd > 0)  {
                FD_SET( sd , &readfds);  
                FD_SET(sd, &fde); //if it disconnects 
            }                 
            //highest file descriptor number, need it for the select function 
            if(sd > max_sd)  
                max_sd = sd;  
        } 

  //wait for an activity on one of the sockets , timeout is NULL , 
  //so wait indefinitely 
        activity = select( max_sd + 1 , &readfds , 0 , &fde , 0);  
       
        if ((activity < 0) && (errno!=EINTR))  
        {  
            printf("select error");  
        }  
             
  //If something happened on the master socket , 
  //then its an incoming connection 
        if (FD_ISSET(master_socket, &readfds))  
        {  
            if ((new_socket = accept(master_socket, 
                    (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)  
            {  
                perror("accept");  
                exit(EXIT_FAILURE);  
            }  

  //inform user of socket number - used in send and receive commands 
            printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));  
  // новый поток для new_socket
      pthread_t thread;
      pthread_create(&thread,NULL,thread_function,(void*)&new_socket);
        
  //add new socket to array of sockets 
            for (i = 0; i < max_clients; i++)  
            { 
                if( client_socket[i] == 0 )  
                {  
                    client_socket[i] = new_socket;  
                    printf("Adding to list of sockets as %d\n" , i);  
                         
                    break;  
                }  
            }  
        }
        
 //обработчик отключившихся 
 for (i = 0; i < max_clients; i++)  
  {  
    sd = client_socket[i];  
    if (FD_ISSET(sd, &fde))
    {
      getpeername(sd , (struct sockaddr*)&address , \
                        (socklen_t*)&addrlen);  
      printf("Host disconnected , ip %s , port %d \n" , 
                          inet_ntoa(address.sin_addr) , ntohs(address.sin_port));  
      client_socket[i] = 0;                
    }              
  }  

  }
}

/**
 * Creates socket on specified port and starts listening to this socket
 * @param port Listen on this port
 * @return int File descriptor for new socket
 */
int create_socket_act(int port,char* str)
{
  int sock;
  int reuse = 1;

  /* Server addess */
  struct sockaddr_in server_address = (struct sockaddr_in){  
     AF_INET,
     htons(port),
     inet_addr(str)
  };

  if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    fprintf(stderr, "Cannot open socket");
    exit(EXIT_FAILURE);
  }

  /* Address can be reused instantly after program exits */
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);

   if (connect(sock, (SA*)&server_address, sizeof(server_address)) != 0) {
        printf("connection with the client failed...\n");
        exit(EXIT_FAILURE);
    }
    else
        printf("connected to the client\n");

  return sock;
}

//for the master socet only
int create_socket(int port)
{
  int sock;
  int reuse = 1;

  /* Server addess */
  struct sockaddr_in server_address = (struct sockaddr_in){  
     AF_INET,
     htons(port),
     (struct in_addr){INADDR_ANY}
  };


  if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    fprintf(stderr, "Cannot open socket");
    exit(EXIT_FAILURE);
  }

  /* Address can be reused instantly after program exits */
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);

  /* Bind socket to server address */
  if(bind(sock,(struct sockaddr*) &server_address, sizeof(server_address)) < 0){
    fprintf(stderr, "Cannot bind socket to address");
    exit(EXIT_FAILURE);
  }
//maximum of 3 pending connections for the master socket 
  listen(sock,3);
  return sock;
}

/**
 * Accept connection from client
 * @param socket Server listens this
 * @return int File descriptor to accepted connection
 */
int accept_connection(int socket)
{
  int addrlen = 0;
  struct sockaddr_in client_address;
  addrlen = sizeof(client_address);
  return accept(socket,(struct sockaddr*) &client_address,&addrlen);
}

/**
 * Get ip where client connected to
 * @param sock Commander socket connection
 * @param ip Result ip array (length must be 4 or greater)
 * result IP array e.g. {127,0,0,1}
 */
void getip(int sock, int *ip)
{
  socklen_t addr_size = sizeof(struct sockaddr_in);
  struct sockaddr_in addr;
  getsockname(sock, (struct sockaddr *)&addr, &addr_size);
 
  char* host = inet_ntoa(addr.sin_addr);
  sscanf(host,"%d.%d.%d.%d",&ip[0],&ip[1],&ip[2],&ip[3]);
}

/**
 * Lookup enum value of string
 * @param cmd Command string 
 * @return Enum index if command found otherwise -1
 */

int lookup_cmd(char *cmd){
  const int cmdlist_count = sizeof(cmdlist_str)/sizeof(char *);
  return lookup(cmd, cmdlist_str, cmdlist_count);
}

/**
 * General lookup for string arrays
 * It is suitable for smaller arrays, for bigger ones trie is better
 * data structure for instance.
 * @param needle String to lookup
 * @param haystack Strign array
 * @param count Size of haystack
 */
int lookup(char *needle, const char **haystack, int count)
{
  int i;
  for(i=0;i<count; i++){
    if(strcmp(needle,haystack[i])==0)return i;
  }
  return -1;
}


/** 
 * Writes current state to client
 */
void write_state(State *state)
{
  write(state->connection, state->message, strlen(state->message));
}

/**
 * Generate random port for passive mode
 * @param state Client state
 */
void gen_port(Port *port)
{
  srand(time(NULL));
  port->p1 = 128 + (rand() % 64);
  port->p2 = rand() % 0xff;

}

/**
 * Parses FTP command string into struct
 * @param cmdstring Command string (from ftp client)
 * @param cmd Command struct
 */
void parse_command(char *cmdstring, Command *cmd)
{
  sscanf(cmdstring,"%s %s",cmd->command,cmd->arg);
}

/**
 * Handles zombies
 * @param signum Signal number
 */
void my_wait(int signum)
{
  int status;
  wait(&status);
}

main()
{
  server(8021);
  return 0;

}
