/* Program:  Group Chat - Client.c 
   Author:   David Reese with planning assitance from Thomas Melody and Amanda Lamphere
   Date:     November 16, 2022
   File:     Client.c
   Compile:  gcc -lpthread -lcrypt -o client Client.c
   Run:     ./client
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "client-thread-2021.h"
#include "client-thread-2021.c"
#include <pthread.h>
#include "authentication.c"

int connectToServer(){
   char* host = "";        // specific host ommited as it ran on University servers
   char *port = "";        // specific port ommited as it ran on University servers 
   int socketConnection = get_server_connection(host, port);
   if (socketConnection == -1) {
      printf("Socket failed to connect\n");
   }
   printf("connected to server: %d\n", socketConnection);
   return socketConnection;
}

// communication protocol 
void c2s_send_username(int connection, char* username) {
   int type = 1;                          // type = 1
   int nBytes = strlen(username)+1;       // nBytes = N + 1, N < 24 
   send(connection, &type, sizeof(int), 0);
   send(connection, &nBytes, sizeof(int), 0);
   send(connection, username, nBytes, 0);
}

void c2s_send_message(int connection, char * message) {
   int type = 2;                          // type = 2
   int nBytes = strlen(message)+1;        // nBytes = N + 1, N < 127
   send(connection, &type, sizeof(int), 0);
   send(connection, &nBytes, sizeof(int), 0);
   send(connection, message, nBytes, 0);
}

void c2s_send_email(int connection, char * email){
   int type = 3;                          // type = 3
   int nBytes = strlen(email)+1;          // nBytes = N + 1, N < 40
   send(connection, &type, sizeof(int), 0);
   send(connection, &nBytes, sizeof(int), 0);
   send(connection, email, nBytes, 0);
}

void c2s_send_password(int connection, char * pswd){
   int type = 4;                          // type = 4
   int nBytes = strlen(pswd)+1;           // nBytes = N + 1, N < 32
   send(connection, &type, sizeof(int), 0);
   send(connection, &nBytes, sizeof(int), 0);
   send(connection, pswd, nBytes, 0);
}

void c2s_send_registration(int connection) {
   int type = 5;                         // type = 5
   send(connection, &type, sizeof(int), 0);
}

void c2s_send_reg_info(int connection, char *email, char *user, char *pswd){
   c2s_send_email(connection, email);
   c2s_send_username(connection, user);
   c2s_send_password(connection, pswd);
}

void c2s_send_login(int connection) {
   int type = 6;                         // type = 6
   send(connection, &type, sizeof(int), 0);
}

void c2s_send_login_info(int connection, char *email, char *pswd){
   c2s_send_email(connection, email);
   c2s_send_password(connection, pswd);
}

void c2s_send_exit(int connection) {
   int type = 99;                         // type = 99
   send(connection, &type, sizeof(int), 0);
}


// user registration - read
char* readEmail(){
   printf("please enter an email\n");
   char *input = malloc(sizeof(char)*40);
   fgets(input, 39, stdin);
   input[strlen(input)-1] = '\0';
   return input;
}

char* readUser(){
   printf("please enter a username\n");
   char *input = malloc(sizeof(char)*25);
   fgets(input, 24, stdin);
   input[strlen(input)-1] = '\0';
   return input;
}

char* readRegisterPassword(){
   char *plainpswd = malloc(sizeof(char)*64);
   plainpswd = getpass("please enter a password\n");
   return encode(plainpswd);
} 

char* readPassword(){
   char *plainpswd = malloc(sizeof(char)*32);
   plainpswd = getpass("please enter a password\n");
   return plainpswd;
}

char* readMessage(){   
   printf("please enter a message\n");
   char *input = malloc(sizeof(char)*128);
   fgets(input, 127, stdin);
   input[strlen(input)-1] = '\0';
   return input;
}

void readEmptyMessage(){
   char *input = malloc(sizeof(char));
   fgets(input, 2, stdin);
   free(input);
}

void *sendMessages(void *ptr){
   int *connectionPtr = (int *)ptr;
   int connection = *connectionPtr;
   char *message = malloc(sizeof(char)*25);
   message = readMessage();
   char * exit = "#$%";
   while ((strcmp(message,exit))!=0) {
      c2s_send_message(connection, message);
      message = readMessage();
   }
   c2s_send_exit(connection);
   close(connection);
   printf("disconnected from server\n");
}

int acceptUsername(int connection){
   char * username = malloc(sizeof(char)*25);
   int recieved, datatype, bytes;
   recieved = recv(connection, &datatype, sizeof(int), 0);
   recieved = recv(connection, &bytes, sizeof(int), 0);
   recieved = recv(connection, username, bytes, 0);
   if (recieved == -1) {
      return 1;
   }
   else {
      printf("Username: %s - ",username);
      return 0;
   }
}

int acceptMessage(int connection){
   char * message = malloc(sizeof(char)*128);
   int recieved, datatype, bytes;
   recieved = recv(connection, &datatype, sizeof(int), 0);
   recieved = recv(connection, &bytes, sizeof(int), 0);
   recieved = recv(connection, message, bytes, 0);
   if (recieved == -1) {
      return 1;
   }
   else {
      printf("Message: %s\n",message);
      return 0;
   }
}

void *receiveMessages(void *ptr){
   int *connectionPtr = (int *)ptr;
   int connection = *connectionPtr;
   int errorCounter = 0;
   while(errorCounter == 0) {
      errorCounter = errorCounter + acceptUsername(connection);
      errorCounter = errorCounter + acceptMessage(connection);
   }
}

void printRegistrationValidity(int value){
   if(value == 0){
      printf("user registered\n");
   }
   else {
      printf("email is already in use. please reenter your registration info\n");
   }
}

void userRegistration(int connection){
   c2s_send_registration(connection);
   int recieved, stillContinue = 1;
   readEmptyMessage();
   char * email = malloc(sizeof(char)*40);
   char * user = malloc(sizeof(char)*25);
   char * pswd = malloc(sizeof(char)*32);
   while (stillContinue){
      email = readEmail();
      user = readUser();
      pswd = readRegisterPassword();
      c2s_send_reg_info(connection, email, user, pswd);
      recieved = recv(connection, &stillContinue, sizeof(int), 0); 
      printRegistrationValidity(stillContinue);
   }
   free(email);
   free(user);
   free(pswd);
}

void printLoginValidity(int value){
   if(value == 0){
      printf("user logged in\n");
   }
   else {
      printf("invalid log in please reenter your log in info\n");
   }
}

void userAuthentication(int connection){
   c2s_send_login(connection);
   int recieved, stillContinue = 1;
   readEmptyMessage();
   char * email = malloc(sizeof(char)*40);
   char * pswd = malloc(sizeof(char)*32);
   while (stillContinue){
      email = readEmail();
      pswd = readPassword();
      c2s_send_login_info(connection, email, pswd);
      recieved = recv(connection, &stillContinue, sizeof(int), 0);
      printLoginValidity(stillContinue);
   }
   free(email);
   free(pswd);
}

char getInputCode(){
   char inputCode;
   scanf("%c", &inputCode);
   return inputCode;
} 

void printOptions(){
   printf("Please choose one of the following options:\n");
   printf("1: Registration\n");
   printf("2: Log In\n");
}

void options(int connection){
   printOptions();
   char inputCode = getInputCode();
   while(inputCode != '2'){
      if(inputCode == '1'){
         printf("Selected: Registration\n");
         userRegistration(connection);
         printOptions();
      }
      else{
         printf("Error: Not an Option\n");
         printOptions();
      }
      inputCode = getInputCode();
   }
   printf("Selected: Log In\n");
   userAuthentication(connection);
}

int main() {
   int socketConnection = connectToServer();
   printf("socketconnection is %d\n", socketConnection);
   options(socketConnection);
   int * connectionPtr = &socketConnection;
   pthread_t sendThread, receiveThread;
   pthread_create(&sendThread, NULL, sendMessages, connectionPtr);
   pthread_create(&receiveThread, NULL, receiveMessages, connectionPtr);
   pthread_join(sendThread, NULL);
   pthread_join(receiveThread, NULL);
} 