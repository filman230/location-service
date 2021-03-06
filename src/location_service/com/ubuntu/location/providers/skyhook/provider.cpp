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
#include <com/ubuntu/location/providers/skyhook/provider.h>

#include <com/ubuntu/location/logging.h>
#include <com/ubuntu/location/provider_factory.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic warning "-Wignored-qualifiers"
#include <wpsapi.h>
#pragma GCC diagnostic pop
#include <map>
#include <thread>

namespace cul = com::ubuntu::location;
namespace culs = com::ubuntu::location::providers::skyhook;

namespace
{
static const std::map<int, std::string> return_code_lut =
{
    {WPS_OK, "WPS_OK"},
    {WPS_ERROR_SCANNER_NOT_FOUND, "WPS_ERROR_SCANNER_NOT_FOUND"},
    {WPS_ERROR_WIFI_NOT_AVAILABLE, "WPS_ERROR_WIFI_NOT_AVAILABLE"},
    {WPS_ERROR_NO_WIFI_IN_RANGE, "WPS_ERROR_NO_WIFI_IN_RANGE"},
    {WPS_ERROR_UNAUTHORIZED, "WPS_ERROR_UNAUTHORIZED"},
    {WPS_ERROR_SERVER_UNAVAILABLE, "WPS_ERROR_SERVER_UNAVAILABLE"},
    {WPS_ERROR_LOCATION_CANNOT_BE_DETERMINED, "WPS_ERROR_LOCATION_CANNOT_BE_DETERMINED"},
    {WPS_ERROR_PROXY_UNAUTHORIZED, "WPS_ERROR_PROXY_UNAUTHORIZED"},
    {WPS_ERROR_FILE_IO, "WPS_ERROR_FILE_IO"},
    {WPS_ERROR_INVALID_FILE_FORMAT, "WPS_ERROR_INVALID_FILE_FORMAT"},
    {WPS_ERROR_TIMEOUT, "WPS_ERROR_TIMEOUT"},
    {WPS_NOT_APPLICABLE, "WPS_NOT_APPLICABLE"},
    {WPS_GEOFENCE_ERROR, "WPS_GEOFENCE_ERROR"},
    {WPS_ERROR_NOT_TUNED, "WPS_ERROR_NOT_TUNED"},
    {WPS_NOMEM, "WPS_NOMEM"},
    {WPS_ERROR, "WPS_ERROR"}
};
}


WPS_Continuation culs::Provider::Private::periodic_callback(void* context,
                                                            WPS_ReturnCode code,
                                                            const WPS_Location* location,
                                                            const void*)
{
    if (code != WPS_OK)
    {
        LOG(WARNING) << return_code_lut.at(code);
        if (code == WPS_ERROR_WIFI_NOT_AVAILABLE)            
            return WPS_STOP;

        return WPS_CONTINUE;
    }

    auto thiz = static_cast<culs::Provider::Private*>(context);

    if (thiz->state == culs::Provider::Private::State::stop_requested)
    {
        LOG(INFO) << "Stop requested";
        thiz->state = culs::Provider::Private::State::stopped;
        return WPS_STOP;
    }

    cul::Position pos;
    pos.latitude(cul::wgs84::Latitude{location->latitude * cul::units::Degrees})
            .longitude(cul::wgs84::Longitude{location->longitude * cul::units::Degrees});
    if (location->altitude >= 0.f)
        pos.altitude(cul::wgs84::Altitude{location->altitude * cul::units::Meters});

    LOG(INFO) << pos;

    thiz->parent->deliver_position_updates(cul::Update<cul::Position>{pos, cul::Clock::now()});
        
    if (location->speed >= 0.f)
    {
        cul::Velocity v{location->speed * cul::units::MetersPerSecond};
        LOG(INFO) << v;
        thiz->parent->deliver_velocity_updates(cul::Update<cul::Velocity>{v, cul::Clock::now()});
    }

    if (location->bearing >= 0.f)
    {
        cul::Heading h{location->bearing * cul::units::Degrees};
        LOG(INFO) << h;
        thiz->parent->deliver_heading_updates(cul::Update<cul::Heading>{h, cul::Clock::now()});
    }

    return WPS_CONTINUE;
}
    
cul::Provider::Ptr culs::Provider::create_instance(const cul::ProviderFactory::Configuration& config)
{
    culs::Provider::Configuration configuration
    {
        config.get(Configuration::key_username(), ""), 
        config.get(Configuration::key_realm(), ""),
        std::chrono::milliseconds{config.get(Configuration::key_period(), 500)}
    };

    return cul::Provider::Ptr{new culs::Provider{configuration}};
}

const cul::Provider::FeatureFlags& culs::Provider::default_feature_flags()
{
    static const cul::Provider::FeatureFlags flags{"001"};
    return flags;
}

const cul::Provider::RequirementFlags& culs::Provider::default_requirement_flags()
{
    static const cul::Provider::RequirementFlags flags{"1010"};
    return flags;
}

culs::Provider::Provider(const culs::Provider::Configuration& config) 
        : com::ubuntu::location::Provider(culs::Provider::default_feature_flags(), culs::Provider::default_requirement_flags()),
          config(config),
          state(State::stopped)
{
}

culs::Provider::~Provider() noexcept
{
    request_stop();
}

void culs::Provider::start()
{
    if (state != State::stopped)
        return;

    if (worker.joinable())
        worker.join();

    static const unsigned infinite_iterations = 0;

    authentication.username = config.user_name.c_str();
    authentication.realm = config.realm.c_str();

    worker = std::move(std::thread([&]()
    {
        int rc = WPS_periodic_location(&authentication, WPS_NO_STREET_ADDRESS_LOOKUP, config.period.count(),
                                       infinite_iterations, culs::Provider::Private::periodic_callback, this);

        if (rc != WPS_OK)
            LOG(ERROR) << return_code_lut.at(rc);
    }));

    state = State::started;
}

void culs::Provider::request_stop()
{
    state = State::stop_requested;
}

bool culs::Provider::matches_criteria(const cul::Criteria&)
{
    return true;
}

void culs::Provider::start_position_updates()
{
    start();
}

void culs::Provider::stop_position_updates()
{
    request_stop();
}

void culs::Provider::start_velocity_updates()
{
    start();
}

void culs::Provider::stop_velocity_updates()
{
    request_stop();
}    

void culs::Provider::start_heading_updates()
{
    start();
}

void culs::Provider::stop_heading_updates()
{
    request_stop();
}
