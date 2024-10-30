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
#include <filesystem>
#include <fstream>
#include "debug.h"
#include "ftp.h"

using std::cerr;
using std::cin;
using std::cout;
using std::endl;
using std::string;

namespace fs = std::filesystem;

void handle_ls(int sockfd, uint8_t *send_buffer, uint8_t *receive_buffer)
{
    send_message(sockfd, Message(LIST_REQUEST_HEADER, NULL, HEADER_LEN), send_buffer);
    Message msg_reply;
    receive_message(sockfd, msg_reply, receive_buffer);
    if (msg_reply.header.get_type() == LIST_REPLY)
    {
        assert(msg_reply.payload != NULL);
        cout << (char *)msg_reply.payload << endl;
    }
    else
    {
        DEBUG_LOG("ls failed");
        exit(114514);
    }
}

void handle_cd(int sockfd, string arg1, uint8_t *send_buffer, uint8_t *receive_buffer)
{
    uint8_t *payload = new uint8_t[arg1.size() + 1];
    strcpy((char *)payload, arg1.c_str());
    send_message(sockfd, Message(CHANGE_DIR_REQUEST_HEADER, payload, HEADER_LEN + strlen((char *)payload) + 1), send_buffer);
    Message msg_reply;
    receive_message(sockfd, msg_reply, receive_buffer);
    if (msg_reply.header == CHANGE_DIR_REPLY_HEADER_1)
    {
        cout << "change dir successfully" << endl;
    }
    else
    {
        cout << "change dir failed" << endl;
    }
}

void handle_get(int sockfd, string arg1, uint8_t *send_buffer, uint8_t *receive_buffer)
{
    uint8_t *payload = new uint8_t[arg1.size() + 1];
    strcpy((char *)payload, arg1.c_str());
    send_message(sockfd, Message(GET_REQUEST_HEADER, payload, HEADER_LEN + strlen((char *)payload) + 1), send_buffer);
    Message msg_reply;
    if (!receive_message(sockfd, msg_reply, receive_buffer))
    {
        DEBUG_LOG("get failed : lost connect during receiving reply");
        exit(114514);
    }

    if (msg_reply.header == GET_REPLY_HEADER_1)
    {
        cout << "get success, now downloading" << endl;
    }
    else
    {
        cout << "get failed : file not exists or is not a regular file" << endl;
        return;
    }

    Message msg_data;
    if (!receive_message(sockfd, msg_data, receive_buffer))
    {
        DEBUG_LOG("get failed : lost connect during receiving data");
        exit(114514);
    }

    if (msg_data.header.get_type() == FILE_DATA)
    {
        cout << "get success : data downloaded" << endl;
        fs::path file_name = arg1;
        if (fs::exists(file_name))
        {
            cout << "file already exists, overwriting" << endl;
        }
        else
        {
            cout << "file not exists, creating" << endl;
        }

        std::ofstream fileIO(file_name, std::ios::out | std::ios::trunc);
        if (!fileIO)
        {
            DEBUG_LOG("creating file failed");
            exit(114514);
        }
        else
        {
            // pay attention to len and '\0'
            fileIO.write((char *)msg_data.payload, msg_data.header.get_payload_len());
            if (!fileIO)
            {
                DEBUG_LOG("writing file failed");
                exit(114514);
            }
            else
            {
                cout << "file written" << endl;
            }
            fileIO.close();
        }
    }
    else
    {
        DEBUG_LOG("get failed");
        exit(114514);
    }
}

void handle_put(int sockfd, string arg1, uint8_t *send_buffer, uint8_t *receive_buffer)
{
    if (!fs::exists(arg1))
    {
        cout << "file not exists" << endl;
        return;
    }

    uint8_t *payload = new uint8_t[arg1.size() + 1];
    strcpy((char *)payload, arg1.c_str());
    send_message(sockfd, Message(PUT_REQUEST_HEADER, payload, HEADER_LEN + strlen((char *)payload) + 1), send_buffer);
    Message msg_reply;
    if (!receive_message(sockfd, msg_reply, receive_buffer))
    {
        DEBUG_LOG("put failed : lost connect during receiving reply");
        exit(114514);
    }

    if (msg_reply.header.get_type() == PUT_REPLY)
    {
        cout << "put success, now uploading" << endl;
    }
    else
    {
        DEBUG_LOG("put failed");
        exit(114514);
    }

    fs::path file_name = arg1;
    std::ifstream fileIO(file_name, std::ios::in | std::ios::binary);
    if (!fileIO)
    {
        DEBUG_LOG("open file failed");
        exit(114514);
    }
    else
    {
        fileIO.seekg(0, std::ios::end);
        std::streamsize file_size = fileIO.tellg();
        fileIO.seekg(0, std::ios::beg);
        uint8_t *file_content = new uint8_t[file_size];
        fileIO.read((char *)file_content, file_size);
        if (!fileIO)
        {
            DEBUG_LOG("read file failed");
            exit(114514);
        }
        else
        {
            Message msg(FILE_DATA_HEADER, file_content, HEADER_LEN + file_size);
            send_message(sockfd, msg, send_buffer);
            cout << "file sent" << endl;
        }
        fileIO.close();
    }
}

void handle_sha256(int sockfd, string arg1, uint8_t *send_buffer, uint8_t *receive_buffer)
{
    uint8_t *payload = new uint8_t[arg1.size() + 1];
    strcpy((char *)payload, arg1.c_str());
    send_message(sockfd, Message(SHA256_REQUEST_HEADER, payload, HEADER_LEN + strlen((char *)payload) + 1), send_buffer);
    Message msg_reply;
    receive_message(sockfd, msg_reply, receive_buffer);
    if (msg_reply.header == SHA256_REPLY_HEADER_1)
    {
        cout << "sha256 reply received" << endl;
    }
    else
    {
        cout << "sha256 failed : file not exists or is not a regular file" << endl;
        return;
    }

    Message msg_data;
    if (!receive_message(sockfd, msg_data, receive_buffer))
    {
        DEBUG_LOG("sha256 failed : lost connect during receiving data");
        exit(114514);
    }

    if (msg_data.header.get_type() == FILE_DATA)
    {
        cout << "sha256 success : data downloaded" << endl;
        cout << "sha256 : " << (char *)msg_data.payload << endl;
    }
    else
    {
        DEBUG_LOG("sha256 failed");
        exit(114514);
    }
}