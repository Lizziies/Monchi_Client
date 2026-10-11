#pragma once

class Module;

namespace gui {

void beginFrame();
void hideOverlays();
void draw();

bool open();
void setOpen(bool on);
void toggle();
void showModule(Module* m);

bool editingHud();
void setEditingHud(bool on);

bool wantsInput();
bool wantsCursor();
bool capturesKeyboard();
void claimKeyboard();

}
