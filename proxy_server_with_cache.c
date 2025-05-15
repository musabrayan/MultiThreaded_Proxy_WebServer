#include "proxy_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

// Constants
#define MAX_BYTES 4096         // Maximum buffer size
#define MAX_CLIENTS 400        // Maximum concurrent clients
#define MAX_SIZE 200*(1<<20)   // Cache size (200 MB)
#define MAX_ELEMENT_SIZE 10*(1<<20)  // Max cache element size (10 MB)

// Cache element structure
typedef struct cache_element cache_element;
struct cache_element {
    char* data;                // Response data
    int len;                   // Data length
    char* url;                 // Request URL
    time_t lru_time_track;     // Last access timestamp
    cache_element* next;       // Next element pointer
};

// Function prototypes
cache_element* find(char* url);
int add_cache_element(char* data, int size, char* url);
void remove_cache_element();

// Global variables
int port_number = 8080;        // Default port
int proxy_socketId;            // Proxy socket descriptor
pthread_t tid[MAX_CLIENTS];    // Thread IDs array
sem_t seamaphore;              // Client queue semaphore
pthread_mutex_t lock;          // Cache access mutex

cache_element* head;           // Cache head pointer
int cache_size;                // Current cache size

// Send HTTP error response to client
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

// Connect to remote server using hostname and port
int connectRemoteServer(char* host_addr, int port_num)
{
    // Create socket for remote server connection
    int remoteSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (remoteSocket < 0)
    {
        fprintf(stderr, "Error: Unable to create socket.\n");
        return -1;
    }
    
    // Resolve hostname to IP address
    struct hostent *host = gethostbyname(host_addr);
    if (host == NULL)
    {
        fprintf(stderr, "Error: No such host exists: %s\n", host_addr);	
        return -1;
    }

    // Set up server address structure
    struct sockaddr_in server_addr;
    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_num);
    bcopy((char *)host->h_addr_list[0], (char *)&server_addr.sin_addr.s_addr, host->h_length);

    // Connect to remote server
    if (connect(remoteSocket, (struct sockaddr*)&server_addr, (socklen_t)sizeof(server_addr)) < 0)
    {
        fprintf(stderr, "Error: Unable to connect to remote server at %s:%d\n", host_addr, port_num); 
        return -1;
    }
    
    return remoteSocket;
}

// Handle HTTP request and forward to remote server
int handle_request(int clientSocket, ParsedRequest *request, char *tempReq)
{
    // Create buffer for HTTP request
    char *buf = (char*)malloc(sizeof(char) * MAX_BYTES);
    
    // Build HTTP GET request
    strcpy(buf, "GET ");
    strcat(buf, request->path);
    strcat(buf, " ");
    strcat(buf, request->version);
    strcat(buf, "\r\n");

    size_t len = strlen(buf);

    // Set Connection header to close
    if (ParsedHeader_set(request, "Connection", "close") < 0) {
        fprintf(stderr, "Error: Failed to set header key 'Connection' to 'close'.\n");
    }

    // Set Host header if not present
    if (ParsedHeader_get(request, "Host") == NULL) {
        if (ParsedHeader_set(request, "Host", request->host) < 0) {
            fprintf(stderr, "Error: Failed to set 'Host' header key.\n");
        }
    }

    // Add headers to request
    if (ParsedRequest_unparse_headers(request, buf + len, (size_t)MAX_BYTES - len) < 0) {
        fprintf(stderr, "Error: Failed to unparse headers.\n");
    }

    // Get server port (default is 80)
    int server_port = 80;
    if (request->port != NULL)
        server_port = atoi(request->port);

    // Connect to remote server
    int remoteSocketID = connectRemoteServer(request->host, server_port);
    if (remoteSocketID < 0) {
        fprintf(stderr, "Error: Unable to connect to remote server at %s:%d.\n", request->host, server_port);
        return -1;
    }

    // Send request to remote server
    int bytes_send = send(remoteSocketID, buf, strlen(buf), 0);
    bzero(buf, MAX_BYTES);

    // Receive initial response from remote server
    bytes_send = recv(remoteSocketID, buf, MAX_BYTES - 1, 0);

    // Allocate buffer for complete response
    char *temp_buffer = (char*)malloc(sizeof(char) * MAX_BYTES);
    int temp_buffer_size = MAX_BYTES;
    int temp_buffer_index = 0;

    // Receive and forward complete response
    while (bytes_send > 0) {
        bytes_send = send(clientSocket, buf, bytes_send, 0);

        // Save response to temp buffer for caching
        for (size_t i = 0; i < bytes_send / sizeof(char); i++) {
            temp_buffer[temp_buffer_index] = buf[i];
            temp_buffer_index++;
        }
        temp_buffer_size += MAX_BYTES;
        temp_buffer = (char*)realloc(temp_buffer, temp_buffer_size);

        if (bytes_send < 0) {
            fprintf(stderr, "Error: Failed to send data to client socket.\n");
            break;
        }
        bzero(buf, MAX_BYTES);

        // Get more data from remote server
        bytes_send = recv(remoteSocketID, buf, MAX_BYTES - 1, 0);
    } 

    temp_buffer[temp_buffer_index] = '\0';
    free(buf);
    
    // Add response to cache
    add_cache_element(temp_buffer, strlen(temp_buffer), tempReq);
    printf("Response handling completed successfully.\n");
    free(temp_buffer);

    close(remoteSocketID);
    return 0;
}

// Validate HTTP version
int checkHTTPversion(char *msg)
{
    if (strncmp(msg, "HTTP/1.1", 8) == 0) {
        return 1;
    }
    else if (strncmp(msg, "HTTP/1.0", 8) == 0) {
        return 1;
    }
    else {
        return -1;
    }
}

// Thread function to handle client requests
void* thread_fn(void* socketNew)
{
    int* t = (int*)(socketNew);
    int socket = *t;
    int bytes_send_client = 0, len = 0;
    char *buffer = NULL;
    char *tempReq = NULL;
    ParsedRequest* request = NULL;
    
    // Wait for semaphore
    if (sem_wait(&seamaphore) != 0) {
        perror("sem_wait failed");
        return NULL;
    }

    int p;
    sem_getvalue(&seamaphore, &p);
    printf("Semaphore value before processing: %d\n", p);

    // Allocate buffer for request
    buffer = (char*)calloc(MAX_BYTES, sizeof(char));
    if (!buffer) {
        perror("Failed to allocate buffer");
        goto cleanup;
    }
    bzero(buffer, MAX_BYTES);

    // Receive client request
    bytes_send_client = recv(socket, buffer, MAX_BYTES, 0);
    
    // Ensure complete request is received
    while (bytes_send_client > 0)
    {
        len = strlen(buffer);
        if (strstr(buffer, "\r\n\r\n") == NULL)
        {   
            bytes_send_client = recv(socket, buffer + len, MAX_BYTES - len, 0);
        }
        else
        {
            break;
        }
    }

    // Process request if data was received
    if (bytes_send_client > 0) {
        // Save original request for cache key
        tempReq = (char*)malloc(strlen(buffer) * sizeof(char) + 1);
        if (!tempReq) {
            perror("Failed to allocate tempReq");
            goto cleanup;
        }
        strcpy(tempReq, buffer);

        // Check if response is in cache
        struct cache_element* temp = find(tempReq);

        if (temp != NULL)
        {
            // Send cached response to client
            int size = temp->len / sizeof(char);
            int pos = 0;
            char response[MAX_BYTES];

            while (pos < size)
            {
                bzero(response, MAX_BYTES);
                for (int i = 0; i < MAX_BYTES && pos < size; i++)
                {
                    response[i] = temp->data[pos];
                    pos++;
                }
                send(socket, response, MAX_BYTES, 0);
            }
            printf("Data retrieved from the cache.\n");
        }
        else 
        {
            // Parse and process new request
            request = ParsedRequest_create();
            if (!request) {
                perror("Failed to create request");
                goto cleanup;
            }

            if (ParsedRequest_parse(request, buffer, len) < 0) {
                fprintf(stderr, "Error: Parsing failed.\n");
                goto cleanup;
            }

            // Handle GET requests
            if (!strcmp(request->method, "GET"))
            {
                if (request->host && request->path && (checkHTTPversion(request->version) == 1))
                {
                    bytes_send_client = handle_request(socket, request, tempReq);
                    if (bytes_send_client == -1)
                    {   
                        sendErrorMessage(socket, 500);
                    }
                }
                else
                {
                    sendErrorMessage(socket, 500);
                }
            }
            else
            {
                fprintf(stderr, "Error: This code doesn't support any method other than GET.\n");
            }
        }
    }
    else if (bytes_send_client < 0)
    {
        perror("Error in receiving from client.\n");
    }
    else if (bytes_send_client == 0)
    {
        printf("Client disconnected!\n");
    }

cleanup:
    // Free resources and close connection
    if (request) ParsedRequest_destroy(request);
    if (buffer) free(buffer);
    if (tempReq) free(tempReq);
    
    shutdown(socket, SHUT_RDWR);
    close(socket);
    
    sem_post(&seamaphore);
    sem_getvalue(&seamaphore, &p);
    printf("Semaphore value after processing: %d\n", p);
    
    return NULL;
}

int main(int argc, char * argv[]) {
    int client_socketId, client_len;
    struct sockaddr_in server_addr, client_addr;

    // Initialize synchronization primitives
    sem_init(&seamaphore, 0, MAX_CLIENTS);
    pthread_mutex_init(&lock, NULL);

    // Parse command line arguments
    if (argc == 2) {
        port_number = atoi(argv[1]);
    } else {
        fprintf(stderr, "Error: Too few arguments. Usage: %s <port_number>\n", argv[0]);
        exit(1);
    }

    printf("Setting Proxy Server Port: %d\n", port_number);

    // Create proxy server socket
    proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_socketId < 0) {
        perror("Error: Failed to create socket.");
        exit(1);
    }

    // Set socket options
    int reuse = 1;
    if (setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
        perror("Error: setsockopt(SO_REUSEADDR) failed.");
    }

    // Configure server address
    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind socket to address
    if (bind(proxy_socketId, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error: Port is not free.");
        exit(1);
    }
    printf("Binding on port: %d\n", port_number);

    // Listen for connections
    int listen_status = listen(proxy_socketId, MAX_CLIENTS);
    if (listen_status < 0) {
        perror("Error: Failed to listen for connections.");
        exit(1);
    }

    int i = 0;
    int Connected_socketId[MAX_CLIENTS];

    // Main server loop
    while (1) {
        bzero((char*)&client_addr, sizeof(client_addr));
        client_len = sizeof(client_addr); 

        // Accept client connection
        client_socketId = accept(proxy_socketId, (struct sockaddr*)&client_addr, (socklen_t*)&client_len);
        if (client_socketId < 0) {
            fprintf(stderr, "Error: Failed to accept connection.\n");
            exit(1);
        } else {
            Connected_socketId[i] = client_socketId;
        }

        // Log client connection details
        struct sockaddr_in* client_pt = (struct sockaddr_in*)&client_addr;
        struct in_addr ip_addr = client_pt->sin_addr;
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN);
        printf("Client connected with port number: %d and IP address: %s\n", ntohs(client_addr.sin_port), str);

        // Create thread to handle client request
        pthread_create(&tid[i], NULL, thread_fn, (void*)&Connected_socketId[i]); 
        i++; 
    }

    close(proxy_socketId);
    return 0;
}

// Find URL in cache
cache_element* find(char* url) {
    cache_element* site = NULL;

    int temp_lock_val = pthread_mutex_lock(&lock);
    if (temp_lock_val != 0) {
        perror("Mutex lock failed");
        return NULL;
    }
    printf("Cache Lock Acquired for Find: %d\n", temp_lock_val); 

    // Search for URL in cache
    if (head != NULL) {
        site = head;
        while (site != NULL) {
            if (!strcmp(site->url, url)) {
                printf("LRU Time Track Before: %ld\n", site->lru_time_track);
                printf("\nURL found\n\n");
                site->lru_time_track = time(NULL);  // Update access time
                printf("LRU Time Track After: %ld\n", site->lru_time_track);
                break;
            }
            site = site->next;
        }       
    }
    
    if (site == NULL) {
        printf("\nURL not found\n\n");
    }

    temp_lock_val = pthread_mutex_unlock(&lock);
    if (temp_lock_val != 0) {
        perror("Mutex unlock failed");
    }
    printf("Cache Lock Unlocked for Find: %d\n", temp_lock_val); 
    
    return site;
}

// Remove least recently used cache element
void remove_cache_element() {
    cache_element *p;
    cache_element *q;
    cache_element *temp;

    // Lock cache for thread safety
    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Cache Lock Acquired for Remove: %d\n", temp_lock_val); 

    if (head != NULL) {
        // Find the oldest element in cache
        for (q = head, p = head, temp = head; q->next != NULL; q = q->next) {
            if ((q->next->lru_time_track) < (temp->lru_time_track)) {
                temp = q->next;
                p = q;
            }
        }
        
        // Remove the element from linked list
        if (temp == head) { 
            head = head->next;
        } else {
            p->next = temp->next;
        }
        
        // Update cache size and free memory
        cache_size -= (temp->len) + sizeof(cache_element) + strlen(temp->url) + 1; 
        free(temp->data);
        free(temp->url);
        free(temp);
    } 

    temp_lock_val = pthread_mutex_unlock(&lock);
    printf("Cache Lock Unlocked for Remove: %d\n", temp_lock_val); 
}

// Add new element to cache
int add_cache_element(char* data, int size, char* url) {
    // Lock cache for thread safety
    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Cache Lock Acquired for Add: %d\n", temp_lock_val);

    int element_size = size + 1 + strlen(url) + sizeof(cache_element);
    
    // Skip if element exceeds maximum size
    if (element_size > MAX_ELEMENT_SIZE) {
        temp_lock_val = pthread_mutex_unlock(&lock);
        printf("Cache Lock Unlocked for Add: %d\n", temp_lock_val);
        return 0;
    } else {
        // Make room for new element if needed
        while (cache_size + element_size > MAX_SIZE) {
            remove_cache_element();
        }
        
        // Create and initialize new cache element
        cache_element* element = (cache_element*)malloc(sizeof(cache_element)); 
        element->data = (char*)malloc(size + 1);
        strcpy(element->data, data);
        element->url = (char*)malloc(1 + (strlen(url) * sizeof(char)));
        strcpy(element->url, url);
        element->lru_time_track = time(NULL);
        element->next = head;
        element->len = size;
        head = element;
        cache_size += element_size;

        temp_lock_val = pthread_mutex_unlock(&lock);
        printf("Cache Lock Unlocked for Add: %d\n", temp_lock_val);
        return 1;
    }
    return 0;
}