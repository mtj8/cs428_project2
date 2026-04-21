#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <sstream>
#include <thread>

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

    sockaddr_in servAddr;
    bzero((char*)&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);
    
    // client socket
    int clientSD = socket(AF_INET, SOCK_DGRAM, 0);
    if(clientSD < 0)
    {
        cerr << "Socket error!" << endl;
        exit(EXIT_FAILURE);
    }

    timeval t;
    t.tv_sec = 1;
    t.tv_usec = 0;
    if (setsockopt(clientSD, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t)) < 0) {
        cerr << "Failed to set timeout" << endl;
        exit(EXIT_FAILURE);
    }

    int total_rtts = 0, failed_rtts = 0;
    long long max_rtt = 0, avg_rtt = 0;
    long long min_rtt = 1000000000;
    long long total_time = 0;

    int seq = 1;
    char ping[1024];
    char msg[1024];

    socklen_t len = sizeof(servAddr);

    // for 3 minutes, every 2 seconds:
    auto start = chrono::steady_clock::now();

    for (int i = 0 ; i < 90; i++) {

        auto send_time_point = chrono::steady_clock::now();
        auto send_time = send_time_point.time_since_epoch();
        auto send_ns = chrono::duration_cast<chrono::nanoseconds>(send_time).count();
        
        // print ping message
        stringstream now_ss;
        memset(&ping, 0, sizeof(ping));
        now_ss << "ping," << seq << "," << send_ns;
        string now_str = now_ss.str();
        strncpy(ping, now_str.c_str(), sizeof(ping) - 1);

        cout << "[CLIENT] Sending " << now_str << "..." << endl;
        // send using UDP to server, start a one-second timer
        int sent_bytes = sendto(clientSD, (const char *)&ping, strlen(ping), MSG_CONFIRM, (const struct sockaddr *)&servAddr, sizeof(servAddr));
        cout << "[CLIENT] Message sent to server." << endl;

        // if nothing comes in one second, print timeout
        // if something comes, print response, calculate RTT, and print it out
        memset(&msg, 0, sizeof(msg));
        int rec_bytes = recvfrom(clientSD, msg, 1024, MSG_WAITALL, (struct sockaddr *)&servAddr, &len);
        if (rec_bytes < 0) {
            cout << "[CLIENT] Client UDP Pinger Timed Out" << endl;
            failed_rtts++;
        }
        else {

            auto rec_time = chrono::steady_clock::now().time_since_epoch();
            auto rtt =  rec_time - send_time;
            auto rtt_ns = chrono::duration_cast<chrono::nanoseconds>(rtt).count();
            
            // parse and create echo
            string echo(msg);
            stringstream echo_ss(msg);
            string type, echo_seq, timestamp;

            getline(echo_ss, type, ',');
            getline(echo_ss, echo_seq, ',');
            getline(echo_ss, timestamp, ',');

            cout << "[CLIENT] Received " << msg << endl;
            cout << "[CLIENT] RTT: " << rtt_ns << " nanoseconds" << endl;

            if (stoi(echo_seq) != seq) {
                cout << "[CLIENT] Sequence numbers don't match. Skipping RTT calculation..." << endl;
            }
            else {
                if (rtt_ns > max_rtt) {
                    max_rtt = rtt_ns;
                }
                if (rtt_ns < min_rtt) {
                    min_rtt = rtt_ns;
                }

                total_time = total_time + rtt_ns;
            }
        }
        total_rtts++;

        // (i think the above part can be done with the dgram timeout, while everything else is timer)

        // sleep until the two seconds is done then go back to the top
        seq++;
        std::this_thread::sleep_until(send_time_point + 2000ms);
    }
    auto end = chrono::steady_clock::now();

    // auto three_minutes = chrono::duration_cast<chrono::seconds>(end - start).count();
    // cout << "[CLIENT] Time elapsed: " << three_minutes << endl;

    // print RTT statistics 
    cout << "[CLIENT] RTT statistics:" << endl;
    cout << "         Minimum RTT: " << min_rtt << " nanoseconds" << endl;
    cout << "         Maximum RTT: " << max_rtt << " nanoseconds" << endl;
    cout << "         Total Successful RTTs: " << total_rtts - failed_rtts << endl;
    cout << "         Packet loss %: " << (float(failed_rtts) / float(total_rtts)) * 100 << endl;
    cout << "         Average RTT: " << total_time / (total_rtts - failed_rtts) << " nanoseconds" << endl;

    return EXIT_SUCCESS;
}