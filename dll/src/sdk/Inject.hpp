#pragma once

#include <deque>
#include <string>

namespace inject {

struct Sent {
    std::string text;
    double at = 0.0;
    bool real = true;
};

bool ours();
bool focused();
const std::deque<Sent>& sent();

void key(int vk, bool down);
void tap(int vk);
void tapLater(int vk);
void say(const std::string& text, int chatKey = 'T');
// the key the game has bound to chat, once known: it replaces the chat key a module asks for
void gameChatKey(int vk);
void shutdown();

}
