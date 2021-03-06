#include "Field.h"

#include "Effect.h"
#include "Condition.h"
#include "ValueRef.h"
#include "Enums.h"
#include "../parse/Parse.h"
#include "../util/MultiplayerCommon.h"
#include "../util/OptionsDB.h"
#include "../util/AppInterface.h"
#include "../util/Directories.h"

#include <boost/filesystem/fstream.hpp>

std::string DumpIndent();

extern int g_indent;

namespace {
    boost::shared_ptr<const Effect::EffectsGroup>
    IncreaseMeter(MeterType meter_type, double increase) {
        typedef boost::shared_ptr<const Effect::EffectsGroup> EffectsGroupPtr;
        typedef std::vector<Effect::EffectBase*> Effects;
        Condition::Source* scope = new Condition::Source;
        Condition::Source* activation = new Condition::Source;
        ValueRef::ValueRefBase<double>* vr =
            new ValueRef::Operation<double>(
                ValueRef::PLUS,
                new ValueRef::Variable<double>(std::vector<adobe::name_t>(1, Value_name)),
                new ValueRef::Constant<double>(increase)
            );
        return EffectsGroupPtr(
            new Effect::EffectsGroup(
                scope, activation, Effects(1, new Effect::SetMeter(meter_type, vr))));
    }
}

/////////////////////////////////////////////////
// Field                                       //
/////////////////////////////////////////////////
Field::Field() :
    UniverseObject(),
    m_type_name("")
{}

Field::Field(const std::string& field_type, double x, double y, double radius) :
    UniverseObject("", x, y),
    m_type_name(field_type)
{
    const FieldType* type = GetFieldType(m_type_name);
    if (type)
        Rename(UserString(type->Name()));
    else
        Rename(UserString("ENC_FIELD"));

    UniverseObject::Init();

    AddMeter(METER_STARLANE_SPEED);
    AddMeter(METER_SIZE);

    UniverseObject::GetMeter(METER_SIZE)->Set(radius, radius);
}

Field* Field::Clone(int empire_id) const {
    Visibility vis = GetUniverse().GetObjectVisibilityByEmpire(this->ID(), empire_id);

    if (!(vis >= VIS_BASIC_VISIBILITY && vis <= VIS_FULL_VISIBILITY))
        return 0;

    Field* retval = new Field();
    retval->Copy(this, empire_id);
    return retval;
}

void Field::Copy(const UniverseObject* copied_object, int empire_id) {
    if (copied_object == this)
        return;
    const Field* copied_field = universe_object_cast<Field*>(copied_object);
    if (!copied_field) {
        Logger().errorStream() << "Field::Copy passed an object that wasn't a Field";
        return;
    }

    int copied_object_id = copied_object->ID();
    Visibility vis = GetUniverse().GetObjectVisibilityByEmpire(copied_object_id, empire_id);
    std::set<std::string> visible_specials = GetUniverse().GetObjectVisibleSpecialsByEmpire(copied_object_id, empire_id);

    UniverseObject::Copy(copied_object, vis, visible_specials);

    if (vis >= VIS_BASIC_VISIBILITY) {
        this->m_name =                      copied_field->m_name;
        this->m_type_name =                 copied_field->m_type_name;
    }
}

std::vector<std::string> Field::Tags() const {
    const FieldType* type = GetFieldType(m_type_name);
    if (!type)
        return std::vector<std::string>();
    return type->Tags();
}

bool Field::HasTag(const std::string& name) const {
    const FieldType* type = GetFieldType(m_type_name);
    if (!type)
        return false;
    const std::vector<std::string>& tags = type->Tags();
    for (std::vector<std::string>::const_iterator it = tags.begin(); it != tags.end(); ++it)
        if (*it == name)
            return true;
    return false;
}

const std::string& Field::TypeName() const
{ return UserString("FIELD"); }

UniverseObjectType Field::ObjectType() const
{ return OBJ_FIELD; }

std::string Field::Dump() const {
    std::stringstream os;
    os << UniverseObject::Dump();
    os << " field type: " << m_type_name;
    return os.str();
}

const std::string& Field::PublicName(int empire_id) const {
    // always just return name since fields (as of this writing) don't have owners
    return UserString(m_type_name);
}

UniverseObject* Field::Accept(const UniverseObjectVisitor& visitor) const
{ return visitor.Visit(const_cast<Field* const>(this)); }

bool Field::InField(const UniverseObject* obj) const
{ return obj && InField(obj->X(), obj->Y()); }

bool Field::InField(double x, double y) const {
    const Meter* size_meter = GetMeter(METER_SIZE);
    double radius = 1.0;
    if (size_meter)
        radius = size_meter->Current();

    double dist2 = (x - this->X())*(x - this->X()) + (y - this->Y())*(y - this->Y());
    return dist2 < radius*radius;
}

void Field::ResetTargetMaxUnpairedMeters() {
    UniverseObject::ResetTargetMaxUnpairedMeters();

    GetMeter(METER_STARLANE_SPEED)->ResetCurrent();
    // intentionally not resetting size, so that it is presistant
}

void Field::ClampMeters() {
    UniverseObject::ClampMeters();

    // intentionally not clamping METER_STARLANE_SPEED, to allow negative speeds
    UniverseObject::GetMeter(METER_SIZE)->ClampCurrentToRange();
}

/////////////////////////////////////////////////
// FieldType                                   //
/////////////////////////////////////////////////
FieldType::FieldType(const std::string& name, const std::string& description,
                     double stealth, const std::vector<std::string>& tags,
                     //const Condition::ConditionBase* location,
                     const std::vector<boost::shared_ptr<const Effect::EffectsGroup> >& effects,
                     const std::string& graphic) :
    m_name(name),
    m_description(description),
    m_stealth(stealth),
    m_tags(tags),
    //m_location(location),
    m_effects(effects),
    m_graphic(graphic)
{
    if (m_stealth != 0)
        m_effects.push_back(IncreaseMeter(METER_STEALTH,    m_stealth));
}

FieldType::~FieldType()
{ /*delete m_location;*/ }

std::string FieldType::Dump() const {
    using boost::lexical_cast;

    std::string retval = DumpIndent() + "FieldType\n";
    ++g_indent;
    retval += DumpIndent() + "name = \"" + m_name + "\"\n";
    retval += DumpIndent() + "description = \"" + m_description + "\"\n";
    retval += DumpIndent() + "location = \n";
    //++g_indent;
    //retval += m_location->Dump();
    //--g_indent;
    if (m_effects.size() == 1) {
        retval += DumpIndent() + "effectsgroups =\n";
        ++g_indent;
        retval += m_effects[0]->Dump();
        --g_indent;
    } else {
        retval += DumpIndent() + "effectsgroups = [\n";
        ++g_indent;
        for (unsigned int i = 0; i < m_effects.size(); ++i) {
            retval += m_effects[i]->Dump();
        }
        --g_indent;
        retval += DumpIndent() + "]\n";
    }
    retval += DumpIndent() + "graphic = \"" + m_graphic + "\"\n";
    --g_indent;
    return retval;
}


/////////////////////////////////////////////////
// FieldTypeManager                         //
/////////////////////////////////////////////////
// static(s)
FieldTypeManager* FieldTypeManager::s_instance = 0;

FieldTypeManager::FieldTypeManager() {
    if (s_instance)
        throw std::runtime_error("Attempted to create more than one FieldTypeManager.");

    s_instance = this;

    parse::fields(GetResourceDir() / "fields.txt", m_field_types);

    if (GetOptionsDB().Get<bool>("verbose-logging")) {
        Logger().debugStream() << "Field Types:";
        for (iterator it = begin(); it != end(); ++it) {
            Logger().debugStream() << " ... " << it->first;
        }
    }
}

FieldTypeManager::~FieldTypeManager() {
    for (std::map<std::string, FieldType*>::iterator it = m_field_types.begin();
         it != m_field_types.end(); ++it)
    { delete it->second; }
}

const FieldType* FieldTypeManager::GetFieldType(const std::string& name) const {
    std::map<std::string, FieldType*>::const_iterator it = m_field_types.find(name);
    return it != m_field_types.end() ? it->second : 0;
}

FieldTypeManager& FieldTypeManager::GetFieldTypeManager() {
    static FieldTypeManager manager;
    return manager;
}

///////////////////////////////////////////////////////////
// Free Functions                                        //
///////////////////////////////////////////////////////////
FieldTypeManager& FieldTypeManager()
{ return FieldTypeManager::GetFieldTypeManager(); }

const FieldType* GetFieldType(const std::string& name)
{ return FieldTypeManager().GetFieldType(name); }
