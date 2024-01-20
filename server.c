#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include "server.h"

#define DEAULT_DICTIONARY "dictionary.txt";
#define DEFAULT_PORT_NUMBER 8080
#define MAX_NUMBER_OF_WORDS 100
#define QUEUE_SIZE 10


//void initializeNetwork(int portNumber);
void *workerThreadFunc(void *args);
void *acceptClientThreadsFunc(void *args);
char **loadDictionary(char *dictionaryFile, int isDictionaryFileProvided);
//void addToConnectionQueue(int client_socket, int priority);
struct connectionCell removeFromConnectionQueue();
//void processConnection(struct connectionCell connection_info);
void *loggerThreadFunc(void *args);
void addToLogQueue(char *response);
char *removeFromLogQueue();
void addToConnectionBuffer(struct connectionCell new_connection);
int getPriority();
int isTheWordInDictionary(char *buffer);
// void processLogEntry(struct logCell logEntry);
void initializeConnectionQueue(int numberOfCells);
int checkIfInteger(char *str);
struct connectionCell getClientWithHighestPriority();
void runServer(int numberOfWorkerThreads, char *schedulingType);
char *removeSpace(char *str);

pthread_mutex_t connection_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t connection_queue_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t connection_queue_empty = PTHREAD_COND_INITIALIZER;

pthread_mutex_t log_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t log_queue_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t log_queue_empty = PTHREAD_COND_INITIALIZER;




struct connectionCell *connection_queue = NULL;
char *log_queue[QUEUE_SIZE];


int connection_queue_size = 0;
int log_queue_size = 0;

char *dictionaryFile;
int portNumber = 0;
int numberOfWorkerThreads = 0;
int numberOfCellsInConnectionQueue = 0;
char *schedulingType;
char **dictionaryWords = NULL;


int main(int argc, char **argv) {
    pthread_t workerThread[numberOfWorkerThreads];
    pthread_t loggerThread;

    
    int isDictionaryFileProvided = 1;

    if (argc <= 1 || argc > 6) {
        printf("%s", "Incorrect Number of arguments provided");
        exit(EXIT_FAILURE);
    } else if (argc == 4) {
        isDictionaryFileProvided = 1;
        //dictionaryFile = "/usr/share/dict/words";
        dictionaryFile = DEAULT_DICTIONARY;
        portNumber = DEFAULT_PORT_NUMBER;
        numberOfWorkerThreads = atoi(argv[1]);
        numberOfCellsInConnectionQueue = atoi(argv[2]);
        schedulingType = argv[3];
    } else {
        if (argc == 5) {
            if (checkIfInteger(argv[1])) {
                isDictionaryFileProvided = 1;
                dictionaryFile = DEAULT_DICTIONARY;
                portNumber = atoi(argv[1]);
                numberOfWorkerThreads = atoi(argv[2]);
                numberOfCellsInConnectionQueue = atoi(argv[3]);
                schedulingType = argv[4];
            } else {
                dictionaryFile = argv[1];
                isDictionaryFileProvided = 0;
                portNumber = DEFAULT_PORT_NUMBER;
                numberOfWorkerThreads = atoi(argv[2]);
                numberOfCellsInConnectionQueue = atoi(argv[3]);
                schedulingType = argv[4];
            }
        } else {
            isDictionaryFileProvided = 0;
            dictionaryFile = argv[1];
            portNumber = atoi(argv[2]);
            numberOfWorkerThreads = atoi(argv[3]);
            numberOfCellsInConnectionQueue = atoi(argv[4]);
            schedulingType = argv[5];
        }
    }
    for (int i = 0; i < numberOfWorkerThreads; i++) {
        pthread_create(&workerThread[i], NULL, workerThreadFunc, (void*)schedulingType);
    }

    initializeConnectionQueue(numberOfCellsInConnectionQueue);
    pthread_create(&loggerThread, NULL, loggerThreadFunc, NULL);

    // for (int i = 0; i < numberOfWorkerThreads; i++) {
    //     pthread_join(workerThread[i], NULL);
    // }

    // pthread_join(loggerThread, NULL);


        // order in which worker threads will access connection queues
        // order is either FIFO or priority 
    dictionaryWords = loadDictionary(dictionaryFile, isDictionaryFileProvided);

    int socket_desc, client_socket;
    struct sockaddr_in server, client;

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc < 0) {
        puts("Error creating socket");
        exit(1);
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    //server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(portNumber);

    int bind_result = bind(socket_desc, (struct sockaddr*)&server, sizeof(server));
    if (bind_result < 0) {
        puts("Error: failed to bind");
        exit(1);
    }
    puts("Bind done");

    int listenResult = listen(socket_desc, 3);
    puts("waiting for incoming connections...");
    if (listenResult < 0) {
        perror("Error listening on server socket");
        exit(EXIT_FAILURE);
    }

    while (1) {
        int client_length = sizeof(client);
        client_socket = accept(socket_desc, (struct sockaddr*)&client, (socklen_t*)&client_length);
        if (client_socket < 0) {
            puts("Error: Accept failed");
            continue;
        }

        puts("connection accepted");

        int priority = getPriority();
        struct connectionCell new_connection;
        new_connection.socketDescriptor = client_socket;
        new_connection.priority = priority;
        new_connection.connectionTime = time(NULL);
        // ctime, when the time connected;
       
        addToConnectionBuffer(new_connection);
        pthread_cond_signal(&connection_queue_full);
    }
    return 0;    
}


void initializeConnectionQueue(int numberOfCells) {
    printf("Inside initializeConnectionQueue function\n");
    connection_queue = (struct connectionCell*)malloc(numberOfCells *sizeof(struct connectionCell));
    //connection_queue_size = numberOfCells;
    printf("connection_queue_size = %d, numberOfCells = %d\n", connection_queue_size, numberOfCells);
}


void *workerThreadFunc(void *args) {
    printf("inside workerThreadFunc\n");
    char *schedulingType = (char *)args;
    char buffer[250];
    //struct logCell logEntry;
    while (1) {
        pthread_mutex_lock(&connection_queue_mutex);
        while (connection_queue_size == 0) {
            printf("Inside wait for workerThread\n");
            pthread_cond_wait(&connection_queue_full, &connection_queue_mutex);
        }
        pthread_mutex_unlock(&connection_queue_mutex);
        printf("Lock gets released\n");
        //int temp = connection_queue_size;
        if (strcmp(schedulingType, "FIFO") == 0) {
            printf("Entering FIFO\n");
            // for(int i = 0; i < temp; i++) {
            struct connectionCell connection_info = removeFromConnectionQueue();
            printf("priority = %d, socket = %d\n", connection_info.priority, connection_info.socketDescriptor);
            ssize_t bytes_received;
            while ((bytes_received = read(connection_info.socketDescriptor, buffer, sizeof(buffer))) > 0) {
                buffer[bytes_received] = '\0';  // Null-terminate the received data
                printf("Received: %s\n", buffer);
                int flag;
                char newBuffer[200];                
                char response[400];
                char *removeSpaceBuffer = removeSpace(buffer);
                strcpy(newBuffer, removeSpaceBuffer);
                free(removeSpaceBuffer);
                flag = isTheWordInDictionary(newBuffer);
                if (flag) {
                    printf("buffer = %s, Word is in the dictionary\n", newBuffer);
                    // snprintf(response, sizeof(response), "%s + OK\n", newBuffer);
                    snprintf(response, sizeof(response), "%s + OK + %d\n", newBuffer, connection_info.socketDescriptor);
                    // response[sizeof(response) - 1] = '\0'; 
                    printf("response = %s\n", response);
                } else {
                    printf("Word is not in the dictionary\n");
                    snprintf(response, sizeof(response), "%s + MISSPELLED + %d\n", newBuffer, connection_info.socketDescriptor);
                    // response[sizeof(response) - 1] = '\0';  
                    printf("response = %s\n", response);
                }
                write(connection_info.socketDescriptor, response, strlen(response));
                addToLogQueue(response);
                pthread_cond_signal(&log_queue_full);
                printf("closing socketDescriptor = %d\n", connection_info.socketDescriptor);
            }
            close(connection_info.socketDescriptor);
        }
        if (strcmp(schedulingType, "priority") == 0) {
            printf("Entering priority\n");
            struct connectionCell connection_info = getClientWithHighestPriority();
            //printf("priority = %d, socket = %d\n", connection_info.priority, connection_info.socketDescriptor);
            ssize_t bytes_received;
            while ((bytes_received = read(connection_info.socketDescriptor, buffer, sizeof(buffer))) > 0) {
                buffer[bytes_received] = '\0';  // Null-terminate the received data
                printf("Received: %s\n", buffer);
                int flag;
                char newBuffer[250];                
                char response[2000];
                char *removeSpaceBuffer = removeSpace(buffer);
                strcpy(newBuffer, removeSpaceBuffer);
                free(removeSpaceBuffer);
                flag = isTheWordInDictionary(newBuffer);
                if (flag) {
                    printf("buffer = %s, Word is in the dictionary\n", newBuffer);
                    snprintf(response, sizeof(response), "%s + OK\n", newBuffer);
                    response[sizeof(response) - 1] = '\0'; 
                    printf("response = %s\n", response);
                } else {
                    printf("Word is not in the dictionary\n");
                    snprintf(response, sizeof(response), "%s + MISSPELLED\n", newBuffer);
                    response[sizeof(response) - 1] = '\0'; 
                    printf("response = %s\n", response);
                    
                }
                write(connection_info.socketDescriptor, response, strlen(response));
                addToLogQueue(response);
                pthread_cond_signal(&log_queue_full);
            }
            close(connection_info.socketDescriptor);
        }
    }
    return NULL;
}





char **loadDictionary(char *dictionaryFile, int isDictionaryFileProvided) {
    // Implement dictionary loading logic
    // open the dictionary file, read words, and populate the data strcuture
    char **toReturn = NULL;
    if (isDictionaryFileProvided == 0) {
        FILE *dicFile = fopen(dictionaryFile, "r");
        if (dicFile == NULL) {
            perror("Error opening the file");
        }
        toReturn = (char **)malloc(MAX_NUMBER_OF_WORDS * sizeof(char*));
        if (toReturn == NULL) {
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < MAX_NUMBER_OF_WORDS; i++) {
            toReturn[i] = (char *)malloc(MAX_NUMBER_OF_WORDS * sizeof(char));
            if (toReturn[i] == NULL) {
                perror("Memory allocation failed PART2");
                exit(EXIT_FAILURE);
            }
        }
        int index = 0;
        char line[MAX_NUMBER_OF_WORDS];
        while (fgets(line, sizeof(line), dicFile) != NULL) {
            size_t length = strlen(line);
            if (line[length - 1] == '\n') {
                line[length - 1] = '\0';
            }
            strcpy(toReturn[index], line);
            index++;
        }
        fclose(dicFile);
    } else {
        FILE *file;
        //char pathName[200];
        //snprintf(pathName, sizeof(pathName), "/proc/%s/stat", PID);
        //snprintf(pathName, sizeof(pathName), "/usr/share/dict/%s", "words");
        file = fopen(dictionaryFile, "r");
        if (file == NULL) {
            perror("Error opening the dictionary file given the path");
            exit(EXIT_FAILURE);
        }
        int numberOfWords = 0;
        char entireLine[MAX_NUMBER_OF_WORDS];
        while (fgets(entireLine, sizeof(entireLine), file) != NULL) {
            size_t length = strlen(entireLine);
            if (entireLine[length - 1] == '\n') {
                entireLine[length - 1] = '\0';
            }
            char *newWord = strdup(entireLine);
            toReturn = realloc(toReturn, (numberOfWords + 1) * sizeof(char *));
            strcpy(toReturn[numberOfWords], newWord);
            numberOfWords++;
        }
        fclose(file);
    }
    return toReturn;

}

void addToConnectionBuffer(struct connectionCell new_connection) {
    pthread_mutex_lock(&connection_queue_mutex);

    while(connection_queue_size == numberOfCellsInConnectionQueue) {
        pthread_cond_wait(&connection_queue_empty, &connection_queue_mutex);
    }
    connection_queue[connection_queue_size] = new_connection;
    connection_queue_size = connection_queue_size + 1;
    pthread_cond_signal(&connection_queue_full);
    pthread_mutex_unlock(&connection_queue_mutex);
    for(int i = 0; i < connection_queue_size; i++) {
        printf("i = %d, priority = %d, sd = %d\n", i, connection_queue[i].priority, connection_queue[i].socketDescriptor);
    }
}


// Function to remove connection from the connection queue
struct connectionCell removeFromConnectionQueue() {
    struct connectionCell connect_info;
    /*
    Lock the connect queue
    remove the connection information from the queue
    update the queue size
    unlock the connection queue
    */
    //pthread_mutex_lock(&connection_queue_mutex);
    connect_info = connection_queue[0];
    for(int i = 0; i < connection_queue_size - 1; i++) {
        connection_queue[i] = connection_queue[i+1];
    }
    connection_queue_size = connection_queue_size - 1;
    //pthread_mutex_unlock(&connection_queue_mutex);

    return connect_info;
}



void *loggerThreadFunc(void *args) {
    printf("Entering the loggerThreadFunc\n");
    FILE *file;
    while (1) {
        char *response = NULL;
        pthread_mutex_lock(&log_queue_mutex);
        while(log_queue_size == 0) {
            printf("Entering while loop 2, log_queue_size = %d\n", log_queue_size);
            pthread_cond_wait(&log_queue_full, &log_queue_mutex);
        }
        response = removeFromLogQueue();
        pthread_cond_signal(&log_queue_empty);
        pthread_mutex_unlock(&log_queue_mutex);
        printf("Just before file, value = %s\n", response);
        file = fopen("log.txt", "a");
        if (file == NULL) {
            perror("Error opening the logEntry file");
            exit(EXIT_FAILURE);
        }
        fprintf(file, "%s\n", response);
        fflush(file);
    }
    fclose(file);
    return NULL;
}

void addToLogQueue(char *response) {
    // Lock the log queue
    /*
    Add the log entry to the queue
    update the queue size
    signal waiting logger thread
    unlock the log queue
    */
    printf("Entering adding to Log queue\n");
    pthread_mutex_lock(&log_queue_mutex);

    while (log_queue_size == QUEUE_SIZE) {
        pthread_cond_wait(&log_queue_empty, &log_queue_mutex);
    }
    log_queue[log_queue_size] = response;
    log_queue_size = log_queue_size + 1;
    pthread_cond_signal(&log_queue_full);
    pthread_mutex_unlock(&log_queue_mutex);
} 

char *removeFromLogQueue() {
    char *toReturn;
    //pthread_mutex_lock(&log_queue_mutex);
    toReturn = log_queue[0];
    for (int i = 0; i < log_queue_size - 1; i++) {
        log_queue[i] = log_queue[i+1];
    }
    log_queue_size = log_queue_size - 1;
    

    return toReturn;
}



int getPriority() {
    int randomNumber = 1 + rand() %(10 - 1 + 1);
    return randomNumber;
}

int isTheWordInDictionary(char *buffer) {
    //printf("length of param = %lu\n", strlen(buffer));
    for (int i = 0; i < MAX_NUMBER_OF_WORDS; i++) {
        //printf("i = %d, word = %s\n, lengthOfWord = %lu\n", i, dictionaryWords[i], strlen(dictionaryWords[i]));
        if (strcmp(buffer, dictionaryWords[i]) == 0) {
            printf("Entering matched\n");
            return 1;
        }
    }
    return 0;
}

// void processLogEntry(struct logCell logEntry) {

    
// }

struct connectionCell getClientWithHighestPriority() {
    printf("%s", "inside highest priority\n");
    int maxPriority = 0; 
    int maxPriorityIndex = -1;
    struct connectionCell connect_info;

    pthread_mutex_lock(&connection_queue_mutex);
    for(int i = 0; i < connection_queue_size; i++) {
        if (connection_queue[i].priority > maxPriority) {
            maxPriority = connection_queue[i].priority;
            maxPriorityIndex = i;
        }
    }
    if (maxPriorityIndex != -1) {
        connect_info = connection_queue[maxPriorityIndex];
        for (int i = maxPriorityIndex; i < connection_queue_size - 1; i++) {
            connection_queue[i] = connection_queue[i+1];
        }
        connection_queue_size = connection_queue_size - 1;
    } else {
        connect_info.socketDescriptor = -1;
        connect_info.priority = -1;

    }
    pthread_mutex_unlock(&connection_queue_mutex);
    return connect_info;
}

int checkIfInteger(char *str) {
    if (str == NULL || *str == '\0') {
        return 0;
    }
    char *endptr;
    strtol(str, &endptr, 10);
    // If strtol successfully parses the entire string, and the first character is not the null terminator,
    // then the string represents a valid integer
    if (*endptr == '\0') {
        return 1;
    } else {
        return 0;
    }

}

char *removeSpace(char *str) {
    int lengthOfString = strlen(str);
    int j = 0;
    char *toReturn = (char *)malloc(lengthOfString + 1);
    
    for(int i = 0; i < lengthOfString; i++) {
        if (!isspace((unsigned char)str[i])) {
            toReturn[j] = str[i];
            j++;
        }
    }
    toReturn[j] = '\0';
    return toReturn;
}



