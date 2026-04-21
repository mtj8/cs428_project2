#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include<vector>
using namespace std;

int server_sd;
volatile bool alive = true; // global bool for server loop (need for SIGINT) (https://stackoverflow.com/questions/4437527/why-do-we-use-the-volatile-keyword)

// thread handling
vector<pthread_t> threads;


// SIGINT shutdown function https://en.cppreference.com/w/cpp/utility/program/signal.html
void shutdown_handler(int sig) {
    alive = false;
    shutdown(server_sd, SHUT_RDWR);
}

// serve pinger thread
void* serve_pinger(void* arg) {
    int port = (int) (intptr_t) arg;

    // server buffer
    char msg[1024];
    char echo[1024];

    // setup server UDP socket
    // notice the use of SOCK_DGRAM for UDP
    sockaddr_in servAddr, cliAddr;
    bzero((char*)&servAddr, sizeof(servAddr));
    bzero((char*)&cliAddr, sizeof(cliAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);
    // server socket descriptor
    int serverSD = socket(AF_INET, SOCK_DGRAM, 0);
    if(serverSD < 0)
    {
        cerr << "[SERVER] Pinger socket error!" << endl;
        return NULL;
    }

    // bind socket
    int bindSock = bind(serverSD, (const struct sockaddr *)&servAddr, sizeof(servAddr));
    if(bindSock < 0)
    {
        cerr << "[SERVER] Pinger bind error!" << endl;
        close(serverSD);
        return NULL;
    }

    // exit thread if no ping received for 30 seconds
    timeval timeout;
    timeout.tv_sec = 30;
    timeout.tv_usec = 0;
    setsockopt(serverSD, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    socklen_t len = sizeof(cliAddr);
    while(1)
    {
        cout << "[SERVER] Wait for client ping..." << endl;
        memset(&msg, 0, sizeof(msg));
        int bytes = recvfrom(serverSD, (char *)&msg, sizeof(msg), MSG_WAITALL, (struct sockaddr *)&cliAddr, &len);

        if (bytes < 0)
        {
            cout << "[SERVER] Server UDP Pinger Timed Out" << endl;
            close(serverSD);
            return NULL;
        }

        // generate a random number between 0-99
        // if random number less than 30, we consider the received message lost and do not respond
        srand((unsigned)time(NULL));
        int random = rand() % 100;
        cout << "Random: " << random << endl;
        if(random < 30)
        {
            continue;
        }

        // otherwise, message not lost
        cout << "[SERVER] Pinger client message: " << msg << endl;

        // parse and create echo
        string ping(msg);
        stringstream ping_ss(ping);
        string type, seq, timestamp;

        getline(ping_ss, type, ',');
        getline(ping_ss, seq, ',');
        getline(ping_ss, timestamp, ',');

        auto echo_time = chrono::steady_clock::now().time_since_epoch();
        auto echo_ns = chrono::duration_cast<chrono::nanoseconds>(echo_time).count();

        stringstream echo_ss;
        echo_ss << "echo," << seq << "," << echo_ns;
        string echo_str = echo_ss.str();

        memset(&echo, 0, sizeof(echo));
        strncpy(echo, echo_str.c_str(), sizeof(echo) - 1);

        // send the server response message
        bytes = sendto(serverSD, (const char *)&echo, strlen(echo), MSG_CONFIRM, (const struct sockaddr *)&cliAddr, sizeof(cliAddr));
        cout << "[SERVER] Pinger response sent." << endl;
    }

    close(serverSD);
    return NULL;
}

// serve client thread
void* serve_client(void* arg) {
    int client_sd = (int) (intptr_t) arg;

    // keeping timeout here so threads exit properly on shutdown
    timeval timeout;
    timeout.tv_sec = 30;
    timeout.tv_usec = 0;
    setsockopt(client_sd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)); // https://pubs.opengroup.org/onlinepubs/009695099/functions/setsockopt.html

    // every thread gets its own buffer now because i don't want to deal with async for that
    char buf[4096] = {0};
    string res, extension, content_type;

    int received = recv(client_sd, (char*)&buf, sizeof(buf), 0);
    if (received <= 0) {
        cerr << "[ERROR] Client timed out." << endl;
        close(client_sd);
        return NULL;
    }

    cout << "[SERVER] Client message received: " << buf << endl;

    // read first line specifically for the file we want
    string request = (string)buf;
    string request_line = request.substr(0, request.find("\r\n"));

    string method, filepath, ver;
    stringstream ss(request_line);

    ss >> method >> filepath >> ver;

    // check for file in server
    auto dot_pos = filepath.rfind('.');
    if (dot_pos == string::npos) { // no "." means we're not looking for a file
        res = "HTTP/1.1 404 Not Found\r\n\r\n404 Not Found";
        send(client_sd, res.c_str(), res.size(), 0);
        close(client_sd);
        return NULL;
    }
    extension = filepath.substr(dot_pos); // everything after the "."
    filepath = "." + filepath;
    
    fstream file;
    file.open(filepath, ios::in | ios::binary);

    // not found
    if(!file) {
        cout << "[SERVER] File not found." << endl;
        res = "HTTP/1.1 404 Not Found\r\n\r\n404 Not Found";
        send(client_sd, res.c_str(), res.size(), 0);
        close(client_sd);
        return NULL;
    }
    
    // checking supported extensions
    if(extension == ".html") {
        content_type = "text/html; charset=utf-8";
    }
    else if(extension == ".jpg" || extension == ".jpeg") {
        content_type = "image/jpeg";
    }
    else if (extension == ".pdf") {
        content_type = "application/pdf";
    }
    else {
        cout << "[SERVER] Filetype not supported." << endl;
        res = "HTTP/1.1 404 Not Found\r\n\r\n404 Not Found";
        send(client_sd, res.c_str(), res.size(), 0);
        file.close();
        close(client_sd);
        return NULL;
    }
    
    // read file https://cplusplus.com/doc/tutorial/files/
    file.seekg(0, ios::end);
    int file_size = file.tellg();
    file.seekg(0, ios::beg);

    char* file_buf = new char[file_size]; // use heap instead of stack since the stack segfaults
    file.read(file_buf, file_size);

    // send file and appropriate response, go back to top and wait for another request
    string header = "HTTP/1.1 200 OK\r\n"
                    "Content-Type: " + content_type + "\r\n"
                    "Content-Length: " + to_string(file_size) + "\r\n\r\n";
    
    send(client_sd, header.c_str(), header.size(), 0);
    send(client_sd, file_buf, file_size, 0);

    delete[] file_buf;
    close(client_sd);

    return NULL;
}

int main(int argc, char* argv[]) {
    // listen for shutdown
    signal(SIGINT, shutdown_handler);

    // get port
    if (argc != 2) {
        cerr << "[ERROR] Please specify a port." << endl;
        exit(0);
    }

    int port = atoi(argv[1]);

    // set up socket
    sockaddr_in serv_addr;
    bzero((char*)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    server_sd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sd < 0) {
        cerr << "[ERROR] Server socket could not be created." << endl;
        exit(0);
    }
    
    // allow for reusing the port when terminating (so i dont have to wait)
    int reuse = 1;
    setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // bind socket
    int binded = bind(server_sd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (binded < 0) {
        cerr << "[ERROR] Server socket could not be binded." << endl;
        exit(0);
    }

    // listen for connection
    listen(server_sd, 10);

    // create UDP pinger thread
    pthread_t pinger_thread;
    pthread_create(&pinger_thread, NULL, serve_pinger, (void*)(intptr_t)port);

    // when connection request:
    sockaddr_in new_socket_addr;
    socklen_t new_socket_addr_size = sizeof(new_socket_addr);
    int client_sd;

    // init needed stuff for requests
    string content_type, res;

    while(alive) {
        // open connection with incoming clients
        cout << "[SERVER] Waiting for client connection... " << endl;
        client_sd = accept(server_sd, (sockaddr* )&new_socket_addr, &new_socket_addr_size);
        if (client_sd < 0) {
            cerr << "[ERROR] Client request could not be accepted." << endl;
            continue;
        }
        cout << "[SERVER] Client connected to server." << endl;

        // open a new thread for each client connection
        pthread_t thread;
        pthread_create(&thread, NULL, serve_client, (void*) (intptr_t) client_sd); // intptr_t to suppress warnings https://stackoverflow.com/questions/19527965/cast-to-pointer-from-integer-of-different-size-pthread-code

        // add thread to vector for joining on sigint
        threads.push_back(thread);
    }

    // shutdown
    cout << "[SERVER] CTRL+C received from user, shutting down server..." << endl;

    // join all threads to shut down properly (?)
    for (pthread_t thread : threads) {
        pthread_join(thread, NULL);
    }
    pthread_join(pinger_thread, NULL);
    close(server_sd);

    return 0;
}