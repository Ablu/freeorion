// -*- C++ -*-
#ifndef _Parser_h_
#define _Parser_h_

#define PHOENIX_LIMIT 12
#define BOOST_SPIRIT_CLOSURE_LIMIT PHOENIX_LIMIT

#include "Enums.h"
#include "ShipDesign.h"     // because HullType::Slot are stored by value in vector passed to HullType constructor

#include <boost/spirit.hpp>
#include <boost/spirit/attribute.hpp>
#include <boost/spirit/phoenix.hpp>
#include <boost/tuple/tuple.hpp>

#include <stdexcept>
#include <set>
#include <string>

#include <GG/Clr.h>

////////////////////////////////////////////////////////////
// Forward Declarations                                   //
////////////////////////////////////////////////////////////
namespace Condition {
    struct ConditionBase;
}
namespace Effect {
    class EffectsGroup;
    class EffectBase;
}
class Special;
class BuildingType;
class Tech;
class ShipDesign;
struct TechCategory;
struct ItemSpec;
struct FleetPlan;

////////////////////////////////////////////////////////////
// Scanner                                                //
////////////////////////////////////////////////////////////
struct Skip : boost::spirit::grammar<Skip>
{
    template <class ScannerT>
    struct definition
    {
        definition(const Skip&)
        {
            using namespace boost::spirit;
            skip = space_p | comment_p("//") | comment_p("/*", "*/");
        }
        boost::spirit::rule<ScannerT> skip;
        const boost::spirit::rule<ScannerT>& start() const {return skip;}
    };
};
extern const Skip skip_p;

typedef boost::spirit::scanner<const char*, boost::spirit::scanner_policies<boost::spirit::skip_parser_iteration_policy<Skip> > > ScannerBase;
typedef boost::spirit::as_lower_scanner<ScannerBase>::type Scanner;

struct NameClosure : boost::spirit::closure<NameClosure, std::string>
{
    member1 this_;
};

struct ColourClosure : boost::spirit::closure<ColourClosure, GG::Clr, unsigned int, unsigned int, unsigned int, unsigned int>
{
    member1 this_;
    member2 r;
    member3 g;
    member4 b;
    member5 a;
};

////////////////////////////////////////////////////////////
// Condition Parser                                       //
////////////////////////////////////////////////////////////
struct ConditionClosure : boost::spirit::closure<ConditionClosure, Condition::ConditionBase*>
{
    member1 this_;
};

extern boost::spirit::rule<Scanner, ConditionClosure::context_t> condition_p;


////////////////////////////////////////////////////////////
// Effect Parser                                          //
////////////////////////////////////////////////////////////
struct EffectClosure : boost::spirit::closure<EffectClosure, Effect::EffectBase*>
{
    member1 this_;
};

extern boost::spirit::rule<Scanner, EffectClosure::context_t> effect_p;


////////////////////////////////////////////////////////////
// Top Level Parsers                                      //
////////////////////////////////////////////////////////////
struct BuildingTypeClosure : boost::spirit::closure<BuildingTypeClosure, BuildingType*, std::string,
                                                    std::string, double, int, double, Condition::ConditionBase*,
                                                    std::vector<boost::shared_ptr<const Effect::EffectsGroup> >,
                                                    std::string>
{
    member1 this_;
    member2 name;
    member3 description;
    member4 build_cost;
    member5 build_time;
    member6 maintenance_cost;
    member7 location;
    member8 effects_groups;
    member9 graphic;
};

struct SpecialClosure : boost::spirit::closure<SpecialClosure, Special*, std::string, std::string,
                                               std::vector<boost::shared_ptr<const Effect::EffectsGroup> >,
                                               std::string>
{
    member1 this_;
    member2 name;
    member3 description;
    member4 effects_groups;
    member5 graphic;
};

struct CategoryClosure : boost::spirit::closure<CategoryClosure, TechCategory*, std::string, std::string, GG::Clr>
{
    member1 this_;
    member2 name;
    member3 graphic;
    member4 colour;
};

struct TechClosure : boost::spirit::closure<TechClosure, Tech*, std::string, std::string, std::string,
                                            std::string, TechType, double, int,
                                            std::vector<boost::shared_ptr<const Effect::EffectsGroup> >,
                                            std::set<std::string>, std::vector<ItemSpec>, std::string>
{
    member1 this_;
    member2 name;
    member3 description;
    member4 short_description;
    member5 category;
    member6 tech_type;
    member7 research_cost;
    member8 research_turns;
    member9 effects_groups;
    member10 prerequisites;
    member11 unlocked_items;
    member12 graphic;
};

struct PartStatsClosure : boost::spirit::closure<PartStatsClosure, PartTypeStats, double, double,
                                                 double, double, double, double, CombatFighterType,
                                                 double, double, double, int>
{
    member1 this_;
    member2 damage;
    member3 rate;
    member4 range;
    member5 speed;
    member6 stealth;
    member7 health;
    member8 fighter_type;
    member9 anti_fighter_damage;
    member10 anti_ship_damage;
    member11 detection;
    member12 capacity;
};

struct PartClosure : boost::spirit::closure<PartClosure, PartType*, std::string, std::string, ShipPartClass,
                                            PartTypeStats, double, int, std::vector<ShipSlotType>,
                                            Condition::ConditionBase*, std::string>
{
    member1 this_;
    member2 name;
    member3 description;
    member4 part_class;
    member5 stats;
    member6 cost;
    member7 build_time;
    member8 mountable_slot_types;
    member9 location;
    member10 graphic;
};

struct HullClosure : boost::spirit::closure<HullClosure, HullType*, std::string, std::string, double, double,
                                            double, double, double, int, std::vector<HullType::Slot>,
                                            Condition::ConditionBase*, std::string>
{
    member1 this_;
    member2 name;
    member3 description;
    member4 speed;
    member5 starlane_speed;
    member6 fuel;
    member7 health;
    member8 cost;
    member9 build_time;
    member10 slots;
    member11 location;
    member12 graphic;
};

struct ShipDesignClosure : boost::spirit::closure<ShipDesignClosure, ShipDesign*, std::string, std::string,
                                                  std::string, std::vector<std::string>, std::string, std::string>
{
    member1 this_;
    member2 name;
    member3 description;
    member4 hull;
    member5 parts;
    member6 graphic;
    member7 model;
};

struct FleetPlanClosure : boost::spirit::closure<FleetPlanClosure, FleetPlan*, std::string,
                                                 std::vector<std::string> >
{
    member1 this_;
    member2 name;
    member3 ship_designs;
};

extern boost::spirit::rule<Scanner, BuildingTypeClosure::context_t> building_type_p;
extern boost::spirit::rule<Scanner, SpecialClosure::context_t>      special_p;
extern boost::spirit::rule<Scanner, CategoryClosure::context_t>     category_p;
extern boost::spirit::rule<Scanner, TechClosure::context_t>         tech_p;
extern boost::spirit::rule<Scanner, PartStatsClosure::context_t>    part_stats_p;
extern boost::spirit::rule<Scanner, PartClosure::context_t>         part_p;
extern boost::spirit::rule<Scanner, HullClosure::context_t>         hull_p;
extern boost::spirit::rule<Scanner, ShipDesignClosure::context_t>   ship_design_p;
extern boost::spirit::rule<Scanner, FleetPlanClosure::context_t>    fleet_plan_p;

#endif // _Parser_h_
