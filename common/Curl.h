/*************************************************************************
    > File Name: Curl.h
    > Author: yangjianwei
    > Mail: yangjianwei@sogou-inc.com 
    > Created Time: Wed 06 Jun 2018 04:21:32 PM CST
 ************************************************************************/

#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>

#include <curl/curl.h>

struct MemoryStruct {
    char *memory;
    size_t size;
};

class Curl {
    public :
        Curl(){}

        bool Init();
        bool SetHeaders(std::vector<std::string> &headers);
        bool SetTimeout(unsigned int timeout);
        bool ResetHeaders(std::vector<std::string> &headers);
        bool Post(const std::string &url, const std::string &content, void *chunk);
        ~Curl();

    private:
        CURL *curl_handle;
        CURLcode res;
        curl_slist *head;
        //struct MemoryStruct chunk;
};
