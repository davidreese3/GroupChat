/* Program:  Group Chat - Server.c 
   Author:   David Reese in collaboration with Thomas Melody and Amanda Lamphere
   Date:     November 16, 2022
   File:     Server.c
   Compile:  gcc -lpthread -lcrypt -o server Server.c
   Run:     ./server
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netdb.h>
#include <pthread.h>
#include "server-thread-2021.h"
#include "server-thread-2021.c"
#include "authentication.c"

struct SESSION;

typedef struct USER {
   int userId;
   char email[40];
   char name[25];
   char password[64];
   struct SESSION *session; // active when != NULL
   struct USER *next;
} User;

typedef struct MESSAGE {
   int id;
   char message[128];
   long time;          // receiving time
   User *sender;       // sender of the message
   struct MESSAGE *next;
} Message;

typedef struct APP_CONTEXT {
   User *firstUser;
   int nUsers;
   Message *firstMessage;
   Message *lastMessage;
   int nMessages;
   int serverSocket;
   pthread_mutex_t messageLock;
   pthread_mutex_t registrationLock;
   pthread_mutex_t broadcastLock;
} AppContext;

typedef struct REGISTRATION_CONTEXT {
   AppContext *appContext;
   User *user;
} RegistrationContext;

typedef struct SESSION {
 int socket;
 AppContext *appContext;
 User *owner;
} Session;

typedef struct BROADCAST {
   AppContext *appContext;
   Message *message;
} Broadcast;

// server to client
void s2c_send_ok_ack(int socket) {
   int type = 200;  // type = 200
   send(socket, &type, sizeof(int), 0);
}

// send a username to a client
void *send_username(User *user, int socket) {
   int type = 1;
   int nBytes = strlen(user->name) + 1;
   send(socket, &type, sizeof(int), 0);
   send(socket, &nBytes, sizeof(int), 0);
   send(socket, user->name, nBytes, 0);
}

// send a message to a client
void *send_message(Message *message, int socket) {
   int type = 2;
   int nBytes = strlen(message->message) + 1;
   send(socket, &type, sizeof(int), 0);
   send(socket, &nBytes, sizeof(int), 0);
   send(socket, message->message, nBytes, 0);
}


// delivers messages to all connected users
void *broadcast(void *ptr) {
   Broadcast *bcast = (Broadcast *)ptr;

   //lock the broadcast lock
   pthread_mutex_lock(&(bcast->appContext->broadcastLock));

   User *user = bcast->appContext->firstUser;
   User *sender = bcast->message->sender;
   while (user && user->session) {
      send_username(sender, user->session->socket);
      send_message(bcast->message, user->session->socket);
      user = user->next;
   }
   free(user);
   free(sender);

   //unlock the broadcast lock
   pthread_mutex_unlock(&(bcast->appContext->broadcastLock));
}

// creates a new client session when a user connects
void create_session(AppContext *appContext, User *newUser, int clientSocket) {
   Session *newSession = (Session*)malloc(sizeof(Session));
   newSession->appContext = appContext;
   newSession->socket = clientSocket;
   newSession->owner = newUser;
   newUser->session = newSession;
}

// creates and returns a new user
User *create_user(AppContext *appContext, int clientSocket) {
   User *newUser = (User*)malloc(sizeof(User));
   create_session(appContext, newUser, clientSocket);
   return newUser;
}

// add a message to the queue
void addMessage(AppContext *appContext, Message *newMessage) {
   //lock message queue
   pthread_mutex_lock(&(appContext -> messageLock));

   //add message to message queue;
   if(appContext -> firstMessage == NULL){
      appContext -> firstMessage = newMessage;
   }
   else{
      Message *msgPtr = appContext->firstMessage;
      Message *prevPtr = NULL;
      while (msgPtr) {
         prevPtr = msgPtr;
         msgPtr = msgPtr->next;
      }
      prevPtr->next = newMessage;
   }
   appContext->lastMessage = newMessage;

   //unlock message queue
  pthread_mutex_unlock(&(appContext -> messageLock));
}


int user_registration(RegistrationContext *reg) {
   int data_type = 1;
   int nBytes;
   char email[40], name[25], password[64];
   User *user = reg->user;
   while (data_type != 0) {
      // receive email
      int received = recv(user->session->socket, &data_type, sizeof(int), 0);
      received = recv(user->session->socket, &nBytes, sizeof(int), 0);
      received = recv(user->session->socket, email, nBytes, 0);
      // receive username
      received = recv(user->session->socket, &data_type, sizeof(int), 0);
      received = recv(user->session->socket, &nBytes, sizeof(int), 0);
      received = recv(user->session->socket, name, nBytes, 0);
      // receive encrypted password
      received = recv(user->session->socket, &data_type, sizeof(int), 0);
      received = recv(user->session->socket, &nBytes, sizeof(int), 0);
      received = recv(user->session->socket, password, nBytes, 0);

      //lock the registration lock
      pthread_mutex_lock(&(reg->appContext->registrationLock));

      // check if valid email
      User *userPtr = reg->appContext->firstUser;
      while (userPtr) {
         if (userPtr->email && strcmp(userPtr->email, email) == 0) {
            data_type = 1;
            send(user->session->socket, &data_type, sizeof(int), 0);
            break;
         }
         else {
            userPtr = userPtr->next;
         }
      }
      if (data_type != 1) {
         data_type = 0;
         AppContext *appContext = reg->appContext;
         appContext -> nUsers = (appContext -> nUsers) + 1;
         //insert user as first in user queue
         user->next = appContext->firstUser;
         appContext->firstUser = user;
         strcpy(user->email, email);
         strcpy(user->name, name);
         strcpy(user->password, password);
         send(user->session->socket, &data_type, sizeof(int), 0);
      }
   }
   free(user);

   //unlock the registration lock
   pthread_mutex_unlock(&(reg->appContext->registrationLock));

   return 1;
}

int user_login(RegistrationContext *reg) {
   int data_type = 1;
   int nBytes;
   char email[40], password[64];
   User *user = reg->user;
   while (data_type != 0) {
      // receive email
      int received = recv(user-> session->socket, &data_type, sizeof(int), 0);
      received = recv(user->session->socket, &nBytes, sizeof(int), 0);
      received = recv(user->session->socket, &email, nBytes, 0);
      // receive password
      received = recv(user->session->socket, &data_type, sizeof(int), 0);
      received = recv(user->session->socket, &nBytes, sizeof(int), 0);
      received = recv(user->session->socket, &password, nBytes, 0);

      int finding_email = 1;
      User *userPtr = reg->appContext->firstUser;
      while (finding_email && userPtr) {
         if (userPtr->email && strcmp(email, userPtr->email) == 0) {
            finding_email = 0;
         }
         else {
            userPtr = userPtr->next;
         }
      }
      data_type = 1;
      if (authenticate(password, userPtr->password)) {
         data_type = 0;
      }
      if (finding_email == 0 && data_type == 0) {
         userPtr->session = user->session;
         reg->user = userPtr; // so subserver has the proper user pointer
      }

      send(user->session->socket, &data_type, sizeof(int), 0);
   }
   free(user);
   return 0;
}

// waits for the user to select either "register" or "login" after connecting
void wait_for_registration(RegistrationContext *reg) {
   int data_type;
   int received = recv(reg->user->session->socket, &data_type, sizeof(int), 0);
   while (data_type) {
      if (data_type == 5) {
         data_type = user_registration(reg);
         recv(reg->user->session->socket, &data_type, sizeof(int), 0); //wait for login request
      }
      else {
         data_type = user_login(reg);
      }
   }
}

void *subserver(void *ptr) {
   RegistrationContext *regContext = (RegistrationContext *)ptr;

   wait_for_registration(regContext);
   User *user = regContext->user;

   int data_type = 1; // 1 is the data type used for username communication
   int EXIT_DATA_TYPE = 99; // this constant value is the exit number

   //send ok ack to client
   AppContext *appContext = regContext->appContext;
   s2c_send_ok_ack(appContext -> serverSocket);
   
   int nBytesm;
   int nRecievedm;
      
   while (user->session) {
      //allocate space for new message
      Message *newMessage = (Message*)malloc(sizeof(Message));
      appContext -> nMessages = (appContext -> nMessages) + 1;
      //accept a message from the client
      nRecievedm = recv(user->session->socket, &data_type, sizeof(int), 0);
      if (nRecievedm == 0) {
         break;
      }

      if (data_type != EXIT_DATA_TYPE) {
         nRecievedm = recv(user->session->socket, &nBytesm, sizeof(int), 0);
         nRecievedm = recv(user->session->socket, newMessage -> message, nBytesm, 0);

         newMessage->sender = user;
         newMessage->next = NULL;
               
         addMessage(appContext, newMessage);

         Broadcast *bcast = (Broadcast*)malloc(sizeof(Broadcast));
         bcast->appContext = appContext;
         bcast->message = newMessage;
         pthread_t bcastThread;
         pthread_create(&bcastThread, NULL, broadcast, bcast);
      }
      else {
         printf("Client [%s] disconnected from the server.\n", user->name);
         break;
      }
   }
   if (nRecievedm > 0) {
      close(user->session->socket);
      printf("Client [%s] closed the connection.\n", user->name);
      user->session = NULL;
   }
   else {
      printf("Client [%s] terminated their program.\n", user->name);
      close(user->session->socket);
   }
   
}

void *server(void *ptr) {
   int data_type = 1;
   int nReceived = 1;
   
   int clientSocket;

   AppContext *appContext = (AppContext *)ptr;
   while(1) {
      //accept socket connection
      clientSocket = accept_client(appContext -> serverSocket);
      if (clientSocket == -1) {
         continue;
      }
      User *newUser = create_user(appContext, clientSocket);
      RegistrationContext *regContext = (RegistrationContext*)malloc(sizeof(RegistrationContext));
      regContext->appContext = appContext;
      regContext->user = newUser;
      pthread_t subserverThread;
      pthread_create(&subserverThread, NULL, subserver, regContext); // make sure the arguments here are correct
   }
}

// pass in AppContext
void *printUsers(void *ptr) {
   AppContext *app = (AppContext*)ptr;
   int i = 0;
   if (app->nUsers != 0) {
      printf("Current users:\n");
      User *userPtr = app->firstUser;
      while (userPtr) {
         printf("%s - ", userPtr->name);
         if (userPtr->session) {
            printf("active");
         }
         else {
            printf("inactive");
         }
         if (strlen(userPtr->password) > 0) {
            printf(" | %s\n", userPtr->password);
         }
         else {
            printf("\n");
         }
         userPtr = userPtr->next;
      }
   }
   else {
      printf("No users!\n");
   }
   printf("\n");
}

// pass in AppContext
void *printMessages(void *ptr) {
   AppContext *app = (AppContext*)ptr;
   int i = 0;
   if (app->nMessages != 0) {
      printf("Messages:\n");
      Message *msgPtr = app->firstMessage;
      while (msgPtr) {
         printf("%s: %s\n", msgPtr->sender->name, msgPtr->message);
         msgPtr = msgPtr->next;
      }
   }
   else {
      printf("No messages!\n");
   }
   printf("\n");
}

void *reportState(void *ptr) {
   AppContext *appContext = (AppContext *)ptr;
   while (1) {
      printUsers((void*)appContext);
      printMessages((void*)appContext);
      sleep(3);
   }
}

int main() {
   //create/initiate user/message queues
   AppContext *appContext = (AppContext*)malloc(sizeof(AppContext));
   appContext -> firstUser = NULL;
   appContext -> firstMessage = NULL;
   
   //connect to the server - connected
   char *host = "";           // specific host ommited as it ran on University servers
   char *port = "";           // specific port ommited as it ran on University servers
   int socketConnection = start_server(host, port, 10);
   
   //set serverSocket (appContext->serverSocket)
   appContext -> serverSocket = socketConnection;

   //initialize the locks
   pthread_mutex_init(&(appContext -> messageLock), NULL);
   pthread_mutex_init(&(appContext -> registrationLock), NULL);
   pthread_mutex_init(&(appContext -> broadcastLock), NULL);
   
   //create&start reportState thread
   pthread_t reportThread;
   pthread_create(&reportThread, NULL, reportState, appContext);
   // start server thread
   pthread_t serverThread;
   pthread_create(&serverThread, NULL, server, appContext);
   pthread_join(reportThread, NULL);
   pthread_join(serverThread, NULL);
}
