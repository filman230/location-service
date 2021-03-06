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
#include <com/ubuntu/location/engine.h>

#include <com/ubuntu/location/logging.h>
#include <com/ubuntu/location/provider_selection_policy.h>
#include <com/ubuntu/location/state_tracking_provider.h>

#include <iostream>
#include <stdexcept>
#include <unordered_map>

#include "time_based_update_policy.h"

namespace cul = com::ubuntu::location;

const cul::SatelliteBasedPositioningState cul::Engine::Configuration::Defaults::satellite_based_positioning_state;
const cul::WifiAndCellIdReportingState cul::Engine::Configuration::Defaults::wifi_and_cell_id_reporting_state;
const cul::Engine::Status cul::Engine::Configuration::Defaults::engine_state;

cul::Engine::Engine(const cul::ProviderSelectionPolicy::Ptr& provider_selection_policy,
                    const cul::Settings::Ptr& settings)
          : provider_selection_policy(provider_selection_policy),
            settings(settings),
            update_policy(std::make_shared<cul::TimeBasedUpdatePolicy>())
{
    if (!provider_selection_policy) throw std::runtime_error
    {
        "Cannot construct an engine given a null ProviderSelectionPolicy"
    };

    if (!settings) throw std::runtime_error
    {
        "Cannot construct an engine given a null Settings instance"
    };

    // Setup behavior in case of configuration changes.
    configuration.satellite_based_positioning_state.changed().connect([this](const SatelliteBasedPositioningState& state)
    {
        for_each_provider([this, state](const Provider::Ptr& provider)
        {
            if (provider->requires(cul::Provider::Requirements::satellites))
            {
                switch (state)
                {
                case SatelliteBasedPositioningState::on:
                    // We only enable a provider if the overall engine is enabled.
                    if (configuration.engine_state == Engine::Status::on)
                        provider->state_controller()->enable();
                    break;
                case SatelliteBasedPositioningState::off:
                    provider->state_controller()->disable();
                    break;
                }
            }
        });
    });

    configuration.engine_state.changed().connect([this](const Engine::Status& status)
    {
        for_each_provider([this, status](const Provider::Ptr& provider)
        {
            switch (status)
            {
            case Engine::Status::on:
                // We only enable providers that require satellites if the respective engine option is set to on.
                if (provider->requires(cul::Provider::Requirements::satellites) && configuration.satellite_based_positioning_state == SatelliteBasedPositioningState::off)
                    return;

                provider->state_controller()->enable();
                break;
            case Engine::Status::off:
                provider->state_controller()->disable();
                break;
            default:
                break;
            }
        });
    });

    configuration.engine_state =
            settings->get_enum_for_key<Engine::Status>(
                Configuration::Keys::engine_state,
                Configuration::Defaults::engine_state);

    configuration.satellite_based_positioning_state = Configuration::Defaults::satellite_based_positioning_state;

    configuration.wifi_and_cell_id_reporting_state =
            settings->get_enum_for_key<WifiAndCellIdReportingState>(
                Configuration::Keys::wifi_and_cell_id_reporting_state,
                Configuration::Defaults::wifi_and_cell_id_reporting_state);

    configuration.engine_state.changed().connect([this](const Engine::Status& status)
    {
        Engine::settings->set_enum_for_key<Engine::Status>(Configuration::Keys::engine_state, status);
    });    

    configuration.wifi_and_cell_id_reporting_state.changed().connect([this](WifiAndCellIdReportingState state)
    {
        Engine::settings->set_enum_for_key<WifiAndCellIdReportingState>(Configuration::Keys::wifi_and_cell_id_reporting_state, state);
    });
}

cul::Engine::~Engine()
{
    settings->set_enum_for_key<Engine::Status>(
        Configuration::Keys::engine_state,
        configuration.engine_state);

    settings->set_enum_for_key<WifiAndCellIdReportingState>(
        Configuration::Keys::wifi_and_cell_id_reporting_state,
        configuration.wifi_and_cell_id_reporting_state);

    for_each_provider([](const Provider::Ptr& provider)
    {
        provider->state_controller()->stop_position_updates();
        provider->state_controller()->stop_heading_updates();
        provider->state_controller()->stop_velocity_updates();
    });
}

cul::ProviderSelection cul::Engine::determine_provider_selection_for_criteria(const cul::Criteria& criteria)
{
    return provider_selection_policy->determine_provider_selection_for_criteria(criteria, *this);
}

void cul::Engine::add_provider(const cul::Provider::Ptr& impl)
{
    if (!impl)
        throw std::runtime_error("Cannot add null provider");

    auto provider = std::make_shared<StateTrackingProvider>(impl);

    // We synchronize to the engine state.
    if (provider->requires(Provider::Requirements::satellites) && configuration.satellite_based_positioning_state == SatelliteBasedPositioningState::off)
        provider->state_controller()->disable();

    if (configuration.engine_state == Engine::Status::off)
        provider->state_controller()->disable();

    // We wire up changes in the engine's configuration to the respective slots
    // of the provider.
    auto cp = updates.last_known_location.changed().connect([provider](const cul::Optional<cul::Update<cul::Position>>& pos)
    {
        if (pos)
        {
            provider->on_reference_location_updated(pos.get());
        }
    });

    auto cv = updates.last_known_velocity.changed().connect([provider](const cul::Optional<cul::Update<cul::Velocity>>& velocity)
    {
        if (velocity)
        {
            provider->on_reference_velocity_updated(velocity.get());
        }
    });

    auto ch = updates.last_known_heading.changed().connect([provider](const cul::Optional<cul::Update<cul::Heading>>& heading)
    {
        if (heading)
        {
            provider->on_reference_heading_updated(heading.get());
        }
    });

    auto cr = configuration.wifi_and_cell_id_reporting_state.changed().connect([provider](cul::WifiAndCellIdReportingState state)
    {
        provider->on_wifi_and_cell_reporting_state_changed(state);
    });

    // And do the reverse: Satellite visibility updates are funneled via the engine's configuration.
    auto cs = provider->updates().svs.connect([this](const cul::Update<std::set<cul::SpaceVehicle>>& src)
    {
        updates.visible_space_vehicles.update([src](std::map<cul::SpaceVehicle::Key, cul::SpaceVehicle>& dest)
        {
            for(auto& sv : src.value)
            {
                dest[sv.key] = sv;
            }

            return true;
        });
    });

    // We are a bit dumb and just take any position update as new reference.
    // We should come up with a better heuristic here.
    auto cpr = provider->updates().position.connect([this](const cul::Update<cul::Position>& src)
    {
        updates.last_known_location = update_policy->verify_update(src);
    });

    auto cps = provider->state().changed().connect([this](const StateTrackingProvider::State&)
    {
        bool is_any_active = false;

        std::lock_guard<std::recursive_mutex> lg(guard);
        for (const auto& pair : providers)
            is_any_active = pair.first->state() == StateTrackingProvider::State::active;

        configuration.engine_state = is_any_active ? Engine::Status::active : Engine::Status::on;
    });

    std::lock_guard<std::recursive_mutex> lg(guard);
    providers.emplace(provider, std::move(ProviderConnections{cp, ch, cv, cr, cs, cpr, cps}));
}

void cul::Engine::for_each_provider(const std::function<void(const Provider::Ptr&)>& enumerator) const noexcept
{
    std::lock_guard<std::recursive_mutex> lg(guard);
    for (const auto& provider : providers)
    {
        try
        {
            enumerator(provider.first);
        }
        catch(const std::exception& e)
        {
            VLOG(1) << e.what();
        }
    }
}

namespace std
{
template<>
struct hash<cul::Engine::Status>
{
    std::size_t operator()(const cul::Engine::Status& s) const
    {
        static const std::hash<std::uint32_t> hash;
        return hash(static_cast<std::uint32_t>(s));
    }
};
}

std::ostream& cul::operator<<(std::ostream& out, cul::Engine::Status state)
{
    static constexpr const char* the_unknown_state
    {
        "Engine::Status::unknown"
    };

    static const std::unordered_map<location::Engine::Status, std::string> lut
    {
        {cul::Engine::Status::off, "Engine::Status::off"},
        {cul::Engine::Status::on, "Engine::Status::on"},
        {cul::Engine::Status::active, "Engine::Status::active"}
    };

    auto it = lut.find(state);
    if (it != lut.end())
        out << it->second;
    else
        out << the_unknown_state;

    return out;
}

/** @brief Parses the status from the given stream. */
std::istream& cul::operator>>(std::istream& in, cul::Engine::Status& state)
{
    static const std::unordered_map<std::string, cul::Engine::Status> lut
    {
        {"Engine::Status::off", cul::Engine::Status::off},
        {"Engine::Status::on", cul::Engine::Status::on},
        {"Engine::Status::active", cul::Engine::Status::active}
    };

    std::string s; in >> s;
    auto it = lut.find(s);
    if (it != lut.end())
        state = it->second;
    else throw std::runtime_error
    {
        "cul::operator>>(std::istream&, Engine::Status&): "
        "Could not resolve state " + s
    };

    return in;
}
