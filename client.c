#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>

/*
I have a struct for the threadLog. This struct stores a thread's:
1. ThreadID: which is assigned in the for loop. 
2. socketID: which comes from the server's response.
3. timeOfSpellCheckRequest: Used for saving the time for the spell check
4. timeOfSpellCheckReply: used for saving the time for the response from the server.
5. timeOfThreadTerminationRequest: saving the time the thread got terminated.
*/
typedef struct {
    int threadID;
    int socketID;
    time_t timeOfSpellCheckRequest;
    time_t timeOfSpellCheckReply;
    time_t timeOfThreadTerminationRequest;
} ThreadLog;

int getRandomNumber();
void printTheClientData(ThreadLog* clientData);
void *clientThreadFunc(void *args);
int getSocketID(char *replyFromSever);

int numberOfClientThreads = 0;
int portNumber = 0;

char *WORDS[] = {"Hello", "this", "how", "awesome", "are", "greatest", "thing", "you", "the", "world", "love"};


int main(int argc, char **argv) {
    portNumber = atoi(argv[1]);
    numberOfClientThreads = atoi(argv[2]);
    // if the numberOfClientThreads are less than 10, give the user an error.
    if (numberOfClientThreads < 10) {
        perror("The number of client threads must atleast be 10");
        exit(EXIT_FAILURE);
    }
    // creating the number clientThreads, and client_data based on the numberOfClientThreads.
    // Waiting for them to join and finish before moving to the next thread. 
    pthread_t clientThreads[numberOfClientThreads];
    ThreadLog client_data[numberOfClientThreads];
    for(int i = 0; i < numberOfClientThreads; i++) {
        client_data[i].threadID = i;
        pthread_create(&clientThreads[i], NULL, clientThreadFunc, &client_data[i]);
        pthread_join(clientThreads[i], NULL);
        printf("Thread %d finished.\n", i);
        printf("\n");
    }
    return 0;
}

void *clientThreadFunc(void *args) {
    // Setting a threadLog pointer to a void ponter argument for that specific thread.
    ThreadLog *client_data = (ThreadLog *)(args);

    // declaring server of type sockaddr_in
    // with specific address family, IP Address and port number
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(portNumber);

    // Getting a randomNumber and setting a char *randomWord to that
    int randomNumber = getRandomNumber();
    char *randomWord = WORDS[randomNumber];
    // creating a socket here.
    int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    
    // connecting to the server here. 
    connect(socket_desc, (struct sockaddr *)&server, sizeof(server));
           
    

    // time of spell check request. This is where I ask for the spell check. 
    // save the time requesting for spell check and save it in the timeOfSpellCheckRequest for
    // the thread's client_data.
    struct timespec checkRequest;
    clock_gettime(CLOCK_REALTIME, &checkRequest);
    client_data->timeOfSpellCheckRequest = checkRequest.tv_sec * 1000 + checkRequest.tv_nsec / 1000000;

    // sending the message to the server
    char message[256];
    strcpy(message, randomWord);
    printf("Message before send = %s\n", randomWord);
    send(socket_desc, message, strlen(message), 0);

    // creating a random delay
    int delay = getRandomNumber();
    sleep(delay);

    char replyFromServer[256];
    // receiving the reply from server if there are bytes which I get from the server, 
    // put a null character at the end.
    // The reply from server gets the serverID and then I store the server ID
    ssize_t bytesReceivedFromServer = recv(socket_desc, replyFromServer, sizeof(replyFromServer), 0);
    if (bytesReceivedFromServer > 0) {
        replyFromServer[bytesReceivedFromServer] = '\0';
        printf("Received: %s\n", replyFromServer);
    } else if (bytesReceivedFromServer == 0) {
        printf("Connection closed by the server\n");
    } else {
        perror("recv");
    }
    int socketID = getSocketID(replyFromServer);
    client_data->socketID = socketID;
    // check reply time and storing the reply in the timeOfSpellCheckReply for the thread.
    struct timespec checkReply;
    clock_gettime(CLOCK_REALTIME, &checkReply);
    client_data->timeOfSpellCheckReply = checkReply.tv_sec * 1000 + checkReply.tv_nsec/1000000;

    // All the things are done now is the time to terminate the thread, and save that value. 
    struct timespec threadTerminate;
    clock_gettime(CLOCK_REALTIME, &threadTerminate);
    client_data->timeOfSpellCheckReply = threadTerminate.tv_sec * 1000 + threadTerminate.tv_nsec/1000000;

    // closing the socket_desc
    time(&client_data->timeOfThreadTerminationRequest);
    printTheClientData(client_data);
    close(socket_desc);
    return NULL;
}

// Function just returns a random number. 
int getRandomNumber() {
    int randomNumber = rand() % 10; 
    return randomNumber;
}

// This prints the perimeters of the clientData
void printTheClientData(ThreadLog* clientData) {
    printf("threadId = %d, socketId = %d, timeOfSpellCheckRequest = %ld, timeOfSpellCheckReply = %ld, timeOfThreadTerminationRequest = %ld\n"
    ,clientData->threadID, clientData->socketID, clientData->timeOfSpellCheckRequest, clientData->timeOfSpellCheckReply, clientData->timeOfThreadTerminationRequest);
}

// This function returns the socketId in the response.
// It passes any char values and stores the integer values, and then I return the integer value.
int getSocketID(char *replyFromServer) {
    int number;
    if (sscanf(replyFromServer, "%*[^0-9]%d", &number) == 1) {
        return number;
    } else {
        return -1; 
    }
}

