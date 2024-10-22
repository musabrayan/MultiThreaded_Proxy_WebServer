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

## Proxy Server Architecture

The following diagram illustrates the architecture of the proxy server:

<div align="center">
    <svg viewBox="0 0 800 600" xmlns="http://www.w3.org/2000/svg">
        <!-- Background -->
        <rect width="800" height="600" fill="#f5f5f5"/>
        
        <!-- Client Section -->
        <g transform="translate(50,100)">
            <!-- Multiple clients -->
            <rect x="0" y="0" width="60" height="40" rx="5" fill="#FF9999"/>
            <rect x="10" y="10" width="60" height="40" rx="5" fill="#FF8888"/>
            <rect x="20" y="20" width="60" height="40" rx="5" fill="#FF7777"/>
            <text x="40" y="45" text-anchor="middle" fill="#333">Clients</text>
            
            <!-- Client request arrows -->
            <path d="M90,40 L180,40" stroke="#666" stroke-width="2" marker-end="url(#arrowhead)"/>
            <text x="135" y="30" text-anchor="middle" fill="#666" font-size="12">HTTP Requests</text>
        </g>

        <!-- Proxy Server Section -->
        <g transform="translate(230,50)">
            <!-- Main proxy container -->
            <rect x="0" y="0" width="300" height="400" rx="10" fill="#E8E8E8" stroke="#333" stroke-width="2"/>
            <text x="150" y="30" text-anchor="middle" font-weight="bold">Proxy Server</text>

            <!-- Thread Pool -->
            <g transform="translate(20,50)">
                <rect x="0" y="0" width="120" height="80" fill="#AEC6CF" rx="5"/>
                <text x="60" y="45" text-anchor="middle">Thread Pool</text>
                <text x="60" y="65" text-anchor="middle" font-size="12">(Max 400 Clients)</text>
            </g>

            <!-- Cache System -->
            <g transform="translate(160,50)">
                <rect x="0" y="0" width="120" height="80" fill="#98FB98" rx="5"/>
                <text x="60" y="35" text-anchor="middle">Cache</text>
                <text x="60" y="55" text-anchor="middle" font-size="10">(200MB Max)</text>
                <text x="60" y="70" text-anchor="middle" font-size="10">LRU Policy</text>
            </g>

            <!-- Request Handler -->
            <g transform="translate(20,160)">
                <rect x="0" y="0" width="260" height="60" fill="#DDA0DD" rx="5"/>
                <text x="130" y="35" text-anchor="middle">Request Handler</text>
            </g>

            <!-- Error Handler -->
            <g transform="translate(20,240)">
                <rect x="0" y="0" width="120" height="60" fill="#FFB6C1" rx="5"/>
                <text x="60" y="35" text-anchor="middle">Error Handler</text>
            </g>

            <!-- Connection Manager -->
            <g transform="translate(160,240)">
                <rect x="0" y="0" width="120" height="60" fill="#87CEEB" rx="5"/>
                <text x="60" y="35" text-anchor="middle">Connection</text>
                <text x="60" y="50" text-anchor="middle">Manager</text>
            </g>

            <!-- Synchronization -->
            <g transform="translate(20,320)">
                <rect x="0" y="0" width="260" height="60" fill="#F0E68C" rx="5"/>
                <text x="130" y="25" text-anchor="middle">Synchronization</text>
                <text x="130" y="45" text-anchor="middle" font-size="12">Mutex & Semaphores</text>
            </g>
        </g>

        <!-- Remote Servers Section -->
        <g transform="translate(600,100)">
            <!-- Multiple servers -->
            <rect x="0" y="0" width="80" height="40" rx="5" fill="#90EE90"/>
            <rect x="10" y="10" width="80" height="40" rx="5" fill="#98FB98"/>
            <rect x="20" y="20" width="80" height="40" rx="5" fill="#A0FFA0"/>
            <text x="60" y="45" text-anchor="middle" fill="#333">Remote</text>
            <text x="60" y="60" text-anchor="middle" fill="#333">Servers</text>
        </g>

        <!-- Arrows between proxy and servers -->
        <path d="M530,150 L590,150" stroke="#666" stroke-width="2" marker-end="url(#arrowhead)"/>
        <path d="M590,180 L530,180" stroke="#666" stroke-width="2" marker-end="url(#arrowhead)"/>
        <text x="560" y="140" text-anchor="middle" fill="#666" font-size="12">Forwards</text>
        <text x="560" y="210" text-anchor="middle" fill="#666" font-size="12">Responses</text>

        <!-- Arrow definitions -->
        <defs>
            <marker id="arrowhead" markerWidth="10" markerHeight="7" refX="9" refY="3.5" orient="auto">
                <polygon points="0 0, 10 3.5, 0 7" fill="#666"/>
            </marker>
        </defs>

        <!-- Legend -->
        <g transform="translate(50,500)">
            <text x="0" y="0" font-weight="bold">Legend:</text>
            <rect x="0" y="10" width="15" height="15" fill="#AEC6CF"/>
            <text x="25" y="22" font-size="12">Thread Management</text>
            
            <rect x="150" y="10" width="15" height="15" fill="#98FB98"/>
            <text x="175" y="22" font-size="12">Cache System</text>
            
            <rect x="300" y="10" width="15" height="15" fill="#DDA0DD"/>
            <text x="325" y="22" font-size="12">Request Processing</text>

            <rect x="450" y="10" width="15" height="15" fill="#F0E68C"/>
            <text x="475" y="22" font-size="12">Synchronization</text>
        </g>
    </svg>
</div>



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
