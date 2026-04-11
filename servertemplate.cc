#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

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

        // TODO: prepare the server response

        // send the server response message
        bytes = sendto(serverSD, (const char *)&msg, strlen(msg), MSG_CONFIRM, (const struct sockaddr *)&cliAddr, sizeof(cliAddr));
        cout << "Response sent." << endl;
    }

    // close socket descriptor
    close(serverSD);

    return EXIT_SUCCESS;
}

