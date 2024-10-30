#pragma once
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <cassert>
#include "debug.h"

using std::cerr;
using std::cin;
using std::cout;
using std::endl;
using std::string;
const int MAGIC_NUMBER_LENGTH = 6;
const int BUFFER_SIZE = 1 << 22; // 4MB
struct MessageHeader;            // forward declaration

void set_protocol(struct MessageHeader &msg);
void set_type(struct MessageHeader &msg, uint8_t type);
void set_status(struct MessageHeader &msg, uint8_t status);
void set_len(struct MessageHeader &msg, uint32_t len);

const uint8_t protocol_base[MAGIC_NUMBER_LENGTH] = {0xc1, 0xa1, 0x10, 'f', 't', 'p'};

// TODO: 用private保护起来，只暴露接口
struct MessageHeader
{
    // 不用char防止比较时发生整形提升
    uint8_t m_protocol[MAGIC_NUMBER_LENGTH]; /* protocol magic number (6 bytes) */
    uint8_t m_type;                          /* type (1 byte) */
    uint8_t m_status;                        /* status (1 byte) */
    uint32_t m_length;                       /* length (4 bytes) in Big endian*/
    MessageHeader()
    {
        _set_protocol();
        _set_type(0);
        _set_status(0);
        _set_len(0);
    }
    MessageHeader(uint8_t type, uint8_t status, uint32_t len)
    {
        _set_protocol();
        _set_type(type);
        _set_status(status);
        _set_len(len);
    }
    inline void _set_protocol()
    {
        set_protocol(*this);
    }
    inline void _set_type(uint8_t type)
    {
        set_type(*this, type);
    }
    inline void _set_status(uint8_t status)
    {
        set_status(*this, status);
    }
    inline void _set_len(uint32_t len)
    {
        set_len(*this, len);
    }
    uint8_t get_status() const
    {
        return m_status;
    }
    uint8_t get_type() const
    {
        return m_type;
    }

    bool operator==(const MessageHeader &msg) const
    {
        return memcmp(this, &msg, sizeof(MessageHeader)) == 0;
    }
    inline int get_len() const
    {
        return ntohl(m_length);
    }
    inline int get_payload_len() const
    {
        return get_len() - sizeof(MessageHeader);
    }
} __attribute__((packed));

const int HEADER_LEN = sizeof(MessageHeader); // 12
struct Message
{
    MessageHeader header;
    uint8_t *payload; // payload
    Message()
    {
        header = MessageHeader();
        payload = NULL;
    }
    Message(const MessageHeader &header, uint8_t *payload, size_t len)
    {
        this->header = header;
        this->header._set_len(len);
        this->payload = payload;
    }

    void set_payload(uint8_t *payload)
    {
        if (this->payload != NULL)
        {
            delete[] this->payload;
        }
        this->payload = payload;
    }
    ~Message()
    {
        if (payload != NULL)
        {
            delete[] payload;
        }
    }
};

// always "\xc1\xa1\x10ftp"
inline void set_protocol(struct MessageHeader &msg)
{
    memcpy(msg.m_protocol, protocol_base, 6);
    assert(msg.m_protocol[0] == 0xc1);
    assert(msg.m_protocol[1] == 0xa1);
    assert(msg.m_protocol[2] == 0x10);
}

// set type
inline void set_type(struct MessageHeader &msg, uint8_t type)
{
    msg.m_type = type;
}

// status = 0 or 1
inline void set_status(struct MessageHeader &msg, uint8_t status)
{
    msg.m_status = status;
}

// set length, in big endian
inline void set_len(struct MessageHeader &msg, uint32_t len)
{
    msg.m_length = htonl(len);
}

void printSockaddrIn(const struct sockaddr_in &addr)
{
    char ip[INET_ADDRSTRLEN]; // INET_ADDRSTRLEN是ivp4的地址长度宏定义
    inet_ntop(AF_INET, &(addr.sin_addr), ip, INET_ADDRSTRLEN);
    int port = ntohs(addr.sin_port);
    std::cout << ip << ":" << port << std::endl;
}

// send Message by sockfd
void send_message(int sockfd, const Message &msg, uint8_t *buffer)
{
    // send header
    memcpy(buffer, &msg, HEADER_LEN);
    size_t ret = 0;
    while (ret < HEADER_LEN)
    {
        ssize_t b = send(sockfd, buffer + ret, HEADER_LEN - ret, 0);
        if (b == 0)
        {
            DEBUG_LOG("socket closed unexpected during sending");
            exit(114514);
        }
        if (b < 0)
        {
            DEBUG_LOG("send Message failed");
            exit(114514);
        }
        ret += b; // 成功将b个byte塞进了缓冲区
    }
    // send payload
    size_t payload_len = msg.header.get_len() - HEADER_LEN;
    if (payload_len > 0)
    {
        memcpy(buffer, msg.payload, payload_len);
        ret = 0;
        while (ret < payload_len)
        {
            ssize_t b = send(sockfd, msg.payload + ret, payload_len - ret, 0);
            if (b == 0)
            {

                DEBUG_LOG("socket closed unexpected during sending");
                exit(114514);
            }
            if (b < 0)
            {
                DEBUG_LOG("send Message failed");
                exit(114514);
            }
            ret += b; // 成功将b个byte塞进了缓冲区
        }
    }

    DEBUG_LOG("sended");
}

// return 1 if success, 0 if failed
int receive_message(int sockfd, Message &msg, uint8_t *buffer)
{
    size_t ret = 0;
    // receive header
    while (ret < HEADER_LEN)
    {
        ssize_t b = recv(sockfd, buffer + ret, HEADER_LEN - ret, 0);
        if (b == 0)
        {
            DEBUG_LOG("socket Closed");
            return 0;
        }
        if (b < 0)
        {
            DEBUG_LOG("receive Message failed");
            exit(114514);
        }
        ret += b; // 成功将b个byte塞进了缓冲区
    }
    // copy header
    memcpy(&msg, buffer, HEADER_LEN);
    // receive payload
    size_t payload_len = msg.header.get_len() - HEADER_LEN;
    if (payload_len > 0)
    {
        msg.set_payload(new uint8_t[payload_len]);
        ret = 0;
        while (ret < payload_len)
        {
            ssize_t b = recv(sockfd, buffer + ret, payload_len - ret, 0);
            if (b == 0)
            {
                DEBUG_LOG("socket Closed");
                return 0;
            }
            if (b < 0)
            {
                DEBUG_LOG("receive Message failed");
                exit(114514);
            }
            ret += b; // 成功将b个byte塞进了缓冲区
        }
        // copy payload
        memcpy(msg.payload, buffer, payload_len);
    }
    DEBUG_LOG("received");
    return 1;
}

const MessageHeader OPEN_CONN_REQUEST_HEADER = MessageHeader(0xA1, 0, 12);
const MessageHeader OPEN_CONN_REPLY_HEADER = MessageHeader(0xA2, 1, 12);
const MessageHeader LIST_REQUEST_HEADER = MessageHeader(0xA3, 0, 12);
const MessageHeader LIST_REPLY_HEADER = MessageHeader(0xA4, 0, 12);
const MessageHeader CHANGE_DIR_REQUEST_HEADER = MessageHeader(0xA5, 0, 12);
const MessageHeader CHANGE_DIR_REPLY_HEADER_0 = MessageHeader(0xA6, 0, 12);
const MessageHeader CHANGE_DIR_REPLY_HEADER_1 = MessageHeader(0xA6, 1, 12);
const MessageHeader GET_REQUEST_HEADER = MessageHeader(0xA7, 0, 12);
const MessageHeader GET_REPLY_HEADER_0 = MessageHeader(0xA8, 0, 12);
const MessageHeader GET_REPLY_HEADER_1 = MessageHeader(0xA8, 1, 12);
const MessageHeader FILE_DATA_HEADER = MessageHeader(0xFF, 0, 12);
const MessageHeader PUT_REQUEST_HEADER = MessageHeader(0xA9, 0, 12);
const MessageHeader PUT_REPLY_HEADER = MessageHeader(0xAA, 0, 12);
const MessageHeader SHA256_REQUEST_HEADER = MessageHeader(0xAB, 0, 12);
const MessageHeader SHA256_REPLY_HEADER_0 = MessageHeader(0xAC, 0, 12);
const MessageHeader SHA256_REPLY_HEADER_1 = MessageHeader(0xAC, 1, 12);
const MessageHeader QUIT_REQUEST_HEADER = MessageHeader(0xAD, 0, 12);
const MessageHeader QUIT_REPLY_HEADER = MessageHeader(0xAE, 0, 12);

const int OPEN_CONN_REQUEST = 0xA1;
const int OPEN_CONN_REPLY = 0xA2;
const int LIST_REQUEST = 0xA3;
const int LIST_REPLY = 0xA4;
const int CHANGE_DIR_REQUEST = 0xA5;
const int CHANGE_DIR_REPLY = 0xA6;
const int GET_REQUEST = 0xA7;
const int GET_REPLY = 0xA8;
const int FILE_DATA = 0xFF;
const int PUT_REQUEST = 0xA9;
const int PUT_REPLY = 0xAA;
const int SHA256_REQUEST = 0xAB;
const int SHA256_REPLY = 0xAC;
const int QUIT_REQUEST = 0xAD;
const int QUIT_REPLY = 0xAE;