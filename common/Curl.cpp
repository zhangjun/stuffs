/*************************************************************************
    > File Name: Curl.cpp
    > Author: Jun Zhang
    > Mail: ewalker.zj@gmail.com 
    > Created Time: Wed 06 Jun 2018 04:23:56 PM CST
 ************************************************************************/

#include<iostream>

#include "Curl.h"

//static size_t WriteCallback(void *content, size_t size, size_t nmemb, void *userp){
//    size_t realsize = size * nmemb;
//    MemoryStruct *mem = (struct MemoryStruct *)userp;
//
//    mem -> memory = (char*)realloc(mem -> memory, mem -> size + realsize + 1);
//    if(mem -> memory == NULL){
//        std::cout << "no enought memory" << std::endl;
//        return 0;
//    }
//
//    memcpy(&mem -> memory[mem -> size], content, realsize);
//    mem -> size += realsize;
//    mem -> memory[mem -> size] = 0;
//    return realsize;
//}

static size_t WriteCallback(void *content, size_t size, size_t nmemb, void *userp){
    size_t n = size * nmemb;
    ((std::string*)userp) -> append((char*)content, n);
    return n;
}

bool Curl::Init(){
    //chunk.memory = malloc(1);
    //chunk.size = 0;

    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteCallback);
    head = NULL;

    // time out 
    curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 5L);
    return true;
}

bool Curl::SetHeaders(std::vector<std::string> &headers) {
    curl_slist *temp = NULL;
    for(auto it : headers){
        temp = curl_slist_append(head, it.c_str());
        if(temp == NULL)
            return false;
        head = temp;
    }

    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, head);
    return true;
}

bool Curl::SetTimeout(unsigned int timeout){
    curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, timeout);
}

bool Curl::ResetHeaders(std::vector<std::string> &headers) {
    // NULL in list argument to create new list
    head = NULL;
    curl_slist *temp = NULL;
    for(auto it : headers){
        temp = curl_slist_append(head, it.c_str());
        if(temp == NULL)
            return false;
        head = temp;
    }

    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, head);
    return true;
}


bool Curl::Post(const std::string &url, const std::string &content, void *chunk){
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, chunk);
    curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, content.c_str());
    // for large content
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, content.size());
    
    res = curl_easy_perform(curl_handle);

    if(res != CURLE_OK){
        std::cerr << "curl_easy_perform() faild: " << curl_easy_strerror(res) << std::endl;
        return false;
    }

    return true;
}

Curl::~Curl(){
    //free(chunk.memory);
    curl_slist_free_all(head);
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
}
