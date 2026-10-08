#include "Http.hpp"
#include "Build.hpp"

#include <windows.h>
#include <winhttp.h>
#include <array>

namespace http {

std::optional<std::string> get(const std::wstring& host, const std::wstring& path, int timeoutMs) {
    HINTERNET session = WinHttpOpen(L"Monchi", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return std::nullopt;
    WinHttpSetTimeouts(session, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    std::optional<std::string> result;
    HINTERNET conn = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET req = conn ? WinHttpOpenRequest(conn, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                         : nullptr;

    if (req && WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(req, nullptr)) {
        DWORD status = 0, size = sizeof(status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                            &status, &size, WINHTTP_NO_HEADER_INDEX);
        if (status == 200) {
            std::string body;
            std::array<char, 16384> chunk;
            bool complete = false;
            for (;;) {
                DWORD read = 0;
                if (!WinHttpReadData(req, chunk.data(), DWORD(chunk.size()), &read)) break;
                if (!read) { complete = true; break; }
                if (body.size() + read > (8u << 20)) break;
                body.append(chunk.data(), read);
            }
            if (complete) result = std::move(body);
        }
    }

    if (req) WinHttpCloseHandle(req);
    if (conn) WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return result;
}

std::wstring repoRawPath(const std::wstring& file) {
    return std::wstring(L"/") + build::repoOwner + L"/" + build::repoName + L"/" + build::repoBranch + L"/" + file;
}

}
