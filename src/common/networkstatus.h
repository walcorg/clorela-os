// SPDX-License-Identifier: MIT
// Minimal "are we online?" check, without Qt Network plugins.
//
// Online = the kernel routing table has a default route through an interface
// that is up (normally wlan0 on the Pi Zero W). This does not prove that the
// WALCORG server is reachable; see the TODO list in the README.
//
// For development the result can be forced with CLORELA_NETWORK=online|offline.
#pragma once

#include <QString>

namespace clorela {

class NetworkStatus
{
public:
    static bool isOnline();
    // Parses the content of /proc/net/route (exposed for tests).
    static bool hasDefaultRoute(const QString &procNetRoute);
};

} // namespace clorela
