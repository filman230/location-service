/*
 * Copyright © 2016 Canonical Ltd.
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

#include <location/providers/state_tracking_provider.h>

#include <location/clock.h>
#include <location/update.h>
#include <location/position.h>
#include <location/units/units.h>
#include <location/wgs84/latitude.h>
#include <location/wgs84/longitude.h>
#include <location/wgs84/altitude.h>

#include "mock_provider.h"

#include <gtest/gtest.h>

namespace cul = location;

namespace
{
struct MockEvent : public location::Event
{
    MOCK_CONST_METHOD0(type, Type());
};

auto timestamp = cul::Clock::now();

// Create reference objects for injecting and validating updates.
cul::Update<cul::Position> reference_position_update
{
    {
        cul::wgs84::Latitude{9. * cul::units::Degrees},
        cul::wgs84::Longitude{53. * cul::units::Degrees},
        cul::wgs84::Altitude{-2. * cul::units::Meters}
    },
    timestamp
};

cul::Update<cul::Velocity> reference_velocity_update
{
    {5. * cul::units::MetersPerSecond},
    timestamp
};

cul::Update<cul::Heading> reference_heading_update
{
    {120. * cul::units::Degrees},
    timestamp
};
}
TEST(StateTrackingProviderTest, forwards_calls_to_impl)
{
    using namespace testing;

    auto impl = std::make_shared<MockProvider>();
    EXPECT_CALL(*impl, on_new_event(_)).Times(1);
    EXPECT_CALL(*impl, satisfies(_)).Times(1).WillOnce(Return(false));
    EXPECT_CALL(*impl, requirements()).Times(1).WillOnce(Return(location::Provider::Requirements::none));
    EXPECT_CALL(*impl, activate()).Times(1);
    EXPECT_CALL(*impl, deactivate()).Times(1);
    EXPECT_CALL(*impl, disable()).Times(1);

    location::providers::StateTrackingProvider stp{impl};

    stp.on_new_event(MockEvent{});
    EXPECT_EQ(location::Provider::Requirements::none, stp.requirements());
    EXPECT_FALSE(stp.satisfies(location::Criteria{}));
    stp.enable();
    stp.activate();
    stp.deactivate();
    stp.disable();
}

TEST(StateTrackingProviderTest, state_after_construction_is_enabled)
{
    using namespace ::testing;
    location::providers::StateTrackingProvider stp{std::make_shared<NiceMock<MockProvider>>()};
    EXPECT_EQ(location::providers::StateTrackingProvider::State::enabled, stp.state());
}

TEST(StateTrackingProviderTest, state_after_start_is_active)
{
    using namespace ::testing;
    location::providers::StateTrackingProvider stp{std::make_shared<NiceMock<MockProvider>>()};
    stp.activate();
    EXPECT_EQ(location::providers::StateTrackingProvider::State::active, stp.state());
}

TEST(StateTrackingProviderTest, stop_switches_to_enabled)
{
    using namespace ::testing;
    location::providers::StateTrackingProvider stp{std::make_shared<NiceMock<MockProvider>>()};
    stp.activate();
    stp.deactivate();
    EXPECT_EQ(location::providers::StateTrackingProvider::State::enabled, stp.state());
}
