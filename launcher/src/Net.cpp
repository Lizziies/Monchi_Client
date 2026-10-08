#include "Net.hpp"
#include "Files.hpp"

#include <windows.h>
#include <winhttp.h>

#include <fstream>
#include <array>

namespace net {

namespace {

struct Handle {
    HINTERNET h = nullptr;
    explicit Handle(HINTERNET v = nullptr) : h(v) {}
    ~Handle() {
        if (h) WinHttpCloseHandle(h);
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    explicit operator bool() const { return h != nullptr; }
};

struct Request {
    Handle session, conn, req;
    DWORD status = 0;
    int64_t length = -1;
};

bool open(Request& r, const std::string& url, int timeoutMs) {
    std::wstring wurl = files::widen(url);
    wchar_t host[256] = {}, path[2048] = {}, extra[2048] = {};
    URL_COMPONENTSW uc{sizeof(uc)};
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;
    uc.lpszExtraInfo = extra;
    uc.dwExtraInfoLength = 2048;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) return false;

    r.session.h = WinHttpOpen(L"MonchiLauncher", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
    if (!r.session) return false;
    WinHttpSetTimeouts(r.session.h, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    r.conn.h = WinHttpConnect(r.session.h, host, uc.nPort, 0);
    if (!r.conn) return false;

    DWORD flags = uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    std::wstring target(path, uc.dwUrlPathLength);
    target.append(extra, uc.dwExtraInfoLength);
    if (auto fragment = target.find(L'#'); fragment != std::wstring::npos) target.erase(fragment);
    r.req.h = WinHttpOpenRequest(r.conn.h, L"GET", target.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!r.req) return false;

    static const wchar_t* headers = L"Accept: application/vnd.github+json\r\n";
    if (!WinHttpSendRequest(r.req.h, headers, (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) return false;
    if (!WinHttpReceiveResponse(r.req.h, nullptr)) return false;

    DWORD size = sizeof(r.status);
    WinHttpQueryHeaders(r.req.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &r.status, &size, WINHTTP_NO_HEADER_INDEX);

    wchar_t len[32] = {};
    size = sizeof(len);
    if (WinHttpQueryHeaders(r.req.h, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, len, &size,
                            WINHTTP_NO_HEADER_INDEX))
        r.length = _wtoi64(len);
    return r.status == 200;
}

bool read(Request& r, const std::function<bool(const char*, size_t)>& sink) {
    std::array<char, 16384> chunk;
    for (;;) {
        DWORD got = 0;
        if (!WinHttpReadData(r.req.h, chunk.data(), DWORD(chunk.size()), &got)) return false;
        if (!got) break;
        if (!sink(chunk.data(), got)) return false;
    }
    return true;
}

}

std::optional<std::string> get(const std::string& url, int timeoutMs) {
    Request r;
    if (!open(r, url, timeoutMs)) return std::nullopt;
    std::string body;
    bool ok = read(r, [&](const char* data, size_t n) {
        body.append(data, n);
        return body.size() < (16u << 20);
    });
    if (!ok) return std::nullopt;
    return body;
}

bool download(const std::string& url, const std::filesystem::path& to, const std::function<void(float)>& progress, const std::function<bool()>& cancelled, uint64_t maxBytes) {
    Request r;
    if (!open(r, url, 15000)) return false;
    if (maxBytes && r.length > 0 && uint64_t(r.length) > maxBytes) return false;

    std::ofstream out(to, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    int64_t total = 0;
    bool ok = read(r, [&](const char* data, size_t n) {
        if (cancelled && cancelled()) return false;
        if (maxBytes && (uint64_t(total) > maxBytes || n > maxBytes - uint64_t(total))) return false;
        out.write(data, (std::streamsize)n);
        total += (int64_t)n;
        if (r.length > 0) progress(float(total) / float(r.length));
        return bool(out);
    });
    out.close();
    ok = ok && bool(out) && (r.length < 0 || total == r.length);
    if (!ok) {
        std::error_code ec;
        std::filesystem::remove(to, ec);
    }
    return ok;
}

}
