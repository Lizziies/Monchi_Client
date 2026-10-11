// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Utils/Telemetry.cpp: disable reporting and collection of machine identifiers.
#include "Telemetry.hpp"
#include "Client.hpp"

void Telemetry::sendModuleEvent(const std::string&, const std::string&) {}
void Telemetry::sendStartupVersionPing(const std::string&) {}
std::string Telemetry::generateUserHash() { return {}; }
std::string Telemetry::getClientVersion() { return Client::version; }
