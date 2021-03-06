// -*- C++ -*-
#ifndef _Field_h_
#define _Field_h_

#include "UniverseObject.h"

/** a class representing a region of space */
class Field : public UniverseObject {
public:
    /** \name Structors */ //@{
    Field();                                        ///< default ctor
    Field(const std::string& field_type, double x, double y, double radius);

    virtual Field*              Clone(int empire_id = ALL_EMPIRES) const;   ///< returns new copy of this Field
    //@}

    /** \name Accessors */ //@{
    virtual std::vector<std::string>
                                Tags() const;                                       ///< returns all tags this object has
    virtual bool                HasTag(const std::string& name) const;              ///< returns true iff this object has the tag with the indicated \a name

    virtual const std::string&  TypeName() const;   ///< returns user-readable string indicating the type of UniverseObject this is
    virtual UniverseObjectType  ObjectType() const;
    virtual std::string         Dump() const;

    const std::string&          FieldTypeName() const { return m_type_name; }
    virtual UniverseObject*     Accept(const UniverseObjectVisitor& visitor) const;

    bool                        InField(const UniverseObject* obj) const;
    bool                        InField(double x, double y) const;

    virtual const std::string&  PublicName(int empire_id) const;
    //@}

    /** \name Mutators */ //@{
    virtual void                Copy(const UniverseObject* copied_object, int empire_id = ALL_EMPIRES);
    //@}

protected:
    virtual void                ResetTargetMaxUnpairedMeters();

private:
    virtual void                ClampMeters();

    std::string     m_type_name;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};


/** A specification for a type of field. */
class FieldType {
public:
    /** \name Structors */ //@{
    /** default ctor */
    FieldType() :
        m_name(""),
        m_description(""),
        m_stealth(0.0),
        //m_location(0),
        m_effects(0),
        m_graphic("")
    {}

    /** basic ctor */
    FieldType(const std::string& name, const std::string& description,
              double stealth, const std::vector<std::string>& tags,
//              const Condition::ConditionBase* location,
              const std::vector<boost::shared_ptr<const Effect::EffectsGroup> >& effects,
              const std::string& graphic);
    ~FieldType(); ///< dtor
    //@}

    /** \name Accessors */ //@{
    const std::string&              Name() const            { return m_name; }          ///< returns the unique name for this type of field
    const std::string&              Description() const     { return m_description; }   ///< returns a text description of this type of building
    std::string                     Dump() const;                                       ///< returns a data file format representation of this object
    double                          Stealth() const         { return m_stealth; }       ///< returns stealth of field type
    const std::vector<std::string>& Tags() const            { return m_tags; }
//    const Condition::ConditionBase* Location() const        { return m_location; }      ///< returns the condition that determines the locations where this building can be produced
    const std::vector<boost::shared_ptr<const Effect::EffectsGroup> >&
                                    Effects() const         { return m_effects; }       ///< returns the EffectsGroups that encapsulate the effects of this FieldType
    const std::string&              Graphic() const         { return m_graphic; }       ///< returns the name of the grapic file for this field type
    //@}

private:
    std::string                                                 m_name;
    std::string                                                 m_description;
    double                                                      m_stealth;
    std::vector<std::string>                                    m_tags;
//    const Condition::ConditionBase*                             m_location;
    std::vector<boost::shared_ptr<const Effect::EffectsGroup> > m_effects;
    std::string                                                 m_graphic;
};


class FieldTypeManager {
public:
    typedef std::map<std::string, FieldType*>::const_iterator iterator;

    /** \name Accessors */ //@{
    /** returns the field type with the name \a name; you should use the
      * free function GetFieldType(...) instead, mainly to save some typing. */
    const FieldType*            GetFieldType(const std::string& name) const;

    /** iterator to the first field type */
    iterator                    begin() const   { return m_field_types.begin(); }

    /** iterator to the last + 1th field type */
    iterator                    end() const     { return m_field_types.end(); }

    /** returns the instance of this singleton class; you should use the free
      * function GetFieldTypeManager() instead */
    static FieldTypeManager&    GetFieldTypeManager();
    //@}
private:
    FieldTypeManager();
    ~FieldTypeManager();
    std::map<std::string, FieldType*>   m_field_types;
    static FieldTypeManager*            s_instance;
};

/** returns the singleton field type manager */
FieldTypeManager& GetFieldTypeManager();

/** Returns the BuildingType specification object for a field of
  * type \a name.  If no such FieldType exists, 0 is returned instead. */
const FieldType* GetFieldType(const std::string& name);


#endif // _Ship_h_
