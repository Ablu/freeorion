// -*- C++ -*-
#ifndef _Planet_h_
#define _Planet_h_

#include "UniverseObject.h"
#include "PopCenter.h"
#include "ResourceCenter.h"
#include "Meter.h"

/** A type that is implicitly convertible to and from double, but which is not
    implicitly convertible among other numeric types. */
class TypesafeDouble {
public:
    TypesafeDouble() : m_value(0.0) {}
    TypesafeDouble(double d) : m_value(d) {}
    operator double () const { return m_value; }

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    { ar & BOOST_SERIALIZATION_NVP(m_value); }

private:
    double m_value;
};

class Day;

/** A value type representing a "year".  A "year" is arbitrarily defined to be 4
    turns. */
class Year : public TypesafeDouble {
public:
    Year() : TypesafeDouble() {}
    Year(double d) : TypesafeDouble(d) {}
    explicit Year(Day d);

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    { ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(TypesafeDouble); }
};

/** A value type representing a "day".  A "day" is arbitrarily defined to be
    1/360 of a "year", and 1/90 of a turn. */
class Day : public TypesafeDouble {
public:
    Day() : TypesafeDouble() {}
    Day(double d) : TypesafeDouble(d) {}
    explicit Day(Year y) : TypesafeDouble(y * 360.0) {}

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    { ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(TypesafeDouble); }
};

/** A value type used to represent an angle in radians. */
class Radian : public TypesafeDouble {
public:
    Radian() : TypesafeDouble() {}
    Radian(double d) : TypesafeDouble(d) {}

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    { ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(TypesafeDouble); }
};

/** A value type used to represent an angle in degrees. */
class Degree : public TypesafeDouble {
public:
    Degree() : TypesafeDouble() {}
    Degree(double d) : TypesafeDouble(d) {}

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    { ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(TypesafeDouble); }
};

/** a class representing a FreeOrion planet. */
class Planet :
    public UniverseObject,
    public PopCenter,
    public ResourceCenter
{
public:
    /** \name Structors */ //@{
    Planet();                                   ///< default ctor
    Planet(PlanetType type, PlanetSize size);   ///< general ctor taking just the planet's type and size

    virtual Planet*             Clone(int empire_id = ALL_EMPIRES) const;  ///< returns new copy of this Planet
    //@}

    /** \name Accessors */ //@{
    virtual std::vector<std::string>
                                Tags() const;                                       ///< returns all tags this object has
    virtual bool                HasTag(const std::string& name) const;              ///< returns true iff this object has the tag with the indicated \a name

    virtual const std::string&  TypeName() const;                       ///< returns user-readable string indicating the type of UniverseObject this is
    virtual UniverseObjectType  ObjectType() const;
    virtual std::string         Dump() const;

    PlanetType                  Type() const                        { return m_type; }
    PlanetType                  OriginalType() const                { return m_original_type; }
    int                         DistanceFromOriginalType() const    { return TypeDifference(m_type, m_original_type); }
    PlanetSize                  Size() const                        { return m_size; }
    int                         SizeAsInt() const;

    PlanetEnvironment           EnvironmentForSpecies(const std::string& species_name = "") const;
    PlanetType                  NextBetterPlanetTypeForSpecies(const std::string& species_name = "") const;
    PlanetType                  NextCloserToOriginalPlanetType() const;
    PlanetType                  ClockwiseNextPlanetType() const;
    PlanetType                  CounterClockwiseNextPlanetType() const;
    PlanetSize                  NextLargerPlanetSize() const;
    PlanetSize                  NextSmallerPlanetSize() const;

    Year                        OrbitalPeriod() const;
    Radian                      InitialOrbitalPosition() const;
    Radian                      OrbitalPositionOnTurn(int turn) const;
    Day                         RotationalPeriod() const;
    Degree                      AxialTilt() const;

    const std::set<int>&        Buildings() const {return m_buildings;}

    virtual bool                        Contains(int object_id) const;  ///< returns true iff this Planet contains a building with ID \a id.
    virtual std::vector<UniverseObject*>FindObjects() const;            ///< returns objects contained within this object
    virtual std::vector<int>            FindObjectIDs() const;          ///< returns ids of objects contained within this object

    virtual std::vector<std::string>    AvailableFoci() const;
    virtual const std::string&          FocusIcon(const std::string& focus_name) const;

    bool                        IsAboutToBeColonized() const    { return m_is_about_to_be_colonized; }
    bool                        IsAboutToBeInvaded() const      { return m_is_about_to_be_invaded; }
    int                         LastTurnAttackedByShip() const  { return m_last_turn_attacked_by_ship; }

    virtual UniverseObject*     Accept(const UniverseObjectVisitor& visitor) const;

    virtual float               InitialMeterValue(MeterType type) const;
    virtual float               CurrentMeterValue(MeterType type) const;
    virtual float               NextTurnCurrentMeterValue(MeterType type) const;

    const std::string&          SurfaceTexture() const          { return m_surface_texture; }
    //@}

    /** \name Mutators */ //@{
    virtual void    Copy(const UniverseObject* copied_object, int empire_id = ALL_EMPIRES);

    virtual Meter*  GetMeter(MeterType type);

    virtual void    SetSystem(int sys);
    virtual void    MoveTo(double x, double y);

    void            SetType(PlanetType type);           ///< sets the type of this Planet to \a type
    void            SetSize(PlanetSize size);           ///< sets the size of this Planet to \a size

    /** Sets the orbital period based on the orbit this planet is in. If
        tidally locked, the rotational period is also adjusted. */
    void            SetOrbitalPeriod(unsigned int orbit);

    void            SetRotationalPeriod(Day days);      ///< sets the rotational period of this planet
    void            SetHighAxialTilt();                 ///< randomly generates a new, high axial tilt

    void            AddBuilding(int building_id);       ///< adds the building to the planet
    bool            RemoveBuilding(int building_id);    ///< removes the building from the planet; returns false if no such building was found

    virtual void    Reset();                            ///< Resets the meters, specials, etc., of a planet to an unowned state.
    virtual void    Depopulate();

    void            Conquer(int conquerer);             ///< Called during combat when a planet changes hands
    void            SetIsAboutToBeColonized(bool b);    ///< Called during colonization when a planet is about to be colonized
    void            ResetIsAboutToBeColonized();        ///< Called after colonization, to reset the number of prospective colonizers to 0
    void            SetIsAboutToBeInvaded(bool b);      ///< Marks planet as being invaded or not, depending on whether \a b is true or false
    void            ResetIsAboutToBeInvaded();          ///< Marks planet as not being invaded
    void            SetLastTurnAttackedByShip(int turn);///< Sets the last turn this planet was attacked by a ship

    void            SetSurfaceTexture(const std::string& texture);
    //@}

    static int      TypeDifference(PlanetType type1, PlanetType type2);

protected:
    virtual void            ResetTargetMaxUnpairedMeters();

private:
    void Init();

    virtual const Meter*    GetMeter(MeterType type) const;

    virtual void            PopGrowthProductionResearchPhase();
    virtual void            ClampMeters();

    virtual Visibility      GetVisibility(int empire_id) const  { return UniverseObject::GetVisibility(empire_id); }
    virtual void            AddMeter(MeterType meter_type)      { UniverseObject::AddMeter(meter_type); }

    std::set<int>           VisibleContainedObjects(int empire_id) const;   ///< returns the subset of m_buildings that is visible to empire with id \a empire_id

    PlanetType      m_type;
    PlanetType      m_original_type;
    PlanetSize      m_size;
    Year            m_orbital_period;
    Radian          m_initial_orbital_position;
    Day             m_rotational_period;
    Degree          m_axial_tilt;

    std::set<int>   m_buildings;

    bool            m_just_conquered;
    bool            m_is_about_to_be_colonized;
    bool            m_is_about_to_be_invaded;
    int             m_last_turn_attacked_by_ship;

    std::string     m_surface_texture;  // intentionally not serialized; set by local effects

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};


// Tactical combat planet geometry free functions:

/** Returns the radius, in tactical combat units, of a planet.  Note that 0.0
    is returned for PlanetSize enumerators that have no size, whereas
    PlanetRadius(SZ_MEDIUM) is returned for unknown values. */
double PlanetRadius(PlanetSize size);

/** Returns the radius, in tactical combat units, of the tube in which an
    asteroid belt lies. */
double AsteroidBeltRadius();

#endif // _Planet_h_


