/*
 * Copyright © 2012-2014 Canonical Ltd.
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
#ifndef CACHED_RADIO_CELL_H_
#define CACHED_RADIO_CELL_H_

#include <com/ubuntu/location/connectivity/radio_cell.h>

#include "ofono.h"

namespace
{

struct CachedRadioCell : public com::ubuntu::location::connectivity::RadioCell
{
    typedef std::shared_ptr<CachedRadioCell> Ptr;

    static const std::map<std::string, com::ubuntu::location::connectivity::RadioCell::Type>& type_lut()
    {
        static const std::map<std::string, com::ubuntu::location::connectivity::RadioCell::Type> lut
        {
            {
                org::Ofono::Manager::Modem::NetworkRegistration::Technology::gsm(),
                com::ubuntu::location::connectivity::RadioCell::Type::gsm
            },
            {
                org::Ofono::Manager::Modem::NetworkRegistration::Technology::lte(),
                com::ubuntu::location::connectivity::RadioCell::Type::lte
            },
            {
                org::Ofono::Manager::Modem::NetworkRegistration::Technology::umts(),
                com::ubuntu::location::connectivity::RadioCell::Type::umts
            },
            {
                org::Ofono::Manager::Modem::NetworkRegistration::Technology::edge(),
                com::ubuntu::location::connectivity::RadioCell::Type::unknown
            },
            {
                org::Ofono::Manager::Modem::NetworkRegistration::Technology::hspa(),
                com::ubuntu::location::connectivity::RadioCell::Type::unknown
            },
            {std::string(), com::ubuntu::location::connectivity::RadioCell::Type::unknown}
        };

        return lut;
    };

    CachedRadioCell(const org::Ofono::Manager::Modem& modem)
        : RadioCell(), radio_type(Type::gsm), modem(modem), detail{Gsm()}
    {
        auto technology =
                modem.network_registration.get<
                    org::Ofono::Manager::Modem::NetworkRegistration::Technology
                >();

        auto it = type_lut().find(technology);

        if (it == type_lut().end()) throw std::runtime_error
        {
            "Unknown technology for connected cell: " + technology
        };

        if (it->second == com::ubuntu::location::connectivity::RadioCell::Type::unknown) throw std::runtime_error
        {
            "Unknown technology for connected cell: " + technology
        };

        radio_type = it->second;

        auto lac =
                modem.network_registration.get<
                    org::Ofono::Manager::Modem::NetworkRegistration::LocationAreaCode
                >();

        int cell_id =
                modem.network_registration.get<
                    org::Ofono::Manager::Modem::NetworkRegistration::CellId
                >();

        auto strength =
                modem.network_registration.get<
                    org::Ofono::Manager::Modem::NetworkRegistration::Strength
                >();

        std::stringstream ssmcc
        {
            modem.network_registration.get<
                org::Ofono::Manager::Modem::NetworkRegistration::MobileCountryCode
            >()
        };
        int mcc{0}; ssmcc >> mcc;
        std::stringstream ssmnc
        {
            modem.network_registration.get<
                org::Ofono::Manager::Modem::NetworkRegistration::MobileNetworkCode
            >()
        };
        int mnc{0}; ssmnc >> mnc;

        switch(radio_type)
        {
        case com::ubuntu::location::connectivity::RadioCell::Type::gsm:
        {
            com::ubuntu::location::connectivity::RadioCell::Gsm gsm
            {
                com::ubuntu::location::connectivity::RadioCell::Gsm::MCC{mcc},
                com::ubuntu::location::connectivity::RadioCell::Gsm::MNC{mnc},
                com::ubuntu::location::connectivity::RadioCell::Gsm::LAC{lac},
                com::ubuntu::location::connectivity::RadioCell::Gsm::ID{cell_id},
                com::ubuntu::location::connectivity::RadioCell::Gsm::SignalStrength::from_percent(strength/100.f)
            };
            VLOG(1) << gsm;
            detail.gsm = gsm;
            break;
        }
        case com::ubuntu::location::connectivity::RadioCell::Type::lte:
        {
            com::ubuntu::location::connectivity::RadioCell::Lte lte
            {
                com::ubuntu::location::connectivity::RadioCell::Lte::MCC{mcc},
                com::ubuntu::location::connectivity::RadioCell::Lte::MNC{mnc},
                com::ubuntu::location::connectivity::RadioCell::Lte::TAC{lac},
                com::ubuntu::location::connectivity::RadioCell::Lte::ID{cell_id},
                com::ubuntu::location::connectivity::RadioCell::Lte::PID{},
                com::ubuntu::location::connectivity::RadioCell::Lte::SignalStrength::from_percent(strength/100.f)
            };
            VLOG(1) << lte;
            detail.lte = lte;
            break;
        }
        case com::ubuntu::location::connectivity::RadioCell::Type::umts:
        {
            com::ubuntu::location::connectivity::RadioCell::Umts umts
            {
                com::ubuntu::location::connectivity::RadioCell::Umts::MCC{mcc},
                com::ubuntu::location::connectivity::RadioCell::Umts::MNC{mnc},
                com::ubuntu::location::connectivity::RadioCell::Umts::LAC{lac},
                com::ubuntu::location::connectivity::RadioCell::Umts::ID{cell_id},
                com::ubuntu::location::connectivity::RadioCell::Umts::SignalStrength::from_percent(strength/100.f)
            };
            VLOG(1) << umts;
            detail.umts = umts;
            break;
        }
        default:
            break;
        }

        modem.signals.property_changed->connect([this](const std::tuple<std::string, core::dbus::types::Variant>& tuple)
        {
            on_modem_property_changed(tuple);
        });

        modem.network_registration.signals.property_changed->connect([this](const std::tuple<std::string, core::dbus::types::Variant>& tuple)
        {
            on_network_registration_property_changed(tuple);
        });
    }

    const core::Signal<>& changed() const override
    {
        return on_changed;
    }

    com::ubuntu::location::connectivity::RadioCell::Type type() const override
    {
        return radio_type;
    }

    const com::ubuntu::location::connectivity::RadioCell::Gsm& gsm() const override
    {
        if (radio_type != com::ubuntu::location::connectivity::RadioCell::Type::gsm)
            throw std::runtime_error("Bad access to unset network type.");

        return detail.gsm;
    }

    const com::ubuntu::location::connectivity::RadioCell::Umts& umts() const override
    {
        if (radio_type != RadioCell::Type::umts)
            throw std::runtime_error("Bad access to unset network type.");

        return detail.umts;
    }

    const com::ubuntu::location::connectivity::RadioCell::Lte& lte() const override
    {
        if (radio_type != RadioCell::Type::lte)
            throw std::runtime_error("Bad access to unset network type.");

        return detail.lte;
    }

    void on_modem_property_changed(const std::tuple<std::string, core::dbus::types::Variant>& tuple)
    {
        VLOG(10) << "Property on modem " << modem.object->path() << " changed: " << std::get<0>(tuple);
    }

    void on_network_registration_property_changed(const std::tuple<std::string, core::dbus::types::Variant>& tuple)
    {
        VLOG(10) << "Property changed on modem " << modem.object->path() << " for network registration: " << std::get<0>(tuple);

        const auto& key = std::get<0>(tuple);
        const auto& variant = std::get<1>(tuple);

        if (key == org::Ofono::Manager::Modem::NetworkRegistration::Technology::name())
        {
            auto value = variant.as<
                        org::Ofono::Manager::Modem::NetworkRegistration::Technology::ValueType
                    >();

            auto it = type_lut().find(value);

            if (it == type_lut().end())
            {
                LOG(WARNING) << "Unknown technology for connected cell: " << value;
                return;
            }

            if (it->second == com::ubuntu::location::connectivity::RadioCell::Type::unknown)
            {
                LOG(WARNING) << "Unknown technology for connected cell: " + value;
                return;
            }

            if (radio_type == it->second)
                return;

            switch(radio_type)
            {
            case com::ubuntu::location::connectivity::RadioCell::Type::gsm:
                switch(it->second)
                {
                case com::ubuntu::location::connectivity::RadioCell::Type::umts:
                    detail.umts.location_area_code = detail.gsm.location_area_code;
                    detail.umts.mobile_network_code = detail.gsm.mobile_network_code;
                    detail.umts.mobile_country_code = detail.gsm.mobile_country_code;
                    detail.umts.strength.reset();
                    detail.umts.id.reset();
                    break;
                case com::ubuntu::location::connectivity::RadioCell::Type::lte:
                    detail.lte.tracking_area_code = detail.gsm.location_area_code;
                    detail.lte.mobile_network_code = detail.gsm.mobile_network_code;
                    detail.lte.mobile_country_code = detail.gsm.mobile_country_code;
                    detail.lte.strength.reset();
                    detail.lte.id.reset();
                    break;
                }
                break;
            case com::ubuntu::location::connectivity::RadioCell::Type::umts:
                switch(it->second)
                {
                case com::ubuntu::location::connectivity::RadioCell::Type::gsm:
                    detail.gsm.location_area_code = detail.umts.location_area_code;
                    detail.gsm.mobile_network_code = detail.umts.mobile_network_code;
                    detail.gsm.mobile_country_code = detail.umts.mobile_country_code;
                    detail.gsm.strength.reset();
                    detail.gsm.id.reset();
                    break;
                case com::ubuntu::location::connectivity::RadioCell::Type::lte:
                    detail.lte.tracking_area_code = detail.umts.location_area_code;
                    detail.lte.mobile_network_code = detail.umts.mobile_network_code;
                    detail.lte.mobile_country_code = detail.umts.mobile_country_code;
                    detail.lte.strength.reset();
                    detail.lte.id.reset();
                    break;
                }
                break;
            case com::ubuntu::location::connectivity::RadioCell::Type::lte:
                switch(it->second)
                {
                case com::ubuntu::location::connectivity::RadioCell::Type::gsm:
                    detail.gsm.location_area_code = detail.lte.tracking_area_code;
                    detail.gsm.mobile_network_code = detail.lte.mobile_network_code;
                    detail.gsm.mobile_country_code = detail.lte.mobile_country_code;
                    detail.gsm.strength.reset();
                    detail.gsm.id.reset();
                    break;
                case com::ubuntu::location::connectivity::RadioCell::Type::umts:
                    detail.umts.location_area_code = detail.lte.tracking_area_code;
                    detail.umts.mobile_network_code = detail.lte.mobile_network_code;
                    detail.umts.mobile_country_code = detail.lte.mobile_country_code;
                    detail.umts.strength.reset();
                    detail.umts.id.reset();
                    break;
                }
            default:
                break;
            };

            radio_type = it->second;
            on_changed();
        }

        if (key == org::Ofono::Manager::Modem::NetworkRegistration::CellId::name())
        {
            auto value = variant.as<
                        org::Ofono::Manager::Modem::NetworkRegistration::CellId::ValueType
                    >();

            switch(radio_type)
            {
            case com::ubuntu::location::connectivity::RadioCell::Type::gsm:
                detail.gsm.id.set(value);
                VLOG(1) << detail.gsm;
                break;
            case com::ubuntu::location::connectivity::RadioCell::Type::umts:
                detail.umts.id.set(value);
                VLOG(1) << detail.umts;
                break;
            case com::ubuntu::location::connectivity::RadioCell::Type::lte:
                detail.lte.id.set(value);
                VLOG(1) << detail.lte;
                break;
            default:
                break;
            };

            on_changed();
        }

        if (key == org::Ofono::Manager::Modem::NetworkRegistration::LocationAreaCode::name())
        {
            auto value = variant.as<
                        org::Ofono::Manager::Modem::NetworkRegistration::LocationAreaCode::ValueType
                    >();
            switch(radio_type)
            {
            case com::ubuntu::location::connectivity::RadioCell::Type::gsm:
                detail.gsm.location_area_code.set(value);
                VLOG(1) << detail.gsm;
                break;
            case com::ubuntu::location::connectivity::RadioCell::Type::umts:
                detail.umts.location_area_code.set(value);
                VLOG(1) << detail.umts;
                break;
            case com::ubuntu::location::connectivity::RadioCell::Type::lte:
                detail.lte.tracking_area_code.set(value);
                VLOG(1) << detail.lte;
                break;
            default:
                break;
            };

            on_changed();
        }

        if (key == org::Ofono::Manager::Modem::NetworkRegistration::MobileCountryCode::name())
        {
            std::stringstream ss
            {
                variant.as<
                    org::Ofono::Manager::Modem::NetworkRegistration::MobileCountryCode::ValueType
                >()
            };
            int value{-1}; ss >> value;

            switch(radio_type)
            {
            case com::ubuntu::location::connectivity::RadioCell::Type::gsm:
                detail.gsm.mobile_country_code.set(value);
                VLOG(1) << detail.gsm;
                break;
            case com::ubuntu::location::connectivity::RadioCell::Type::umts:
                detail.umts.mobile_country_code.set(value);
                VLOG(1) << detail.umts;
                break;
            case com::ubuntu::location::connectivity::RadioCell::Type::lte:
                detail.lte.mobile_country_code.set(value);
                VLOG(1) << detail.lte;
                break;
            default:
                break;
            };

            on_changed();
        }

        if (key == org::Ofono::Manager::Modem::NetworkRegistration::MobileNetworkCode::name())
        {
            std::stringstream ss
            {
                variant.as<
                    org::Ofono::Manager::Modem::NetworkRegistration::MobileNetworkCode::ValueType
                >()
            };
            int value{-1}; ss >> value;

            switch(radio_type)
            {
            case com::ubuntu::location::connectivity::RadioCell::Type::gsm:
                detail.gsm.mobile_network_code.set(value);
                VLOG(1) << detail.gsm;
                break;
            case com::ubuntu::location::connectivity::RadioCell::Type::umts:
                detail.umts.mobile_network_code.set(value);
                VLOG(1) << detail.umts;
                break;
            case com::ubuntu::location::connectivity::RadioCell::Type::lte:
                detail.lte.mobile_network_code.set(value);
                VLOG(1) << detail.lte;
                break;
            default:
                break;
            };

            on_changed();
        }

        if (key == org::Ofono::Manager::Modem::NetworkRegistration::Strength::name())
        {
            auto value = variant.as<
                        org::Ofono::Manager::Modem::NetworkRegistration::Strength::ValueType
                    >();

            switch(radio_type)
            {
            case com::ubuntu::location::connectivity::RadioCell::Type::gsm:
                detail.gsm.strength
                        = com::ubuntu::location::connectivity::RadioCell::Gsm::SignalStrength::from_percent(value/100.f);
                VLOG(1) << detail.gsm;
                break;
            case com::ubuntu::location::connectivity::RadioCell::Type::umts:
                detail.umts.strength
                        = com::ubuntu::location::connectivity::RadioCell::Umts::SignalStrength::from_percent(value/100.f);
                VLOG(1) << detail.umts;
                break;
            case com::ubuntu::location::connectivity::RadioCell::Type::lte:
                detail.lte.strength
                        = com::ubuntu::location::connectivity::RadioCell::Lte::SignalStrength::from_percent(value/100.f);
                VLOG(1) << detail.lte;
                break;
            default:
                break;
            };

            on_changed();
        }
    }

    /** @cond */
    core::Signal<> on_changed;
    Type radio_type;
    org::Ofono::Manager::Modem modem;

    struct None {};

    union Detail
    {
        Detail() : none(None{})
        {
        }

        Detail(const com::ubuntu::location::connectivity::RadioCell::Gsm& gsm) : gsm(gsm)
        {
        }

        Detail(const com::ubuntu::location::connectivity::RadioCell::Umts& umts) : umts(umts)
        {
        }

        Detail(const com::ubuntu::location::connectivity::RadioCell::Lte& lte) : lte(lte)
        {
        }

        None none;
        Gsm gsm;
        Umts umts;
        Lte lte;
    } detail;
    /** @endcond */
};
}

#endif // CACHED_RADIO_CELL_H_