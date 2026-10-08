#pragma once

#ifndef MONCHI_VERSION
#define MONCHI_VERSION "0.1.0"
#endif

#ifndef MONCHI_REPO_OWNER
#define MONCHI_REPO_OWNER ""
#endif
#ifndef MONCHI_REPO_NAME
#define MONCHI_REPO_NAME ""
#endif
#ifndef MONCHI_ONLINE_URL
#define MONCHI_ONLINE_URL ""
#endif
#define MONCHI_WIDE2(x) L##x
#define MONCHI_WIDE(x) MONCHI_WIDE2(x)

namespace build {

inline constexpr const char* name = "Monchi";
inline constexpr const char* version = MONCHI_VERSION;

// Where updates, signatures and server rules come from. Given to the build (MONCHI_REPO_OWNER, MONCHI_REPO_NAME, see
// local.cmake.example); a build without them looks nothing up on GitHub and uses what it was shipped with.
inline constexpr const wchar_t* repoOwner = MONCHI_WIDE(MONCHI_REPO_OWNER);
inline constexpr const wchar_t* repoName = MONCHI_WIDE(MONCHI_REPO_NAME);
inline constexpr const wchar_t* repoBranch = L"main";
// default address of the Monchi Online service, empty in a build that was not given one
inline constexpr const char* onlineUrl = MONCHI_ONLINE_URL;

}
