#pragma once

// The game loads a worn item's texture from its file. That file is sealed, so what the game reads from it is opened
// in its read buffer; the open picture exists only in memory.
namespace sealed {

void install();

}
