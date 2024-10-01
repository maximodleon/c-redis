#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#define MAX_CLIENTS 100

struct resp_command {
  char *name;
  char *arguments[10];
  char type;
} resp_command;

void execute_command(struct resp_command command, int fd) {
  if (strcasecmp(command.name, "ECHO") == 0) {
    int length = strlen(command.arguments[0]);
    dprintf(fd, "$%i\r\n%s\r\n", length, command.arguments[0]);
  } else if (strcasecmp(command.name, "PING") == 0) {
    dprintf(fd, "+%s\r\n", "PONG");
  }
}

// change the parameter to be dynamic
void parse_resp(char input[1000], struct resp_command *command) {
  // struct resp_command command;
  // TODO this will be a constant
  char *terminator = "\r\n";
  int i = 0;
  char *token;

  token = strtok(input, terminator);
  while(token != NULL) {
    // start of the string
    // has the amount of data sent
    if (token[0] == '*') {
      //move to the next token
      token = strtok(NULL, "\r\n");
      continue;
    }

    // token with the type
    if(token[0] == '$') {
      command->type = token[0];
      token = strtok(NULL, "\r\n");
      continue;
    }

     // TODO need to find a better way to check if
     // it is a valid command
     // for now, just checking if it is ECHO
     if (strcasecmp(token, "ECHO") == 0) {
        command->name = token;
     } else if (strcasecmp(token, "PING") == 0) {
      command->name = token;
    } else {
      // TODO this is bad parsing
      // need to improve
       command->arguments[i] = token;
       i++;
    }

    // move to the next token
    token = strtok(NULL, "\r\n");
  }
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

  // // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // // ensures that we don't run into 'Address already in use' errors
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

   pollfds[0].fd = server_fd;
   pollfds[0].events = POLLIN;
   int nfds = 1;

   // initialize pollfds array
   for (int i = 1; i < MAX_CLIENTS; i++) {
     pollfds[i].fd = 0;
   }

   while(1) {
    int bytes = poll(pollfds, nfds, -1);
    if (bytes > 0) {
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
          // TODO need to find right size
          // it needs to be dynamic
          // to pass the test for codecrafters
           char response[18] = {};
           struct resp_command command;
           parse_resp(buff, &command);
           execute_command(command, pollfds[i].fd);
           // send(pollfds[i].fd, response, sizeof(response), 0);
        }
      }
    }
   }

   close(server_fd);
   return 0;
}
