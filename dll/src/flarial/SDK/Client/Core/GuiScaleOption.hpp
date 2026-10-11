#pragma once

class ClientInstance;

namespace guiScaleOption {

bool read(ClientInstance* client, int& value);
bool set(ClientInstance* client, int value);
bool repair(ClientInstance* client);
bool refresh(ClientInstance* client);

class Override {
public:
    bool apply(ClientInstance* client, int value);
    bool restore(ClientInstance* client);
    bool active() const { return owner_ != nullptr; }
    void reset() { owner_ = nullptr; }

private:
    ClientInstance* owner_ = nullptr;
    int original_ = 0;
    int applied_ = 0;
};

}
