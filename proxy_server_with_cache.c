#include "proxy_parse.h"  // Include the header file for parsing HTTP requests
#include <stdio.h>        // Standard input/output library
#include <stdlib.h>       // Standard library for memory allocation and conversions
#include <string.h>       // String manipulation functions
#include <sys/types.h>    // Data types used in system calls
#include <sys/socket.h>   // Socket programming functions
#include <netinet/in.h>   // Internet address family
#include <netdb.h>        // Definitions for network database operations
#include <arpa/inet.h>    // Definitions for internet operations
#include <unistd.h>       // Standard symbolic constants and types
#include <fcntl.h>        // File control options
#include <time.h>         // Time-related functions
#include <sys/wait.h>     // Declarations for waiting
#include <errno.h>        // Error number definitions
#include <pthread.h>      // POSIX threads library for multithreading
#include <semaphore.h>     // POSIX semaphore library for synchronization
#include <time.h>         // Time-related functions 

// Constants for the proxy server
#define MAX_BYTES 4096         // Maximum allowed size of request/response
#define MAX_CLIENTS 400        // Maximum number of client requests served at a time
#define MAX_SIZE 200*(1<<20)   // Size of the cache (200 MB)
#define MAX_ELEMENT_SIZE 10*(1<<20)  // Maximum size of an element in cache (10 MB)

// Structure to represent a cache element
typedef struct cache_element cache_element;
struct cache_element {
    char* data;                // Stores the response data
    int len;                   // Length of data
    char* url;                 // URL of the request
    time_t lru_time_track;     // Timestamp of last access (for LRU implementation)
    cache_element* next;       // Pointer to the next element in the cache
};

// Function prototypes for cache operations
cache_element* find(char* url);
int add_cache_element(char* data, int size, char* url);
void remove_cache_element();

// Global variables
int port_number = 8080;        // Default port for the proxy server
int proxy_socketId;            // Socket descriptor of the proxy server
pthread_t tid[MAX_CLIENTS];    // Array to store thread IDs of clients
sem_t seamaphore;              // Semaphore for managing client requests queue
pthread_mutex_t lock;          // Mutex for thread-safe access to the cache

cache_element* head;           // Pointer to the head of the cache
int cache_size;                // Current size of the cache

// Function to send error messages to the client
int sendErrorMessage(int socket, int status_code)
{
    char str[1024];
    char currentTime[50];
    time_t now = time(0);

    struct tm data = *gmtime(&now);
    strftime(currentTime, sizeof(currentTime), "%a, %d %b %Y %H:%M:%S %Z", &data);

    switch(status_code)
    {
        case 400:
            snprintf(str, sizeof(str), "HTTP/1.1 400 Bad Request\r\nContent-Length: 95\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Request</H1>\n</BODY></HTML>", currentTime);
            printf("Sending error response: 400 Bad Request\n");
            send(socket, str, strlen(str), 0);
            break;

        case 403:
            snprintf(str, sizeof(str), "HTTP/1.1 403 Forbidden\r\nContent-Length: 112\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H1>403 Forbidden</H1><br>Permission Denied\n</BODY></HTML>", currentTime);
            printf("Sending error response: 403 Forbidden\n");
            send(socket, str, strlen(str), 0);
            break;

        case 404:
            snprintf(str, sizeof(str), "HTTP/1.1 404 Not Found\r\nContent-Length: 91\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>404 Not Found</H1>\n</BODY></HTML>", currentTime);
            printf("Sending error response: 404 Not Found\n");
            send(socket, str, strlen(str), 0);
            break;

        case 500:
            snprintf(str, sizeof(str), "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 115\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H1>500 Internal Server Error</H1>\n</BODY></HTML>", currentTime);
            printf("Sending error response: 500 Internal Server Error\n");
            send(socket, str, strlen(str), 0);
            break;

        case 501:
            snprintf(str, sizeof(str), "HTTP/1.1 501 Not Implemented\r\nContent-Length: 103\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>501 Not Implemented</TITLE></HEAD>\n<BODY><H1>501 Not Implemented</H1>\n</BODY></HTML>", currentTime);
            printf("Sending error response: 501 Not Implemented\n");
            send(socket, str, strlen(str), 0);
            break;

        case 505:
            snprintf(str, sizeof(str), "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 125\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n<BODY><H1>505 HTTP Version Not Supported</H1>\n</BODY></HTML>", currentTime);
            printf("Sending error response: 505 HTTP Version Not Supported\n");
            send(socket, str, strlen(str), 0);
            break;

        default:
            printf("Unknown error code: %d\n", status_code);
            return -1;
    }
    return 1;
}

int connectRemoteServer(char* host_addr, int port_num)
{
    // Creating a socket for the remote server
    int remoteSocket = socket(AF_INET, SOCK_STREAM, 0);
    // Check if the socket was created successfully
    if (remoteSocket < 0)
    {
        fprintf(stderr, "Error: Unable to create socket.\n");
        return -1; // Return -1 if socket creation failed
    }
    
    // Get host by the name or IP address provided
    struct hostent *host = gethostbyname(host_addr);	
    // Check if the host was found
    if (host == NULL)
    {
        fprintf(stderr, "Error: No such host exists: %s\n", host_addr);	
        return -1; // Return -1 if the host does not exist
    }

    // Insert IP address and port number of host in struct `server_addr`
    struct sockaddr_in server_addr;

    // Zero out the server_addr structure
    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // Set address family to IPv4
    server_addr.sin_port = htons(port_num); // Convert port number to network byte order

    // Copy the host's IP address into the server_addr structure
    bcopy((char *)host->h_addr_list[0], (char *)&server_addr.sin_addr.s_addr, host->h_length);

    // Connect to the remote server
    if (connect(remoteSocket, (struct sockaddr*)&server_addr, (socklen_t)sizeof(server_addr)) < 0)
    {
        fprintf(stderr, "Error: Unable to connect to remote server at %s:%d\n", host_addr, port_num); 
        return -1; // Return -1 if connection fails
    }
    
    // Return the socket descriptor for the connected remote server
    return remoteSocket;
}


int handle_request(int clientSocket, ParsedRequest *request, char *tempReq)
{
    // Allocate memory for the buffer to hold the HTTP request
    char *buf = (char*)malloc(sizeof(char) * MAX_BYTES);
    
    // Construct the HTTP GET request
    strcpy(buf, "GET "); // Start with "GET "
    strcat(buf, request->path); // Append the requested path
    strcat(buf, " "); // Add a space
    strcat(buf, request->version); // Append the HTTP version
    strcat(buf, "\r\n"); // End the request with CRLF

    size_t len = strlen(buf); // Get the length of the constructed request

    // Set the "Connection" header to "close"
    if (ParsedHeader_set(request, "Connection", "close") < 0) {
        fprintf(stderr, "Error: Failed to set header key 'Connection' to 'close'.\n");
    }

    // Check if the "Host" header is set; if not, set it
    if (ParsedHeader_get(request, "Host") == NULL) {
        if (ParsedHeader_set(request, "Host", request->host) < 0) {
            fprintf(stderr, "Error: Failed to set 'Host' header key.\n");
        }
    }

    // Unparse the headers and append them to the request buffer
    if (ParsedRequest_unparse_headers(request, buf + len, (size_t)MAX_BYTES - len) < 0) {
        fprintf(stderr, "Error: Failed to unparse headers.\n");
        // return -1; // If this happens, still try to send request without header
    }

    int server_port = 80; // Default Remote Server Port
    if (request->port != NULL) // If a port is specified in the request
        server_port = atoi(request->port); // Convert it to an integer

    // Connect to the remote server
    int remoteSocketID = connectRemoteServer(request->host, server_port);
    if (remoteSocketID < 0) {
        fprintf(stderr, "Error: Unable to connect to remote server at %s:%d.\n", request->host, server_port);
        return -1;
    }

    // Send the constructed request to the remote server
    int bytes_send = send(remoteSocketID, buf, strlen(buf), 0);
    // Arguments for send:
    // - remoteSocketID: the socket file descriptor for the connected remote server
    // - buf: pointer to the buffer containing the data to be sent
    // - strlen(buf): number of bytes to send
    // - 0: flags (0 means no special options)

    bzero(buf, MAX_BYTES); // Clear the buffer for receiving the response

    // Receive the response from the remote server
    bytes_send = recv(remoteSocketID, buf, MAX_BYTES - 1, 0);
    // Arguments for recv:
    // - remoteSocketID: the socket file descriptor for the connected remote server
    // - buf: pointer to the buffer where the received data will be stored
    // - MAX_BYTES - 1: maximum number of bytes to receive (leaving space for null terminator)
    // - 0: flags (0 means no special options)

    char *temp_buffer = (char*)malloc(sizeof(char) * MAX_BYTES); // Temporary buffer for response data
    int temp_buffer_size = MAX_BYTES; // Initial size of the temporary buffer
    int temp_buffer_index = 0; // Index to track the position in the temporary buffer

    // Loop to send data to the client until no more data is received
    while (bytes_send > 0) {
        bytes_send = send(clientSocket, buf, bytes_send, 0);
        // Arguments for send:
        // - clientSocket: the socket file descriptor for the connected client
        // - buf: pointer to the buffer containing the data to be sent
        // - bytes_send: number of bytes to send (the amount received from the server)
        // - 0: flags (0 means no special options)

        // Copy the received data into the temporary buffer
        for (size_t i = 0; i < bytes_send / sizeof(char); i++) {
            temp_buffer[temp_buffer_index] = buf[i]; // Store each byte in the temp buffer
            temp_buffer_index++; // Increment the index
        }
        temp_buffer_size += MAX_BYTES; // Increase the size of the temporary buffer
        temp_buffer = (char*)realloc(temp_buffer, temp_buffer_size); // Reallocate memory for the temporary buffer

        if (bytes_send < 0) { // Check for errors in sending data to the client
            fprintf(stderr, "Error: Failed to send data to client socket.\n");
            break; // Exit the loop on error
        }
        bzero(buf, MAX_BYTES); // Clear the buffer for the next receive

        // Receive more data from the remote server
        bytes_send = recv(remoteSocketID, buf, MAX_BYTES - 1, 0);
    } 

    temp_buffer[temp_buffer_index] = '\0'; // Null-terminate the temporary buffer
    free(buf); // Free the allocated memory for the request buffer
    add_cache_element(temp_buffer, strlen(temp_buffer), tempReq); // Add the response to the cache
    printf("Response handling completed successfully.\n");
    free(temp_buffer); // Free the temporary buffer

    close(remoteSocketID); // Close the connection to the remote server
    return 0; // Return success
}

int checkHTTPversion(char *msg)
{
    int version = -1; // Initialize version to -1 (unknown)

    // Compare the input message with "HTTP/1.1"
    if (strncmp(msg, "HTTP/1.1", 8) == 0) {
        version = 1; // Set version to 1 for HTTP/1.1
    }
    // Compare the input message with "HTTP/1.0"
    else if (strncmp(msg, "HTTP/1.0", 8) == 0) {
        version = 1; // Handle HTTP/1.0 similarly to HTTP/1.1
    }
    else {
        version = -1; // Set version to -1 for unsupported versions
    }

    return version; // Return the determined version
}


void* thread_fn(void* socketNew)
{
    // Wait for the semaphore to ensure thread-safe access
    sem_wait(&seamaphore); 
    int p;
    sem_getvalue(&seamaphore, &p); // Get the current value of the semaphore
    printf("Semaphore value before processing: %d\n", p);

    int* t = (int*)(socketNew);
    int socket = *t; // Socket is the socket descriptor of the connected client
    int bytes_send_client, len; // Bytes transferred

    // Create a buffer of MAX_BYTES for the client
    char *buffer = (char*)calloc(MAX_BYTES, sizeof(char)); 
    bzero(buffer, MAX_BYTES); // Initialize the buffer to zero

    // Receive the request from the client
    bytes_send_client = recv(socket, buffer, MAX_BYTES, 0); 
    // Arguments for recv:
    // - socket: the socket file descriptor for the connected client
    // - buffer: pointer to the buffer where the received data will be stored
    // - MAX_BYTES: maximum number of bytes to receive
    // - 0: flags (0 means no special options)

    // Loop until the complete request is received
    while (bytes_send_client > 0)
    {
        len = strlen(buffer);
        // Loop until "\r\n\r\n" is found in the buffer
        if (strstr(buffer, "\r\n\r\n") == NULL)
        {   
            bytes_send_client = recv(socket, buffer + len, MAX_BYTES - len, 0);
        }
        else
        {
            break; // Exit the loop if the complete request is received
        }
    }

    // Allocate memory for the temporary request
    char *tempReq = (char*)malloc(strlen(buffer) * sizeof(char) + 1);
    // Copy the buffer content to tempReq
    for (size_t i = 0; i < strlen(buffer); i++)
    {
        tempReq[i] = buffer[i];
    }

    // Check for the request in the cache 
    struct cache_element* temp = find(tempReq);

    if (temp != NULL)
    {
        // Request found in cache, send the response to the client
        int size = temp->len / sizeof(char);
        int pos = 0;
        char response[MAX_BYTES];

        while (pos < size)
        {
            bzero(response, MAX_BYTES); // Clear the response buffer
            for (int i = 0; i < MAX_BYTES && pos < size; i++)
            {
                response[i] = temp->data[pos]; // Copy data from cache
                pos++;
            }
            send(socket, response, MAX_BYTES, 0); // Send response to client
        }
        printf("Data retrieved from the cache.\n");
        // printf("%s\n", response); // Uncomment to print the response
    }
    else if (bytes_send_client > 0)
    {
        len = strlen(buffer); 
        // Parsing the request
        ParsedRequest* request = ParsedRequest_create();
        
        // ParsedRequest_parse returns 0 on success and -1 on failure
        if (ParsedRequest_parse(request, buffer, len) < 0) 
        {
            fprintf(stderr, "Error: Parsing failed.\n");
        }
        else
        {   
            bzero(buffer, MAX_BYTES); // Clear the buffer for the next operation
            if (!strcmp(request->method, "GET")) // Check if the method is GET
            {
                if (request->host && request->path && (checkHTTPversion(request->version) == 1))
                {
                    bytes_send_client = handle_request(socket, request, tempReq); // Handle GET request
                    if (bytes_send_client == -1)
                    {   
                        sendErrorMessage(socket, 500); // Send 500 Internal Server Error
                    }
                }
                else
                {
                    sendErrorMessage(socket, 500); // Send 500 Internal Error
                }
            }
            else
            {
                fprintf(stderr, "Error: This code doesn't support any method other than GET.\n");
            }
        }
        // Freeing up the request pointer
        ParsedRequest_destroy(request);
    }
    else if (bytes_send_client < 0)
    {
        perror("Error in receiving from client.\n");
    }
    else if (bytes_send_client == 0)
    {
        printf("Client disconnected!\n");
    }

    shutdown(socket, SHUT_RDWR); // Shutdown the socket for reading and writing
    close(socket); // Close the socket
    free(buffer); // Free the allocated memory for the buffer
    sem_post(&seamaphore); // Release the semaphore

    sem_getvalue(&seamaphore, &p);
    printf("Semaphore value after processing: %d\n", p);
    free(tempReq); // Free the temporary request memory
    return NULL; // Return from the thread function
}


int main(int argc, char * argv[]) {

    int client_socketId, client_len; // client_socketId to store the client socket id
    struct sockaddr_in server_addr, client_addr; // Addresses for client and server

    // Initialize semaphore and mutex for thread synchronization
    sem_init(&seamaphore, 0, MAX_CLIENTS); // Initializing semaphore
    pthread_mutex_init(&lock, NULL); // Initializing mutex for cache

    // Check if the correct number of arguments is provided
    if (argc == 2) {
        port_number = atoi(argv[1]); // Convert port number from string to integer
    } else {
        fprintf(stderr, "Error: Too few arguments. Usage: %s <port_number>\n", argv[0]);
        exit(1);
    }

    printf("Setting Proxy Server Port: %d\n", port_number);

    // Create the proxy socket
    proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_socketId < 0) {
        perror("Error: Failed to create socket.");
        exit(1);
    }

    // Set socket options to allow reuse of the address
    int reuse = 1;
    if (setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
        perror("Error: setsockopt(SO_REUSEADDR) failed.");
    }

    // Zero out the server address structure
    bzero((char*)&server_addr, sizeof(server_addr));  
    server_addr.sin_family = AF_INET; // Set address family to IPv4
    server_addr.sin_port = htons(port_number); // Assign port to the Proxy
    server_addr.sin_addr.s_addr = INADDR_ANY; // Bind to any available address

    // Bind the socket to the address
    if (bind(proxy_socketId, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error: Port is not free.");
        exit(1);
    }
    printf("Binding on port: %d\n", port_number);

    // Start listening for incoming connections
    int listen_status = listen(proxy_socketId, MAX_CLIENTS);
    if (listen_status < 0) {
        perror("Error: Failed to listen for connections.");
        exit(1);
    }

    int i = 0; // Iterator for thread_id (tid) and accepted client socket for each thread
    int Connected_socketId[MAX_CLIENTS]; // Array to store socket descriptors of connected clients

    // Infinite loop for accepting connections
    while (1) {
        bzero((char*)&client_addr, sizeof(client_addr)); // Clear the client address structure
        client_len = sizeof(client_addr); 

        // Accept incoming connections
        client_socketId = accept(proxy_socketId, (struct sockaddr*)&client_addr, (socklen_t*)&client_len);
        if (client_socketId < 0) {
            fprintf(stderr, "Error: Failed to accept connection.\n");
            exit(1);
        } else {
            Connected_socketId[i] = client_socketId; // Store accepted client socket in array
        }

        // Get IP address and port number of the client
        struct sockaddr_in* client_pt = (struct sockaddr_in*)&client_addr;
        struct in_addr ip_addr = client_pt->sin_addr;
        char str[INET_ADDRSTRLEN]; // Buffer for storing IP address
        inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN); // Convert IP address to string
        printf("Client connected with port number: %d and IP address: %s\n", ntohs(client_addr.sin_port), str);

        // Create a thread for each accepted client
        pthread_create(&tid[i], NULL, thread_fn, (void*)&Connected_socketId[i]); 
        i++; 
    }

    close(proxy_socketId); // Close the proxy socket
    return 0; // Return success
}

cache_element* find(char* url) {
    // Checks for the URL in the cache; if found, returns a pointer to the respective cache element or NULL
    cache_element* site = NULL;

    // Lock the mutex to ensure thread-safe access to the cache
    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Cache Lock Acquired for Find: %d\n", temp_lock_val); 

    if (head != NULL) {
        site = head;
        while (site != NULL) {
            if (!strcmp(site->url, url)) {
                printf("LRU Time Track Before: %ld\n", site->lru_time_track);
                printf("\nURL found\n\n");
                // Update the last access time for LRU
                site->lru_time_track = time(NULL);
                printf("LRU Time Track After: %ld\n", site->lru_time_track);
                break;
            }
            site = site->next; // Move to the next cache element
        }       
    } else {
        printf("\nURL not found\n\n");
    }

    // Unlock the mutex after accessing the cache
    temp_lock_val = pthread_mutex_unlock(&lock);
    printf("Cache Lock Unlocked for Find: %d\n", temp_lock_val); 
    return site; // Return the found cache element or NULL
}

void remove_cache_element() {
    // If the cache is not empty, searches for the node with the least lru_time_track and deletes it
    cache_element *p;   // Pointer to the previous cache element
    cache_element *q;   // Pointer to the current cache element
    cache_element *temp; // Cache element to remove

    // Lock the mutex to ensure thread-safe access to the cache
    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Cache Lock Acquired for Remove: %d\n", temp_lock_val); 

    if (head != NULL) { // Cache is not empty
        for (q = head, p = head, temp = head; q->next != NULL; q = q->next) {
            // Iterate through the entire cache and search for the oldest time track
            if ((q->next->lru_time_track) < (temp->lru_time_track)) {
                temp = q->next; // Update the temp pointer to the oldest element
                p = q; // Update the previous pointer
            }
        }
        // Remove the found element from the cache
        if (temp == head) { 
            head = head->next; // Handle the base case
        } else {
            p->next = temp->next; // Bypass the element to be removed
        }
        // Update the cache size
        cache_size -= (temp->len) + sizeof(cache_element) + strlen(temp->url) + 1; 
        free(temp->data); // Free the data of the removed element
        free(temp->url); // Free the URL of the removed element 
        free(temp); // Free the cache element itself
    } 

    // Unlock the mutex after modifying the cache
    temp_lock_val = pthread_mutex_unlock(&lock);
    printf("Cache Lock Unlocked for Remove: %d\n", temp_lock_val); 
}

int add_cache_element(char* data, int size, char* url) {
    // Adds an element to the cache
    // Lock the mutex to ensure thread-safe access to the cache
    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Cache Lock Acquired for Add: %d\n", temp_lock_val);

    int element_size = size + 1 + strlen(url) + sizeof(cache_element); // Size of the new element to be added to the cache
    if (element_size > MAX_ELEMENT_SIZE) {
        // Unlock the mutex if the element size exceeds the maximum allowed size
        temp_lock_val = pthread_mutex_unlock(&lock);
        printf("Cache Lock Unlocked for Add: %d\n", temp_lock_val);
        return 0; // Do not add the element to the cache
    } else {
        // Keep removing elements from the cache until there is enough space to add the new element
        while (cache_size + element_size > MAX_SIZE) {
            remove_cache_element();
        }
        // Allocate memory for the new cache element
        cache_element* element = (cache_element*)malloc(sizeof(cache_element)); 
        element->data = (char*)malloc(size + 1); // Allocate memory for the response data
        strcpy(element->data, data); // Copy the data into the cache element
        element->url = (char*)malloc(1 + (strlen(url) * sizeof(char))); // Allocate memory for the URL
        strcpy(element->url, url); // Copy the URL into the cache element
        element->lru_time_track = time(NULL); // Update the last access time
        element->next = head; // Link the new element to the head of the cache
        element->len = size; // Store the size of the data
        head = element; // Update the head to point to the new element
        cache_size += element_size; // Update the cache size

        // Unlock the mutex after modifying the cache
        temp_lock_val = pthread_mutex_unlock(&lock);
        printf("Cache Lock Unlocked for Add: %d\n", temp_lock_val);
        return 1; // Successfully added the element to the cache
    }
    return 0; // Return 0 if the element could not be added
}




