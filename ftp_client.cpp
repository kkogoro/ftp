#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include "ftp.h"
#include "debug.h"
#include "client_handler.h"

using std::cerr;
using std::cin;
using std::cout;
using std::endl;
using std::string;

int sockfd = -1;
string ip, port;

void print_format()
{
    cout << "Clinet(" << (sockfd == -1 ? "none" : ip + ":" + port) << ")$ ";
}

// TBD:: is BUFFER_SIZE enough?
// Do we really need 2 buffer?
uint8_t send_buffer[BUFFER_SIZE];
uint8_t receive_buffer[BUFFER_SIZE];

int main()
{
    DEBUG_LOG("IN DEBUG MODE");
    // 断开连接后请把sockfd设置为-1
    while (1)
    {
        DEBUG_LOG("in");

        print_format();
        // cerr << "Clinet(" << (sockfd == -1 ? "none" : ip + ":" + port) << ")$ ";
        string command_line,
            opt, arg1, arg2;
        std::getline(cin, command_line);
        std::istringstream iss(command_line);
        iss >> opt;
        if (opt == "open")
        {
            // ip port
            iss >> arg1 >> arg2;
            ip = arg1;
            port = arg2;

            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd == -1)
            {
                DEBUG_LOG("create socket failed");
                exit(114514);
            }

            struct sockaddr_in server_addr;
            server_addr.sin_port = htons(std::stoi(port));
            server_addr.sin_family = AF_INET;
            inet_pton(AF_INET, arg1.c_str(), &server_addr.sin_addr);
            if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
            {
                DEBUG_LOG("connect failed");
                exit(114514);
            }
            send_message(sockfd, Message(OPEN_CONN_REQUEST_HEADER, NULL, HEADER_LEN), send_buffer);
            Message msg_reply;
            receive_message(sockfd, msg_reply, receive_buffer);
            if (msg_reply.header == OPEN_CONN_REPLY_HEADER)
            {
                cout << "connect to " << arg1 << ":" << arg2 << " successfully" << endl;
            }
            else
            {
                DEBUG_LOG("connect failed");
                exit(114514);
            }
        }
        else if (opt == "ls")
        {
            handle_ls(sockfd, send_buffer, receive_buffer);
        }
        else if (opt == "cd")
        {
            iss >> arg1; // DIR
            handle_cd(sockfd, arg1, send_buffer, receive_buffer);
        }
        else if (opt == "get")
        {
            iss >> arg1;
            handle_get(sockfd, arg1, send_buffer, receive_buffer);
        }
        else if (opt == "put")
        {
            iss >> arg1;
            handle_put(sockfd, arg1, send_buffer, receive_buffer);
        }
        else if (opt == "sha256")
        {
            iss >> arg1;
            handle_sha256(sockfd, arg1, send_buffer, receive_buffer);
        }
        else if (opt == "quit")
        {
            // 如有连接则断开连接，回到 open 前的状态；如果已经是 open 前的状态，则关闭 Client。
            if (sockfd == -1)
            {
                cout << "Bye!" << endl;
                return 0;
            }
            else
            {
                send_message(sockfd, Message(QUIT_REQUEST_HEADER, NULL, HEADER_LEN), send_buffer);
                Message msg_reply;
                if (!receive_message(sockfd, msg_reply, receive_buffer))
                {
                    DEBUG_LOG("quit failed : lost connect during receiving reply");
                    exit(114514);
                }

                if (msg_reply.header == QUIT_REPLY_HEADER)
                {
                    close(sockfd);
                    cout << "disconnect from " << ip << ":" << port << endl;
                    ip = "";
                    port = "";
                    sockfd = -1;
                }
                else
                {
                    DEBUG_LOG("quit failed");
                    exit(114514);
                }
            }
        }
        else
        {
            cout << "非法指令" << endl;
        }
    }
    return 0;
}