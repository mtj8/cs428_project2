#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <sstream>

using namespace std;

int main(int argc, char *argv[])
{
    // get server port number
    if(argc != 2)
    {
        cerr << "Usage: port" << endl;
        exit(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);

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
        cerr << "Socket error!" << endl;
        exit(EXIT_FAILURE);
    }
    
    // bind socket
    int bindSock = bind(serverSD, (const struct sockaddr *)&servAddr, sizeof(servAddr));
    if(bindSock < 0)
    {
        cerr << "Bind error!" << endl;
        exit(EXIT_FAILURE);
    }
    
    socklen_t len;
    while(1)
    {
        cout << "Wait for client message..." << endl;
        memset(&msg, 0, sizeof(msg));
        int bytes = recvfrom(serverSD, (char *)&msg, sizeof(msg), MSG_WAITALL, (struct sockaddr *)&cliAddr, &len);

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
        cout << "Client message: " << msg << endl;

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
        cout << "Response sent." << endl;
    }

    // close socket descriptor
    close(serverSD);

    return EXIT_SUCCESS;
}

