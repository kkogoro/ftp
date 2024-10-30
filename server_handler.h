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
#include <fstream>
#include <filesystem>
#include "debug.h"

namespace fs = std::filesystem;

void handle_list_request(int client, uint8_t *buffer)
{
    const char *command = "ls";
    FILE *pipe = popen(command, "r");
    if (!pipe)
    {
        DEBUG_LOG("popen failed!");
        exit(114514);
    }

    const int MAX_LINE = 2047 + 10; // 在该目录下执行 ls 指令，其返回的结果的总长度不会超过 2047 个字符
    char line[MAX_LINE];
    size_t tot_len = 0;
    while (fgets((char *)line, MAX_LINE, pipe) != NULL)
    {
        size_t len = strlen((char *)line);
        memcpy(buffer + tot_len, line, len);
        tot_len += len;
    }

    buffer[tot_len] = '\0'; // 注意返回的内容结尾应当增加一个\0以表示内容结束。
    uint8_t *payload = new uint8_t[tot_len + 1];
    memcpy(payload, buffer, tot_len + 1); // 含0

    Message msg(LIST_REPLY_HEADER, payload, HEADER_LEN + tot_len + 1);
    send_message(client, msg, buffer);

    if (pclose(pipe) == -1)
    {
        DEBUG_LOG("pclose failed!");
        exit(114514);
    }
    return;
}

void handle_change_dir_request(int client, char *DIR, uint8_t *buffer, fs::path &cur_path)
{
    fs::path target_path = cur_path / DIR;
    if (!fs::exists(target_path) || !fs::is_directory(target_path))
    {
        DEBUG_LOG("path not exists or is not a directory");
        send_message(client, Message(CHANGE_DIR_REPLY_HEADER_0, NULL, HEADER_LEN), buffer);
    }
    else
    {
        fs::current_path(target_path);
        cur_path = target_path;
        send_message(client, Message(CHANGE_DIR_REPLY_HEADER_1, NULL, HEADER_LEN), buffer);
    }
    return;
}

void handle_get_request(int client, char *payload, uint8_t *buffer)
{
    fs::path file_name = payload;
    if (!fs::exists(file_name) || !fs::is_regular_file(file_name))
    {
        DEBUG_LOG("file not exists or is not a regular file");
        send_message(client, Message(GET_REPLY_HEADER_0, NULL, HEADER_LEN), buffer);
    }
    else
    {
        send_message(client, Message(GET_REPLY_HEADER_1, NULL, HEADER_LEN), buffer);
        std::ifstream fileIO(file_name, std::ios::in | std::ios::binary);
        if (!fileIO)
        {
            DEBUG_LOG("open file failed");
            exit(114514);
        }
        else
        {
            // TODO:是否应该加锁
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
                send_message(client, msg, buffer);
                cout << "file sent" << endl;
            }
            fileIO.close();
        }
    }
    return;
}

void handle_put_request(int client, char *payload, uint8_t *buffer)
{
    send_message(client, Message(PUT_REPLY_HEADER, NULL, HEADER_LEN), buffer);
    cout << "put reply sent" << endl;

    Message msg_data;
    if (!receive_message(client, msg_data, buffer))
    {
        DEBUG_LOG("put failed : lost connect during receiving data");
        exit(114514);
    }

    if (msg_data.header.get_type() == FILE_DATA)
    {
        cout << "put success : data downloaded" << endl;
        fs::path file_name = payload;
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
        DEBUG_LOG("put failed");
        exit(114514);
    }
    return;
}

void handle_sha256_request(int client, char *payload, uint8_t *buffer)
{
    fs::path file_name = payload;
    if (!fs::exists(file_name) || !fs::is_regular_file(file_name))
    {
        cout << "sha256 failed : file not exists or is not a regular file" << endl;
        send_message(client, Message(SHA256_REPLY_HEADER_0, NULL, HEADER_LEN), buffer);
        return;
    }
    else
    {
        send_message(client, Message(SHA256_REPLY_HEADER_1, NULL, HEADER_LEN), buffer);
        cout << "sha256 reply sent" << endl;
    }

    // send sha256

    string command = "sha256sum " + file_name.string();
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe)
    {
        DEBUG_LOG("popen failed!");
        exit(114514);
    }

    const int MAX_LINE = 2047 + 10; // sha256 32字节
    char line[MAX_LINE];
    size_t tot_len = 0;
    while (fgets((char *)line, MAX_LINE, pipe) != NULL)
    {
        size_t len = strlen((char *)line);
        memcpy(buffer + tot_len, line, len);
        tot_len += len;
    }

    buffer[tot_len] = '\0'; // 注意返回的内容结尾应当增加一个\0以表示内容结束。
    uint8_t *new_payload = new uint8_t[tot_len + 1];
    memcpy(new_payload, buffer, tot_len + 1); // 含0

    Message msg(FILE_DATA_HEADER, new_payload, HEADER_LEN + tot_len + 1);
    send_message(client, msg, buffer);

    if (pclose(pipe) == -1)
    {
        DEBUG_LOG("pclose failed!");
        exit(114514);
    }
    return;
}

void handle_quit_request(int client, uint8_t *buffer)
{
    send_message(client, Message(QUIT_REPLY_HEADER, NULL, HEADER_LEN), buffer);
    close(client);
    cout << "disconnected" << endl;
    return;
}