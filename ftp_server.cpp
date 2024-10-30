#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <cstdio>
#include <sys/epoll.h>
#include <filesystem>
#include <map>
#include "ftp.h"
#include "debug.h"
#include "server_handler.h"

using std::cerr;
using std::cin;
using std::cout;
using std::endl;
using std::string;
namespace fs = std::filesystem;

const int POLL_SIZE = 21; // to be change into 16
u_int8_t buffer[BUFFER_SIZE];
std::map<int, fs::path> path_map;

int listenfd = -1;
struct sockaddr_in cliaddr, servaddr;
socklen_t clilen = sizeof(cliaddr);

string ip, port;

// 真正的初始地址
const fs::path base_path = fs::current_path();

int main(int argc, char *argv[])
{
    DEBUG_LOG("IN DEBUG MODE");
    if (argc != 3)
    {
        DEBUG_LOG("args error");
        exit(114514);
    }
    ip = argv[1];
    port = argv[2];

    listenfd = socket(AF_INET, SOCK_STREAM, 0); // 申请一个TCP的socket
    if (listenfd == -1)
    {
        DEBUG_LOG("create socket failed");
        exit(114514);
    }
    servaddr.sin_port = htons(std::stoi(port)); // 在23233端口监听 htons是host to network (short)的简称，表示进行大小端表示法转换，网络中一般使用大端法
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &servaddr.sin_addr);
    bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (listen(listenfd, 128) == 0)
    { // 最大连接数为128
        cout << "listening on ";
        printSockaddrIn(servaddr);
        cout << endl;
    }
    else
    {
        DEBUG_LOG("listen failed");
        exit(114514);
    }

    int epfd = epoll_create(128); // 参数在新的Linux kernel中无意义
    struct epoll_event evt;
    evt.events = EPOLLIN;
    evt.data.fd = listenfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &evt);

    struct epoll_event events[POLL_SIZE];

    while (1)
    {
        int nevents = epoll_wait(epfd, events, POLL_SIZE, -1);
        for (int i = 0; i < nevents; ++i)
        {
            int fd = events[i].data.fd;
            if (fd == listenfd)
            {
                int client = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
                if (client == -1)
                {
                    DEBUG_LOG("accept failed");
                    exit(114514);
                }

                cout << "accept a client from ";
                printSockaddrIn(cliaddr);
                cout << endl;

                // 新建连接则重置路径
                fs::current_path(base_path);
                path_map[client] = fs::current_path();

                Message OPEN_CONN_msg;
                receive_message(client, OPEN_CONN_msg, buffer);
                send_message(client, Message(OPEN_CONN_REPLY_HEADER, NULL, HEADER_LEN), buffer);

                evt.events = EPOLLIN;
                evt.data.fd = client;
                epoll_ctl(epfd, EPOLL_CTL_ADD, client, &evt);
            }
            else // 响应客户端的请求
            {
                int client = events[i].data.fd;
                // 开始交互
                Message now_msg;
                fs::current_path(path_map[client]);
                if (receive_message(client, now_msg, buffer) != 0)
                {
                    // delete []now_msg.payload; 改用接口修改payload指针，避免手动释放
                    switch (now_msg.header.get_type())
                    {
                    case LIST_REQUEST:

                        cout << "receive LIST_REQUEST from ";
                        printSockaddrIn(cliaddr);
                        cout << endl;

                        handle_list_request(client, buffer);
                        break;
                    case CHANGE_DIR_REQUEST:

                        cout << "receive CHANGE_DIR_REQUEST from ";
                        printSockaddrIn(cliaddr);
                        cout << endl;
                        handle_change_dir_request(client, (char *)now_msg.payload, buffer, path_map[client]);

                        break;

                    case GET_REQUEST:
                        cout << "receive GET_REQUEST from ";
                        printSockaddrIn(cliaddr);
                        cout << endl;

                        handle_get_request(client, (char *)now_msg.payload, buffer);
                        break;

                    case PUT_REQUEST:
                        cout << "receive PUT_REQUEST from ";
                        printSockaddrIn(cliaddr);
                        cout << endl;

                        handle_put_request(client, (char *)now_msg.payload, buffer);
                        break;
                    case SHA256_REQUEST:
                        cout << "receive SHA256_REQUEST from ";
                        printSockaddrIn(cliaddr);
                        cout << endl;

                        handle_sha256_request(client, (char *)now_msg.payload, buffer);
                        break;
                    case QUIT_REQUEST:
                        cout << "receive QUIT_REQUEST from ";
                        printSockaddrIn(cliaddr);
                        cout << endl;

                        handle_quit_request(client, buffer);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, client, NULL);
                        path_map[client].clear();
                        break;
                    }
                }
            }
        }
    }
    close(listenfd);
    return 0;
}