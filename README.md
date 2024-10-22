# Multi-Threaded HTTP Proxy Server

This project implements a multi-threaded proxy server that efficiently handles client requests while utilizing caching mechanisms to enhance performance.

## Motivation

The primary goals of this project are to:

- Understand the flow of requests from a local machine to a remote server.
- Manage multiple client requests simultaneously.
- Implement a locking mechanism for concurrency control.
- Explore caching concepts and their applications in web browsers.

## Functionality of the Proxy Server

The proxy server serves several purposes:

- **Performance Improvement:** It accelerates the request handling process and alleviates server load.
- **Access Control:** It can restrict users from accessing certain websites.
- **Anonymity:** A well-designed proxy can mask the client's IP address, ensuring anonymity in requests.
- **Security Enhancements:** The proxy can be modified to encrypt requests, preventing unauthorized access to sensitive data.

## Technical Components Used

- **Threading:** To handle multiple requests concurrently.
- **Locks:** For managing access to shared resources.
- **Semaphores:** Used instead of condition variables for thread synchronization, as they do not require parameters for operations like `sem_wait()` and `sem_post()`.
- **Caching:** Implemented using the Least Recently Used (LRU) algorithm.

## Limitations

- **Cache Behavior:** If a URL is accessed by multiple clients, each response is stored separately in the cache. This can lead to incomplete responses when retrieving data.
- **Fixed Cache Size:** Large websites may not be fully cached due to size limitations.

## Architecture Diagram

![{1F4E79C0-D30C-4108-95F3-461577D962B0}](https://github.com/user-attachments/assets/95010d5e-ad22-40a2-8a1b-7667e49f8b44)



### Key Components
- **Thread Pool**: Handles up to 400 concurrent clients
- **Cache System**: 200MB LRU cache for improved performance
- **Request Handler**: Processes incoming client requests
- **Error Handler**: Manages error scenarios
- **Connection Manager**: Handles server connections
- **Synchronization**: Ensures thread safety using mutex and semaphores


## Future Enhancements

This project can be extended in several ways:

- **Multiprocessing:** Implementing multiprocessing can enhance performance through parallel processing.
- **Website Filtering:** Extend the code to allow or block specific types of websites.
- **Support for POST Requests:** Modify the proxy to handle POST requests in addition to GET requests.

## Note

The code is thoroughly commented. For any questions or clarifications, please refer to the comments within the code.

## How to Run the Project

1. Clone the repository:

   ```bash
   https://github.com/musabrayan/MultiThreaded_Proxy_WebServer.git
   ```

2. Navigate to the project directory:

   ```bash
   cd MultiThreadedProxyServerClient
   ```

3. Build the project:

   ```bash
   make all
   ```

4. Start the proxy server:

   ```bash
   ./proxy <port_number>
   ```

5. Open your web browser and navigate to:

   ```
   http://localhost:<port_number>/https://www.cs.princeton.edu/
   ```

## Important Note

This project is designed to run on a Linux machine. Please ensure that your browser cache is disabled for optimal performance.
