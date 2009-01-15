// -*- C++ -*-
#ifndef _InfoPanels_h_
#define _InfoPanels_h_

#include "../universe/Enums.h"

#include <GG/Button.h>
#include <GG/DropDownList.h>
#include <GG/BrowseInfoWnd.h>

class PopulationPanel;
class ResourcePanel;
class BuildingsPanel;
class BuildingIndicator;
class SpecialsPanel;
class MultiTurnProgressBar;
class MultiMeterStatusBar;
class MultiIconValueIndicator;
class Meter;
class ShipDesign;
class System;
class Planet;
class ResourceCenter;
class PopCenter;
class UniverseObject;
class BuildingType;
class StatisticIcon;
class CUIDropDownList;
namespace GG {
    class StaticGraphic;
}

/** Shows population and health with meter bars */
class PopulationPanel : public GG::Wnd {
public:
    /** \name Structors */ //@{
    PopulationPanel(GG::X w, const UniverseObject &obj);  ///< basic ctor
    ~PopulationPanel();
    //@}

    /** \name Accessors */ //@{
    int                 PopCenterID() const {return m_popcenter_id;}
    //@}

    /** \name Mutators */ //@{
    void                ExpandCollapse(bool expanded);  ///< expands or collapses panel to show details or just summary info

    virtual void        Render();

    virtual void        MouseWheel(const GG::Pt& pt, int move, GG::Flags<GG::ModKey> mod_keys);  ///< respond to movement of the mouse wheel (move > 0 indicates the wheel is rolled up, < 0 indicates down)

    void                Update();                       ///< updates indicators with values of associated object.  Does not do layout and resizing.
    void                Refresh();                      ///< updates, redoes layout, resizes indicator

    mutable boost::signal<void ()> ExpandCollapseSignal;
    //@}

private:
    void                ExpandCollapseButtonPressed();  ///< toggles panel expanded or collapsed

    void                DoExpandCollapseLayout();       ///< resizes panel and positions widgets according to present collapsed / expanded status

    PopCenter*          GetPopCenter();                 ///< returns the planet with ID m_planet_id
    const PopCenter*    GetPopCenter() const;

    int                         m_popcenter_id;         ///< object id for the UniverseObject that is also a PopCenter which is being displayed in this panel

    StatisticIcon*              m_pop_stat;             ///< icon and number of population
    StatisticIcon*              m_health_stat;          ///< icon and number of health

    MultiIconValueIndicator*    m_multi_icon_value_indicator;   ///< textually / numerically indicates population and health
    MultiMeterStatusBar*        m_multi_meter_status_bar;       ///< graphically indicates meter values

    GG::Button*                 m_expand_button;        ///< at top right of panel, toggles the panel open/closed to show details or minimal summary

    static std::map<int, bool>  s_expanded_map;         ///< map indexed by popcenter ID indicating whether the PopulationPanel for each object is expanded (true) or collapsed (false)

    static const int            EDGE_PAD;               ///< distance between edges of panel and placement of child controls
};

/** Shows resource meters with meter-bars */
class ResourcePanel : public GG::Wnd {
public:
    /** \name Structors */ //@{
    ResourcePanel(GG::X w, const UniverseObject &obj);
    ~ResourcePanel();
    //@}

    /** \name Accessors */ //@{
    int                     ResourceCenterID() const {return m_rescenter_id;}
    //@}

    /** \name Mutators */ //@{
    void                    ExpandCollapse(bool expanded); ///< expands or collapses panel to show details or just summary info

    virtual void            Render();
    virtual void            MouseWheel(const GG::Pt& pt, int move, GG::Flags<GG::ModKey> mod_keys);  ///< respond to movement of the mouse wheel (move > 0 indicates the wheel is rolled up, < 0 indicates down)

    void                    Update();  ///< updates indicators with values of associated object.  Does not do layout and resizing.
    void                    Refresh(); ///< updates, redoes layout, resizes indicator

    mutable boost::signal<void ()> ExpandCollapseSignal;
    mutable boost::signal<void (FocusType)> PrimaryFocusChangedSignal;
    mutable boost::signal<void (FocusType)> SecondaryFocusChangedSignal;
    //@}

private:
    void                    ExpandCollapseButtonPressed();      ///< toggles panel expanded or collapsed
    void                    DoExpandCollapseLayout();           ///< resizes panel and positions widgets according to present collapsed / expanded status

    void                    PrimaryFocusDropListSelectionChanged(GG::DropDownList::iterator selected);     ///< called when droplist selection changes, emits PrimaryFocusChangedSignal
    void                    SecondaryFocusDropListSelectionChanged(GG::DropDownList::iterator selected);   ///< called when droplist selection changes, emits SecondaryFocusChangedSignal

    ResourceCenter*         GetResourceCenter();        ///< returns the planet with ID m_planet_id
    const ResourceCenter*   GetResourceCenter() const;

    int                         m_rescenter_id;         ///< object id for the UniverseObject that is also a PopCenter which is being displayed in this panel

    StatisticIcon*              m_farming_stat;         ///< icon and number of food production
    StatisticIcon*              m_mining_stat;          ///< icon and number of minerals production
    StatisticIcon*              m_industry_stat;        ///< icon and number of industry production
    StatisticIcon*              m_research_stat;        ///< icon and number of research production
    StatisticIcon*              m_trade_stat;           ///< icon and number of trade production

    MultiIconValueIndicator*    m_multi_icon_value_indicator;   ///< textually / numerically indicates resource production and construction meter
    MultiMeterStatusBar*        m_multi_meter_status_bar;       ///< graphically indicates meter values

    CUIDropDownList*            m_primary_focus_drop;   ///< displays and allows selection of primary focus
    CUIDropDownList*            m_secondary_focus_drop; ///< displays and allows selection of secondary focus
    std::map<CUIDropDownList*, boost::signals::connection>  m_drop_changed_connections; ///< signals connecting selection changed signals from drop lists to responses.  blocked when programmatically changing focus selection, to avoid recursive signal emission

    GG::Button*                 m_expand_button;    ///< at top right of panel, toggles the panel open/closed to show details or minimal summary

    static std::map<int, bool>  s_expanded_map;     ///< map indexed by popcenter ID indicating whether the PopulationPanel for each object is expanded (true) or collapsed (false)

    static const int            EDGE_PAD;           ///< distance between edges of panel and placement of child controls
};

/** Shows military-related meters including stealth, detection, shields, defense; with meter bars */
class MilitaryPanel : public GG::Wnd {
public:
    /** \name Structors */ //@{
    MilitaryPanel(GG::X w, const Planet &plt);
    ~MilitaryPanel();
    //@}

    /** \name Accessors */ //@{
    int                     PlanetID() const {return m_planet_id;}
    //@}

    /** \name Mutators */ //@{
    void                    ExpandCollapse(bool expanded);  ///< expands or collapses panel to show details or just summary info

    virtual void            Render();
    virtual void            MouseWheel(const GG::Pt& pt, int move, GG::Flags<GG::ModKey> mod_keys);  ///< respond to movement of the mouse wheel (move > 0 indicates the wheel is rolled up, < 0 indicates down)

    void                    Update();                       ///< updates indicators with values of associated object.  Does not do layout and resizing.
    void                    Refresh();                      ///< updates, redoes layout, resizes indicator

    mutable boost::signal<void ()> ExpandCollapseSignal;
    //@}

private:
    void                    ExpandCollapseButtonPressed();  ///< toggles panel expanded or collapsed
    void                    DoExpandCollapseLayout();       ///< resizes panel and positions widgets according to present collapsed / expanded status

    Planet*                 GetPlanet();                    ///< returns the planet with ID m_planet_id
    const ResourceCenter*   GetPlanet() const;

    int                         m_planet_id;            ///< object id for the UniverseObject that this panel display info about

    StatisticIcon*              m_fleet_supply_stat;    ///< icon and number of food production
    StatisticIcon*              m_shield_stat;          ///< icon and number of minerals production
    StatisticIcon*              m_defense_stat;         ///< icon and number of industry production
    StatisticIcon*              m_detection_stat;       ///< icon and number of research production
    StatisticIcon*              m_stealth_stat;         ///< icon and number of trade production

    MultiIconValueIndicator*    m_multi_icon_value_indicator;   ///< textually / numerically indicates resource production and construction meter
    MultiMeterStatusBar*        m_multi_meter_status_bar;       ///< graphically indicates meter values

    GG::Button*                 m_expand_button;    ///< at top right of panel, toggles the panel open/closed to show details or minimal summary

    static std::map<int, bool>  s_expanded_map;     ///< map indexed by popcenter ID indicating whether the PopulationPanel for each object is expanded (true) or collapsed (false)

    static const int            EDGE_PAD;           ///< distance between edges of panel and placement of child controls
};

class BuildingsPanel : public GG::Wnd {
public:
    /** \name Structors */ //@{
    BuildingsPanel(GG::X w, int columns, const Planet &plt);  ///< basic ctor
    ~BuildingsPanel();
    //@}

    /** \name Accessors */ //@{
    int             PlanetID() const {return m_planet_id;}
    //@}

    /** \name Mutators */ //@{
    void            ExpandCollapse(bool expanded);          ///< expands or collapses panel to show details or just summary info

    virtual void    Render();
    virtual void    MouseWheel(const GG::Pt& pt, int move, GG::Flags<GG::ModKey> mod_keys);  ///< respond to movement of the mouse wheel (move > 0 indicates the wheel is rolled up, < 0 indicates down)

    void            Refresh();                              ///< recreates indicators, redoes layout, resizes

    mutable boost::signal<void ()> ExpandCollapseSignal;
    //@}

private:
    void            ExpandCollapseButtonPressed();          ///< toggles panel expanded or collapsed
    void            DoExpandCollapseLayout();               ///< resizes panel and positions indicators, differently depending on collapsed / expanded status

    void            Update();                               ///< recreates building indicators for building on or being built at this planet

    Planet*         GetPlanet();                            ///< returns the planet with ID m_planet_id
    const Planet*   GetPlanet() const;

    int                             m_planet_id;            ///< object id for the Planet whose buildings this panel displays
    int                             m_columns;              ///< number of columns in which to display building indicators

    std::vector<BuildingIndicator*> m_building_indicators;

    GG::Button*                     m_expand_button;        ///< at top right of panel, toggles the panel open/closed to show details or minimal summary

    static std::map<int, bool>      s_expanded_map;         ///< map indexed by planet ID indicating whether the BuildingsPanel for each object is expanded (true) or collapsed (false)
};

/** Represents and allows some user interaction with a building */
class BuildingIndicator : public GG::Wnd {
public:
    BuildingIndicator(GG::X w, const BuildingType &type); ///< constructor for use when building is completed, shown without progress bar
    BuildingIndicator(GG::X w, const BuildingType &type, int turns_left, int turns_completed,
                      double partial_turn);             ///< constructor for use when building is partially complete, to show progress bar

    virtual void    Render();

    virtual void    SizeMove(const GG::Pt& ul, const GG::Pt& lr);
    virtual void    MouseWheel(const GG::Pt& pt, int move, GG::Flags<GG::ModKey> mod_keys);  ///< respond to movement of the mouse wheel (move > 0 indicates the wheel is rolled up, < 0 indicates down)


private:
    const BuildingType&     m_type;

    GG::StaticGraphic*      m_graphic;
    MultiTurnProgressBar*   m_progress_bar;
};

/** Displays a set of specials attached to an UniverseObject */
class SpecialsPanel : public GG::Wnd {
public:
    /** \name Structors */ //@{
    SpecialsPanel(GG::X w, const UniverseObject &obj);   ///< basic ctor
    //@}

    /** \name Accessors */ //@{
    bool                    InWindow(const GG::Pt& pt) const;
    int                     ObjectID() const {return m_object_id;}
    //@}

    /** \name Mutators */ //@{
    virtual void            Render();
    virtual void            MouseWheel(const GG::Pt& pt, int move, GG::Flags<GG::ModKey> mod_keys);  ///< respond to movement of the mouse wheel (move > 0 indicates the wheel is rolled up, < 0 indicates down)

    void                    Update();          ///< regenerates indicators according to buildings on planets and on queue on planet and redoes layout
    //@}

private:
    UniverseObject*         GetObject();        ///< returns the object with ID m_object_id
    const UniverseObject*   GetObject() const;

    int                             m_object_id;        ///< id for the Object whose specials this panel displays

    std::vector<GG::StaticGraphic*> m_icons;

    static const int                EDGE_PAD;
};

/** Represents a ShipDesign */
class ShipDesignPanel : public GG::Control {
public:
    /** \name Structors */ //@{
    ShipDesignPanel(GG::X w, GG::Y h, int design_id);   ///< basic ctor
    //@}

    /** \name Accessors */ //@{
    int                     DesignID() const {return m_design_id;}
    //@}

    /** \name Mutators */ //@{
    virtual void            SizeMove(const GG::Pt& ul, const GG::Pt& lr);
    virtual void            Render();
    virtual void            MouseWheel(const GG::Pt& pt, int move, GG::Flags<GG::ModKey> mod_keys);  ///< respond to movement of the mouse wheel (move > 0 indicates the wheel is rolled up, < 0 indicates down)

    void                    Update();           ///< regenerates indicators according to buildings on planets and on queue on planet and redoes layout
    //@}

private:
    const ShipDesign*       GetDesign();        ///< returns the ShipDesign with ID m_design_id
    int                     m_design_id;        ///< id for the Object whose specials this panel displays

protected:
    GG::StaticGraphic*      m_graphic;
    GG::TextControl*        m_name;

    static const int EDGE_PAD;
};

/** Display icon and number for various meter-related quantities associated with objects.  Typical use
    would be to display the resource production values for a planet (not the meter values) and the
    construction (a meter value), or the population (not a meter value) and health (a meter value).
    Given a set of MeterType, the indicator will present the appropriate values for each. */
class MultiIconValueIndicator : public GG::Wnd {
public:
    MultiIconValueIndicator(GG::X w, const UniverseObject& obj, const std::vector<MeterType>& meter_types);
    MultiIconValueIndicator(GG::X w, const std::vector<const UniverseObject*>& obj_vec, const std::vector<MeterType>& meter_types);
    MultiIconValueIndicator(GG::X w); ///< initializes with no icons shown

    bool            Empty();

    virtual void    Render();
    virtual void    MouseWheel(const GG::Pt& pt, int move, GG::Flags<GG::ModKey> mod_keys);

    void            Update();

    void            SetToolTip(MeterType meter_type, const boost::shared_ptr<GG::BrowseInfoWnd>& browse_wnd);

private:
    std::vector<StatisticIcon*>         m_icons;

    std::vector<MeterType>              m_meter_types;
    std::vector<const UniverseObject*>  m_obj_vec;

    static const int                    EDGE_PAD;
    static const int                    ICON_SPACING;
    static const GG::X                  ICON_WIDTH;
    static const GG::Y                  ICON_HEIGHT;
};

/** Graphically represets the current max and projected changes to values of multiple Meters, using a
    horizontal indicator for each meter. */
class MultiMeterStatusBar : public GG::Wnd {
public:
    MultiMeterStatusBar(GG::X w, const UniverseObject& obj, const std::vector<MeterType>& meter_types);

    virtual void Render();
    virtual void MouseWheel(const GG::Pt& pt, int move, GG::Flags<GG::ModKey> mod_keys);

    void Update();

private:
    boost::shared_ptr<GG::Texture> m_bar_shading_texture;

    std::vector<MeterType> m_meter_types;
    std::vector<double> m_initial_maxes;
    std::vector<double> m_initial_currents;
    std::vector<double> m_projected_maxes;
    std::vector<double> m_projected_currents;

    const UniverseObject& m_obj;

    static const int EDGE_PAD;
    static const int BAR_PAD;

    static const GG::Y BAR_HEIGHT;

    std::vector<GG::Clr> m_bar_colours;
};

/** A popup tooltop for display when mousing over in-game icons.  Has an icon and title and some detail text.*/
class IconTextBrowseWnd : public GG::BrowseInfoWnd {
public:
    IconTextBrowseWnd(const boost::shared_ptr<GG::Texture> texture, const std::string& title_text,
                      const std::string& main_text);
    virtual bool WndHasBrowseInfo(const Wnd* wnd, std::size_t mode) const;
    virtual void Render();

private:
    GG::StaticGraphic* m_icon;
    GG::TextControl* m_title_text;
    GG::TextControl* m_main_text;

    static const GG::X TEXT_WIDTH;
    static const GG::X TEXT_PAD;
    static const GG::X ICON_WIDTH;
    static const GG::Y ICON_HEIGHT;
    const GG::Y ROW_HEIGHT;
};

/** Gives information about inporting and exporting of resources to and from this system when mousing
  * over the system resource production summary. */
class SystemResourceSummaryBrowseWnd : public GG::BrowseInfoWnd {
public:
    SystemResourceSummaryBrowseWnd(ResourceType resource_type, const System* system);

    virtual bool WndHasBrowseInfo(const Wnd* wnd, std::size_t mode) const;

    virtual void Render();

private:
    void Initialize();

    ResourceType        m_resource_type;
    const System*       m_system;

    static const GG::X  LABEL_WIDTH;
    static const GG::X  VALUE_WIDTH;
    static const int    EDGE_PAD;

    bool initialized;
};

#endif
