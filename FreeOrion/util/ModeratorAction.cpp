#include "ModeratorAction.h"

#include "AppInterface.h"
#include "../universe/Universe.h"
#include "../universe/UniverseObject.h"
#include "../universe/System.h"
#include "../universe/Planet.h"
#include "../util/Directories.h"

#include <boost/lexical_cast.hpp>
#include <boost/serialization/export.hpp>
#include <boost/filesystem/fstream.hpp>


Moderator::ModeratorAction::ModeratorAction()
{}

/////////////////////////////////////////////////////
// Moderator::DestroyUniverseObject
/////////////////////////////////////////////////////
Moderator::DestroyUniverseObject::DestroyUniverseObject()
{}

Moderator::DestroyUniverseObject::DestroyUniverseObject(int object_id) :
    m_object_id(object_id)
{}

void Moderator::DestroyUniverseObject::Execute() const
{ GetUniverse().RecursiveDestroy(m_object_id); }

std::string Moderator::DestroyUniverseObject::Dump() const {
    std::string retval = "Moderator::DestroyUniverseObject object_id = "
                       + boost::lexical_cast<std::string>(m_object_id);
    return retval;
}

/////////////////////////////////////////////////////
// Moderator::SetOwner
/////////////////////////////////////////////////////
Moderator::SetOwner::SetOwner()
{}

Moderator::SetOwner::SetOwner(int object_id, int new_owner_empire_id) :
    m_object_id(object_id),
    m_new_owner_empire_id(new_owner_empire_id)
{}

void Moderator::SetOwner::Execute() const {
    UniverseObject* obj = GetUniverseObject(m_object_id);
    if (!obj) {
        Logger().errorStream() << "Moderator::SetOwner::Execute couldn't get object with id: " << m_object_id;
        return;
    }
    obj->SetOwner(m_new_owner_empire_id);
}

std::string Moderator::SetOwner::Dump() const {
    std::string retval = "Moderator::SetOwner object_id = "
                       + boost::lexical_cast<std::string>(m_object_id)
                       + " new_owner_empire_id = "
                       + boost::lexical_cast<std::string>(m_new_owner_empire_id);
    return retval;
}

/////////////////////////////////////////////////////
// Moderator::AddStarlane
/////////////////////////////////////////////////////
Moderator::AddStarlane::AddStarlane() :
    m_id_1(INVALID_OBJECT_ID),
    m_id_2(INVALID_OBJECT_ID)
{}

Moderator::AddStarlane::AddStarlane(int system_1_id, int system_2_id) :
    m_id_1(system_1_id),
    m_id_2(system_2_id)
{}

void Moderator::AddStarlane::Execute() const {
    System* sys1 = GetSystem(m_id_1);
    if (!sys1) {
        Logger().errorStream() << "Moderator::AddStarlane::Execute couldn't get system with id: " << m_id_1;
        return;
    }
    System* sys2 = GetSystem(m_id_2);
    if (!sys2) {
        Logger().errorStream() << "Moderator::AddStarlane::Execute couldn't get system with id: " << m_id_2;
        return;
    }
    sys1->AddStarlane(m_id_2);
    sys2->AddStarlane(m_id_1);
}

std::string Moderator::AddStarlane::Dump() const {
    std::string retval = "Moderator::AddStarlane system_id_1 = "
                       + boost::lexical_cast<std::string>(m_id_1)
                       + " system_id_2 = "
                       + boost::lexical_cast<std::string>(m_id_2);
    return retval;
}

/////////////////////////////////////////////////////
// Moderator::CreateSystem
/////////////////////////////////////////////////////
Moderator::CreateSystem::CreateSystem() :
    m_x(UniverseObject::INVALID_POSITION),
    m_y(UniverseObject::INVALID_POSITION),
    m_star_type(STAR_NONE)
{}

Moderator::CreateSystem::CreateSystem(double x, double y, StarType star_type) :
    m_x(x),
    m_y(y),
    m_star_type(star_type)
{}

namespace {
    void LoadSystemNames(std::list<std::string>& names) {
        boost::filesystem::ifstream ifs(GetResourceDir() / "starnames.txt");
        while (ifs) {
            std::string latest_name;
            std::getline(ifs, latest_name);
            if (!latest_name.empty())
                names.push_back(latest_name.substr(0, latest_name.find_last_not_of(" \t") + 1)); // strip off trailing whitespace
        }
    }

    std::string GenerateSystemName() {
        static std::list<std::string> star_names;
        if (star_names.empty())
            LoadSystemNames(star_names);

        const ObjectMap& objects = Objects();
        std::vector<const System*> systems = objects.FindObjects<System>();

        // pick a name for the system
        for (std::list<std::string>::const_iterator it = star_names.begin(); it != star_names.end(); ++it) {
            // does an existing system have this name?
            bool dupe = false;
            for (std::vector<const System*>::const_iterator sys_it = systems.begin();
                 sys_it != systems.end(); ++sys_it)
            {
                if ((*sys_it)->Name() == *it) {
                    dupe = true;
                    break;  // another system has this name. skip to next potential name.
                }
            }
            if (!dupe)
                return *it; // no systems have this name yet. use it.
        }
        return "";  // fallback to empty name.
    }
}

void Moderator::CreateSystem::Execute() const {
    const int MAX_SYSTEM_ORBITS = 9;    // hard coded value in UniverseServer.cpp

    System* system = new System(m_star_type, MAX_SYSTEM_ORBITS, GenerateSystemName(), m_x, m_y);
    if (!system) {
        Logger().errorStream() << "CreateSystem::Execute couldn't create system!";
        return;
    }

    int new_id = GetNewObjectID();
    GetUniverse().InsertID(system, new_id);
}

std::string Moderator::CreateSystem::Dump() const {
    std::string retval = "Moderator::CreateSystem x = "
                       + boost::lexical_cast<std::string>(m_x)
                       + " y = "
                       + boost::lexical_cast<std::string>(m_y)
                       + " star_type = "
                       + boost::lexical_cast<std::string>(m_star_type);
    return retval;
}

/////////////////////////////////////////////////////
// Moderator::CreatePlanet
/////////////////////////////////////////////////////
Moderator::CreatePlanet::CreatePlanet() :
    m_system_id(INVALID_OBJECT_ID),
    m_planet_type(PT_SWAMP),
    m_planet_size(SZ_MEDIUM)
{}

Moderator::CreatePlanet::CreatePlanet(int system_id, PlanetType planet_type, PlanetSize planet_size) :
    m_system_id(system_id),
    m_planet_type(planet_type),
    m_planet_size(planet_size)
{}

void Moderator::CreatePlanet::Execute() const {
    System* location = GetSystem(m_system_id);
    if (!location) {
        Logger().errorStream() << "CreatePlanet::Execute couldn't get a System object at which to create the planet";
        return;
    }

    //  determine if and which orbits are available
    std::set<int> free_orbits = location->FreeOrbits();
    if (free_orbits.empty()) {
        Logger().errorStream() << "CreatePlanet::Execute couldn't find any free orbits in system where planet was to be created";
        return;
    }

    Planet* planet = new Planet(m_planet_type, m_planet_size);
    if (!planet) {
        Logger().errorStream() << "CreatePlanet::Execute unable to create new Planet object";
        return;
    }
    int new_planet_id = GetNewObjectID();
    GetUniverse().InsertID(planet, new_planet_id);

    int orbit = *(free_orbits.begin());
    location->Insert(planet, orbit);
}

std::string Moderator::CreatePlanet::Dump() const {
    std::string retval = "Moderator::CreatePlanet system_id = "
                       + boost::lexical_cast<std::string>(m_system_id)
                       + " planet_type = "
                       + boost::lexical_cast<std::string>(m_planet_type)
                       + " planet_size = "
                       + boost::lexical_cast<std::string>(m_planet_size);
    return retval;
}
