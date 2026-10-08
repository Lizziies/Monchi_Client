#include "ClientClaim.hpp"
#include "ClientImage.hpp"

int main() {
    if (!isClientImage(L"Monchi.dll") || !isClientImage(L"MONCHI-261003-194152.DLL")) return 1;
    if (!isClientImage(L"Mochi.dll") || !isClientImage(L"Mochi-dev.dll")) return 2;
    if (isClientImage(L"MonchiFlarial.dll") || isClientImage(L"MonchiLauncher.exe")) return 3;
    ClientClaim first, second;
    if (!first.acquire() || second.acquire()) return 4;
    first.release();
    if (!second.acquire()) return 5;
    return 0;
}
