#pragma once

// The part of libcurl's easy interface Flarial's HTTP helpers use (APIUtils, Hive stats), served over WinHTTP so
// the client needs no curl dll. Only the options listed here exist; others are rejected.

#include <cstddef>

using CURL = struct CurlHandle;

struct curl_slist {
    char* data;
    curl_slist* next;
};

enum CURLcode { CURLE_OK = 0, CURLE_FAILED_INIT = 2, CURLE_URL_MALFORMAT = 3, CURLE_COULDNT_CONNECT = 7, CURLE_UNKNOWN_OPTION = 48, CURLE_RECV_ERROR = 56 };

enum CURLoption {
    CURLOPT_WRITEDATA = 10001,
    CURLOPT_URL = 10002,
    CURLOPT_POSTFIELDS = 10015,
    CURLOPT_USERAGENT = 10018,
    CURLOPT_HTTPHEADER = 10023,
    CURLOPT_CUSTOMREQUEST = 10036,
    CURLOPT_ACCEPT_ENCODING = 10102,
    CURLOPT_TIMEOUT = 13,
    CURLOPT_CONNECTTIMEOUT = 78,
    CURLOPT_POST = 47,
    CURLOPT_FOLLOWLOCATION = 52,
    CURLOPT_NOSIGNAL = 99,
    CURLOPT_WRITEFUNCTION = 20011,
};

enum CURLINFO { CURLINFO_RESPONSE_CODE = 0x200002 };

constexpr long CURL_GLOBAL_DEFAULT = 3;

using curl_write_callback = size_t (*)(char*, size_t, size_t, void*);

CURLcode curl_global_init(long flags);
void curl_global_cleanup();
CURL* curl_easy_init();
void curl_easy_cleanup(CURL* curl);
CURLcode curl_easy_setopt(CURL* curl, CURLoption option, ...);
CURLcode curl_easy_perform(CURL* curl);
CURLcode curl_easy_getinfo(CURL* curl, CURLINFO info, ...);
const char* curl_easy_strerror(CURLcode code);
curl_slist* curl_slist_append(curl_slist* list, const char* text);
void curl_slist_free_all(curl_slist* list);
