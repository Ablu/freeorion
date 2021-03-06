// -*- C++ -*-
#ifndef _Ship_h_
#define _Ship_h_

#include "UniverseObject.h"
#include "Meter.h"

class Fighter;
class Fleet;
class ShipDesign;

/** a class representing a single FreeOrion ship*/
class Ship : public UniverseObject {
public:
    // map from part type name to (number of parts in the design of that type,
    // number of fighters (or missiles) available of that type) pairs
    typedef std::map<std::string, std::pair<std::size_t, std::size_t> > ConsumablesMap;

    typedef std::map<std::pair<MeterType, std::string>, Meter>          PartMeterMap;

    /** \name Structors */ //@{
    Ship();                                         ///< default ctor
    Ship(int empire_id, int design_id, const std::string& species_name,
         int produced_by_empire_id = ALL_EMPIRES);  ///< general ctor taking ship's empire and design id, species name and production empire id.

    virtual Ship*               Clone(int empire_id = ALL_EMPIRES) const;   ///< returns new copy of this Ship
    //@}

    /** \name Accessors */ //@{
    virtual std::vector<std::string>
                                Tags() const;                                       ///< returns all tags this object has
    virtual bool                HasTag(const std::string& name) const;              ///< returns true iff this object has the tag with the indicated \a name

    virtual const std::string&  TypeName() const;   ///< returns user-readable string indicating the type of UniverseObject this is
    virtual UniverseObjectType  ObjectType() const;
    virtual std::string         Dump() const;

    const ShipDesign*           Design() const;     ///< returns the design of the ship, containing engine type, weapons, etc.
    int                         DesignID() const            { return m_design_id; }             ///< returns the design id of the ship
    int                         FleetID() const             { return m_fleet_id; }              ///< returns the ID of the fleet the ship is residing in
    int                         ProducedByEmpireID() const  { return m_produced_by_empire_id; } ///< returns the empire ID of the empire that produced this ship

    virtual const std::string&  PublicName(int empire_id) const;

    bool                        IsMonster() const;
    bool                        IsArmed() const;
    bool                        CanColonize() const;
    bool                        HasTroops() const;
    const std::string&          SpeciesName() const         { return m_species_name; }
    float                       Speed() const;

    const ConsumablesMap&       Fighters() const            { return m_fighters; }
    const ConsumablesMap&       Missiles() const            { return m_missiles; }

    virtual UniverseObject*     Accept(const UniverseObjectVisitor& visitor) const;

    virtual float               NextTurnCurrentMeterValue(MeterType type) const;    ///< returns expected value of  specified meter current value on the next turn

    bool                        OrderedScrapped() const         { return m_ordered_scrapped; }          ///< returns true iff this ship has been ordered scrapped, or false otherwise
    int                         OrderedColonizePlanet() const   { return m_ordered_colonize_planet_id; }///< returns the ID of the planet this ship has been ordered to colonize, or INVALID_OBJECT_ID if this ship hasn't been ordered to colonize a planet
    int                         OrderedInvadePlanet() const     { return m_ordered_invade_planet_id; }  ///< returns the ID of the planet this ship has been ordered to invade with ground troops, or INVALID_OBJECT_ID if this ship hasn't been ordered to invade a planet
    int                         LastTurnActiveInCombat() const  { return m_last_turn_active_in_combat; }///< returns the last turn this ship has been actively involved in combat

    const PartMeterMap&         PartMeters() const { return m_part_meters; }                                ///< returns this Ship's part meters
    const Meter*                GetPartMeter(MeterType type, const std::string& part_name) const;           ///< returns the requested part Meter, or 0 if no such part Meter of that type is found in this ship for that part name
    float                       CurrentPartMeterValue(MeterType type, const std::string& part_name) const;  ///< returns current value of the specified part meter \a type for the specified part name
    float                       InitialPartMeterValue(MeterType type, const std::string& part_name) const;  ///< returns this turn's initial value for the speicified part meter \a type for the specified part name
    //@}

    /** \name Mutators */ //@{
    virtual void    Copy(const UniverseObject* copied_object, int empire_id = ALL_EMPIRES);

    void            SetFleetID(int fleet_id);                                   ///< sets the ID of the fleet the ship resides in

    void            Resupply();

    void            AddFighters(const std::string& part_name, std::size_t n);
    void            RemoveFighters(const std::string& part_name, std::size_t n);
    void            RemoveMissiles(const std::string& part_name, std::size_t n);

    void            SetSpecies(const std::string& species_name);

    virtual void    MoveTo(double x, double y);

    void            SetOrderedScrapped(bool b = true);                          ///< flags ship for scrapping
    void            SetColonizePlanet(int planet_id);                           ///< marks ship to colonize the indicated planet
    void            ClearColonizePlanet();                                      ///< marks ship to colonize no planets
    void            SetInvadePlanet(int planet_id);                             ///< marks ship to invade the indicated planet
    void            ClearInvadePlanet();                                        ///< marks ship to invade no planets
    void            SetLastTurnActiveInCombat(int turn) { m_last_turn_active_in_combat = turn; } ///< sets the last turn this ship was actively involved in combat

    Meter*          GetPartMeter(MeterType type, const std::string& part_name); ///< returns the requested Meter, or 0 if no such Meter of that type is found in this object
    //@}

protected:
    virtual void    ResetTargetMaxUnpairedMeters();

private:
    virtual void    PopGrowthProductionResearchPhase();
    virtual void    ClampMeters();

    int             m_design_id;
    int             m_fleet_id;
    bool            m_ordered_scrapped;
    int             m_ordered_colonize_planet_id;
    int             m_ordered_invade_planet_id;
    int             m_last_turn_active_in_combat;
    ConsumablesMap  m_fighters;
    ConsumablesMap  m_missiles;
    PartMeterMap    m_part_meters;
    std::string     m_species_name;
    int             m_produced_by_empire_id;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

#endif // _Ship_h_
