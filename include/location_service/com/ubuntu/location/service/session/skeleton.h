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
#ifndef LOCATION_SERVICE_COM_UBUNTU_LOCATION_SERVICE_SESSION_SKELETON_H_
#define LOCATION_SERVICE_COM_UBUNTU_LOCATION_SERVICE_SESSION_SKELETON_H_

#include "com/ubuntu/location/service/session/interface.h"

#include "com/ubuntu/location/channel.h"
#include "com/ubuntu/location/heading.h"
#include "com/ubuntu/location/position.h"
#include "com/ubuntu/location/provider.h"
#include "com/ubuntu/location/update.h"
#include "com/ubuntu/location/velocity.h"

#include <org/freedesktop/dbus/message.h>
#include <org/freedesktop/dbus/skeleton.h>

#include <functional>

namespace com
{
namespace ubuntu
{
namespace location
{
namespace service
{
namespace session
{
class Skeleton : public org::freedesktop::dbus::Skeleton<Interface>
{
  public:
    Skeleton(
        const org::freedesktop::dbus::Bus::Ptr& bus,
        const org::freedesktop::dbus::types::ObjectPath& session_path);
    Skeleton(const Skeleton&) = delete;
    virtual ~Skeleton() noexcept;
    Skeleton& operator=(const Skeleton&) = delete;

    virtual const org::freedesktop::dbus::types::ObjectPath& path() const;

  private:
    struct Private;
    std::unique_ptr<Private> d;
};
}
}
}
}
}

#endif // LOCATION_SERVICE_COM_UBUNTU_LOCATION_SERVICE_SESSION_SKELETON_H_
