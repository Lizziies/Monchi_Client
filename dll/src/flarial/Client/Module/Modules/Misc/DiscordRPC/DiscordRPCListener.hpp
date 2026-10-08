// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Module/Modules/Misc/DiscordRPC/DiscordRPCListener.hpp. Monchi shows its own
// Discord presence (modules/platform/Presence.hpp) and window title, and Flarial's discord library is no longer
// published, so the service Flarial's manager registers stays empty.
#pragma once

#include "Events/Listener.hpp"

class DiscordRPCListener : public Listener {};
