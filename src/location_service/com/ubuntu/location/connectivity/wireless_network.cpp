/*
 * Copyright © 2012-2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */

#include <com/ubuntu/location/connectivity/wireless_network.h>

#include <iostream>

namespace location = com::ubuntu::location;

std::ostream& location::connectivity::operator<<(std::ostream& out, location::connectivity::WirelessNetwork::Mode mode)
{
    switch (mode)
    {
    case location::connectivity::WirelessNetwork::Mode::unknown: out << "Mode::unknown"; break;
    case location::connectivity::WirelessNetwork::Mode::adhoc: out << "Mode::adhoc"; break;
    case location::connectivity::WirelessNetwork::Mode::infrastructure: out << "Mode::infrastructure"; break;
    }

    return out;
}

std::ostream& location::connectivity::operator<<(std::ostream& out, const location::connectivity::WirelessNetwork& wifi)
{
    return out << "("
               << "bssid: " << wifi.bssid().get() << ", "
               << "ssid: " << wifi.ssid().get() << ", "
               << "last seen: " << wifi.last_seen().get().time_since_epoch().count() << ", "
               << "mode: " << wifi.mode().get() << ", "
               << "frequency: " << wifi.frequency().get() << ", "
               << "strength: " << wifi.signal_strength().get()
               << ")";
}
