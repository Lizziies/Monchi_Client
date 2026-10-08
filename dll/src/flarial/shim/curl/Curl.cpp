#include "curl/curl.h"

#include <windows.h>
#include <winhttp.h>

#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

struct CurlHandle {
    std::wstring url;
    std::wstring agent = L"Monchi";
    std::wstring method = L"GET";
    std::string body;
    std::vector<std::wstring> headers;
    curl_write_callback write = nullptr;
    void* writeData = nullptr;
    long timeoutMs = 0;
    long connectMs = 0;
    bool follow = false;
    long status = 0;
};

static std::wstring widen(const char* s) {
    if (!s) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    std::wstring out(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s, -1, out.data(), n);
    return out;
}

CURLcode curl_global_init(long) { return CURLE_OK; }
void curl_global_cleanup() {}
CURL* curl_easy_init() { return new CurlHandle; }
void curl_easy_cleanup(CURL* curl) { delete curl; }

CURLcode curl_easy_setopt(CURL* curl, CURLoption option, ...) {
    va_list args;
    va_start(args, option);
    CURLcode result = CURLE_OK;
    switch (option) {
    case CURLOPT_URL: curl->url = widen(va_arg(args, const char*)); break;
    case CURLOPT_USERAGENT: curl->agent = widen(va_arg(args, const char*)); break;
    case CURLOPT_POSTFIELDS: curl->body = va_arg(args, const char*); if (curl->method == L"GET") curl->method = L"POST"; break;
    case CURLOPT_POST: if (va_arg(args, long)) curl->method = L"POST"; break;
    case CURLOPT_CUSTOMREQUEST: curl->method = widen(va_arg(args, const char*)); break;
    case CURLOPT_HTTPHEADER:
        curl->headers.clear();
        for (auto* h = va_arg(args, curl_slist*); h; h = h->next) curl->headers.push_back(widen(h->data));
        break;
    case CURLOPT_WRITEFUNCTION: curl->write = va_arg(args, curl_write_callback); break;
    case CURLOPT_WRITEDATA: curl->writeData = va_arg(args, void*); break;
    case CURLOPT_TIMEOUT: curl->timeoutMs = va_arg(args, long) * 1000; break;
    case CURLOPT_CONNECTTIMEOUT: curl->connectMs = va_arg(args, long) * 1000; break;
    case CURLOPT_FOLLOWLOCATION: curl->follow = va_arg(args, long) != 0; break;
    case CURLOPT_ACCEPT_ENCODING:
    case CURLOPT_NOSIGNAL: (void)va_arg(args, void*); break;
    default: result = CURLE_UNKNOWN_OPTION;
    }
    va_end(args);
    return result;
}

CURLcode curl_easy_perform(CURL* curl) {
    URL_COMPONENTS parts{sizeof(parts)};
    wchar_t host[256]{}, path[4096]{};
    parts.lpszHostName = host;
    parts.dwHostNameLength = 255;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 4095;
    wchar_t extra[4096]{};
    parts.lpszExtraInfo = extra;
    parts.dwExtraInfoLength = 4095;
    if (!WinHttpCrackUrl(curl->url.c_str(), 0, 0, &parts)) return CURLE_URL_MALFORMAT;

    CURLcode result = CURLE_COULDNT_CONNECT;
    HINTERNET session = WinHttpOpen(curl->agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET connect = session ? WinHttpConnect(session, host, parts.nPort, 0) : nullptr;
    std::wstring target = std::wstring(path) + extra;
    DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = connect ? WinHttpOpenRequest(connect, curl->method.c_str(), target.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags) : nullptr;
    if (request) {
        int connectMs = curl->connectMs ? int(curl->connectMs) : 60000, totalMs = curl->timeoutMs ? int(curl->timeoutMs) : 60000;
        WinHttpSetTimeouts(request, connectMs, connectMs, totalMs, totalMs);
        DWORD decompress = WINHTTP_DECOMPRESSION_FLAG_ALL;
        WinHttpSetOption(request, WINHTTP_OPTION_DECOMPRESSION, &decompress, sizeof(decompress));
        DWORD redirect = curl->follow ? WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS : WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));
        for (auto& h : curl->headers) WinHttpAddRequestHeaders(request, h.c_str(), DWORD(-1), WINHTTP_ADDREQ_FLAG_ADD);
        void* data = curl->body.empty() ? WINHTTP_NO_REQUEST_DATA : curl->body.data();
        DWORD size = DWORD(curl->body.size());
        if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, data, size, size, 0) && WinHttpReceiveResponse(request, nullptr)) {
            DWORD status = 0, len = sizeof(status);
            WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);
            curl->status = long(status);
            result = CURLE_OK;
            std::vector<char> buf(16384);
            for (DWORD got = 0; WinHttpReadData(request, buf.data(), DWORD(buf.size()), &got) && got;) {
                if (curl->write && curl->write(buf.data(), 1, got, curl->writeData) != got) {
                    result = CURLE_RECV_ERROR;
                    break;
                }
            }
        }
    }
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);
    return result;
}

CURLcode curl_easy_getinfo(CURL* curl, CURLINFO info, ...) {
    va_list args;
    va_start(args, info);
    CURLcode result = CURLE_UNKNOWN_OPTION;
    if (info == CURLINFO_RESPONSE_CODE) {
        *va_arg(args, long*) = curl->status;
        result = CURLE_OK;
    }
    va_end(args);
    return result;
}

const char* curl_easy_strerror(CURLcode code) {
    switch (code) {
    case CURLE_OK: return "no error";
    case CURLE_URL_MALFORMAT: return "malformed url";
    case CURLE_COULDNT_CONNECT: return "could not connect";
    case CURLE_RECV_ERROR: return "receive failed";
    case CURLE_UNKNOWN_OPTION: return "unknown option";
    default: return "request failed";
    }
}

curl_slist* curl_slist_append(curl_slist* list, const char* text) {
    auto* item = new curl_slist{_strdup(text), nullptr};
    if (!list) return item;
    curl_slist* last = list;
    while (last->next) last = last->next;
    last->next = item;
    return list;
}

void curl_slist_free_all(curl_slist* list) {
    while (list) {
        curl_slist* next = list->next;
        free(list->data);
        delete list;
        list = next;
    }
}
