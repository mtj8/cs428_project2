#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>

using namespace std;

int main(int argc, char *argv[]) {
    // no connection needed in UDP

    // create socket
    if(argc != 2)
    {
        cerr << "Usage: port" << endl;
        exit(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);

    sockaddr_in servAddr, cliAddr;
    bzero((char*)&servAddr, sizeof(servAddr));
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

    int total_rtts, failed_rtts;
    int min_rtt, max_rtt, avg_rtt;

    int seq;
    char ping[1024];
    char msg[1024];

    // for 3 minutes, every 2 seconds:
    for (int i = 0 ; i < 90; i++) {
        socklen_t len;
        // print ping message
        auto now = chrono::steady_clock::now();
        string now_str = "ping," + seq + format("{:%Y-%m-%d %H:%M:%S}", now);
        // send using UDP to server, start a one-second timer

        // if nothing comes in one second, print timeout

        // if something comes, print response, calculate RTT, and print it out

        // (i think the above part can be done with the dgram timeout, while everything else is timer)

        // sleep until the two seconds is done then go back to the top

    }

    // print RTT statistics 
}