#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#define MAX_CLIENTS 100
#define DB_SIZE 100

struct resp_command {
  char name[100];
  char arguments[300];
  char type;
} resp_command;

struct database {
  char key[100];
  char value[200];
} database;

void insertIntoCache(struct database *db, char *key, char *value) {
  printf("key: %s\n", key);
  printf("value: %s\n", value);
  for (int i = 0; i < DB_SIZE; i++) {
     if(strcmp(&db[i].key, "-1") == 0) {
	strncpy(db[i].key, key, sizeof(db[i].key) - 1);
	strncpy(db[i].value, value, sizeof(db[i].value) - 1);
	break;
     }
     // TODO what if I don't find space in the DB
     // need to return an error 
  }
}

char *getValueFromCache(struct database *db, char key[100]) {
  for (int i = 0; i < DB_SIZE; i++) {
     if(strcasecmp(db[i].key, key) == 0) {
	  printf("value found: %s\n", db[i].value);
	  return db[i].value;
     }
  }

  // return "a";
  // TODO what to return if no record found
}

void trim(char *str) {
  char *start = str; // begining of thes tring
  char *end;

  while (isspace((unsigned char)*start)) {
	  start++;
  }

  // if the string only contains spaces
  if (*start == '\0') {
	str[0] = '\0'; // set it to empty
  }

  end = start + strlen(start) - 1;

  // trim trailing spaces
  while (end > start && isspace((unsigned char)*end)) {
	  end--;
  }

  *(end + 1) = '\0';

  memmove(str, start, end - start + 2); // +2 to include null temrinator

}


// TODO create a function to create response
// with correct format

// TODO
//  add if else statements to handle
//  the different commands
//  starting with ECHO
void handleCommand(struct resp_command command, int fd, struct database *db) {
  printf("command to execute: %s\n", command.name);
  if(strcasecmp(command.name, "PING") == 0) {
     dprintf(fd, "+PONG\r\n");
  } else if(strcasecmp(command.name, "ECHO") == 0) {
     dprintf(fd, "$%i\r\n%s\r\n", strlen(&command.arguments[0]), &command.arguments[0]);
  } else if (strcasecmp(command.name, "SET") == 0) {
     char *key = strtok(command.arguments, " ");
     char *value = strtok(NULL, " ");
     insertIntoCache(db, key, value);
     dprintf(fd, "+OK\r\n");
  } else if (strcasecmp(command.name, "GET") == 0) {
     char *value = getValueFromCache(db, command.arguments); 
     dprintf(fd, "$%i\r\n%s\r\n", strlen(value), value);
  }
}

// change the parameter to be dynamic
struct resp_command parse_resp(char input[1000]) {
  struct resp_command command;
  memset(&command, '\0', sizeof(command));
  char *terminator = "\r\n";
  int i = 0;
  char *token;

  // TODO improve this parsing
  token = strtok(input, terminator);
  while(token != NULL) {
    // printf("token: %s\n", token);
    // start of the string
    // has the amount of data sent
    if (token[0] == '*') {
      // printf("parsing command with %c arguments\n", token[1]);
      //move to the next token
      token = strtok(NULL, terminator);
      continue;
    }
    // printf("token: %s is here\n", token);

    // token with the type
    if(token[0] == '$') {
      command.type = token[0];
      token = strtok(NULL, terminator);
      continue;
    }

    // if command is not already set
    // check if the current token
    // is the name of the command
    if (strcasecmp(token, "PING") == 0) {
       strcat(command.name, token);
    } else if (strcasecmp(token, "ECHO") == 0) {
       strcat(command.name, token);
    } else if (strcasecmp(token, "GET") == 0) {
       strcat(command.name, token);
    } else if (strcasecmp(token, "SET") == 0) {
       strcat(command.name, token);
    }  else {
      // TODO this is bad parsing
      // need to improve
      strcat(command.arguments, " ");
      strcat(command.arguments, token);
      trim(command.arguments);
      i++;
    }
    // move to the next token
    token = strtok(NULL, terminator);
  }

   // printf("name: %s\n", command.name);
   // printf("args: %s\n", command.arguments);

  return command;
}

int main() {
  // Disable output buffering
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  // You can use print statements as follows for debugging, they'll be visible when running tests.
  printf("Logs from your program will appear here!\n");

   int server_fd;
   socklen_t client_addr_len;
   struct sockaddr_in client_addr;
   struct pollfd pollfds[MAX_CLIENTS];

   server_fd = socket(AF_INET, SOCK_STREAM, 0);
   if (server_fd == -1) {
    printf("Socket creation failed: %s...\n", strerror(errno));
    return 1;
   }

   // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
   int reuse = 1;
   if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    printf("SO_REUSEADDR failed: %s \n", strerror(errno));
    return 1;
   }

   struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
                   .sin_port = htons(6379),
                   .sin_addr = { htonl(INADDR_ANY) },
                  };

   if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
    printf("Bind failed: %s \n", strerror(errno));
    return 1;
   }

   int connection_backlog = 5;
   if (listen(server_fd, connection_backlog) != 0) {
    printf("Listen failed: %s \n", strerror(errno));
    return 1;
   }

   printf("Waiting for a client to connect...\n");
   client_addr_len = sizeof(client_addr);

   char buff[1000];
   struct database db[DB_SIZE];

   pollfds[0].fd = server_fd;
   pollfds[0].events = POLLIN;
   int nfds = 1;

   // initialize pollfds array
   for (int i = 1; i < MAX_CLIENTS; i++) {
     pollfds[i].fd = 0;
   }

   // initialize DB
   for (int i = 0; i < DB_SIZE; i++) {
	db[i].key[0] = '-';
	db[i].key[1] = '1';
	db[i].key[2] = '\0';
   }

   while(1) {
    int bytes = poll(pollfds, nfds, -1);
    if (bytes > 0) {
      // handle the case for when a client
      // wants to connect
      if(pollfds[0].revents && POLLIN) {
        int clientfd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
        printf("accept successful: %i\n", clientfd);

        for (int i = 1; i < MAX_CLIENTS; i++) {
          // if slot available
          // set it
          if(pollfds[i].fd == 0) {
            pollfds[i].fd = clientfd;
            pollfds[i].events = POLLIN;
            nfds++;
            break;
          }
        }
      }
    }

    // handle all remaining events
    // start from 1 because index 0
    // is to handle connection events
    for (int i = 1; i < MAX_CLIENTS; i++) {
      // if we get a POLLIN event and the slot is not empty
      // handle it
      if(pollfds[i].revents && pollfds[i].fd != 0 && POLLIN) {
        int buf_size = read(pollfds[i].fd, buff, 1000);
        if(buf_size == -1 || buf_size == 0) {
           pollfds[i].fd = 0;
           pollfds[i].events = 0;
           pollfds[i].revents = 0;
           nfds--;
        } else {
          struct resp_command command;
          char *resp;

          command = parse_resp(buff);
          handleCommand(command, pollfds[i].fd, db);
        }
      }
    }
   }

   close(server_fd);
   return 0;
}
