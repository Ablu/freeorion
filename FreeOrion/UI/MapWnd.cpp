#ifdef FREEORION_WIN32
#include <GL/glew.h>
#endif

#include "MapWnd.h"

#include "ChatWnd.h"
#include "PlayerListWnd.h"
#include "ClientUI.h"
#include "CUIControls.h"
#include "FleetButton.h"
#include "FleetWnd.h"
#include "InfoPanels.h"
#include "InGameMenu.h"
#include "DesignWnd.h"
#include "ProductionWnd.h"
#include "ResearchWnd.h"
#include "EncyclopediaDetailPanel.h"
#include "ObjectListWnd.h"
#include "ModeratorActionsWnd.h"
#include "SidePanel.h"
#include "SitRepPanel.h"
#include "SystemIcon.h"
#include "FieldIcon.h"
#include "TurnProgressWnd.h"
#include "ShaderProgram.h"
#include "../util/Directories.h"
#include "../util/MultiplayerCommon.h"
#include "../util/OptionsDB.h"
#include "../util/Random.h"
#include "../util/ModeratorAction.h"
#include "../universe/Fleet.h"
#include "../universe/Planet.h"
#include "../universe/Predicates.h"
#include "../universe/System.h"
#include "../universe/Field.h"
#include "../universe/Universe.h"
#include "../universe/UniverseObject.h"
#include "../Empire/Empire.h"
#include "../network/Message.h"
#include "../client/human/HumanClientApp.h"

#include <boost/timer.hpp>

#include <GG/DrawUtil.h>
#include <GG/MultiEdit.h>
#include <GG/WndEvent.h>
#include <GG/Layout.h>

#include <vector>
#include <deque>
#include <valarray>

namespace {
    const double    ZOOM_STEP_SIZE = std::pow(2.0, 1.0/3.0);
    const double    ZOOM_IN_MAX_STEPS = 9.0;
    const double    ZOOM_IN_MIN_STEPS = -7.0;   // negative zoom steps indicates zooming out
    const int       ZOOM_TOTAL_STEPS = static_cast<const int>(ZOOM_IN_MAX_STEPS + 1.0 + ZOOM_IN_MIN_STEPS);
    const double    ZOOM_MAX = std::pow(ZOOM_STEP_SIZE, ZOOM_IN_MAX_STEPS);
    const double    ZOOM_MIN = std::pow(ZOOM_STEP_SIZE, ZOOM_IN_MIN_STEPS);
    const GG::X     SITREP_PANEL_WIDTH(400);
    const GG::Y     SITREP_PANEL_HEIGHT(200);
    const GG::Y     ZOOM_SLIDER_HEIGHT(200);
    const GG::Y     SCALE_LINE_HEIGHT(20);
    const GG::X     SCALE_LINE_MAX_WIDTH(240);
    const int       MIN_SYSTEM_NAME_SIZE = 10;
    const int       LAYOUT_MARGIN = 5;
    const GG::Y     TOOLBAR_HEIGHT(30);

    struct ClrLess {
        bool operator()(const GG::Clr& rhs, const GG::Clr& lhs) const {
            if (rhs.r != lhs.r)
                return rhs.r < lhs.r;
            if (rhs.g != lhs.g)
                return rhs.g < lhs.g;
            if (rhs.b != lhs.b)
                return rhs.b < lhs.b;
            return rhs.a < lhs.a;
        }
    };

    double  ZoomScaleFactor(double steps_in) {
        if (steps_in > ZOOM_IN_MAX_STEPS) {
            Logger().errorStream() << "ZoomScaleFactor passed steps in (" << steps_in << ") higher than max (" << ZOOM_IN_MAX_STEPS << "), so using max";
            steps_in = ZOOM_IN_MAX_STEPS;
        } else if (steps_in < ZOOM_IN_MIN_STEPS) {
            Logger().errorStream() << "ZoomScaleFactor passed steps in (" << steps_in << ") lower than minimum (" << ZOOM_IN_MIN_STEPS << "), so using min";
            steps_in = ZOOM_IN_MIN_STEPS;
        }
        return std::pow(ZOOM_STEP_SIZE, steps_in);
    }

    struct BoolToVoidAdapter {
        BoolToVoidAdapter(const boost::function<bool ()>& fn) : m_fn(fn) {}
        void operator()() { m_fn(); }
        boost::function<bool ()> m_fn;
    };

    void AddOptions(OptionsDB& db) {
        db.Add("UI.galaxy-gas-background",          "OPTIONS_DB_GALAXY_MAP_GAS",                    true,       Validator<bool>());
        db.Add("UI.galaxy-starfields",              "OPTIONS_DB_GALAXY_MAP_STARFIELDS",             true,       Validator<bool>());
        db.Add("UI.show-galaxy-map-scale",          "OPTIONS_DB_GALAXY_MAP_SCALE_LINE",             true,       Validator<bool>());
        db.Add("UI.show-galaxy-map-zoom-slider",    "OPTIONS_DB_GALAXY_MAP_ZOOM_SLIDER",            false,      Validator<bool>());
        db.Add("UI.optimized-system-rendering",     "OPTIONS_DB_OPTIMIZED_SYSTEM_RENDERING",        true,       Validator<bool>());
        db.Add("UI.starlane-thickness",             "OPTIONS_DB_STARLANE_THICKNESS",                2.0,        RangedStepValidator<double>(0.25, 0.25, 10.0));
        db.Add("UI.starlane-core-multiplier",       "OPTIONS_DB_STARLANE_CORE",                     6.0,        RangedStepValidator<double>(1.0, 1.0, 10.0));
        db.Add("UI.resource-starlane-colouring",    "OPTIONS_DB_RESOURCE_STARLANE_COLOURING",       true,       Validator<bool>());
        db.Add("UI.fleet-supply-lines",             "OPTIONS_DB_FLEET_SUPPLY_LINES",                true,       Validator<bool>());
        db.Add("UI.fleet-supply-line-width",        "OPTIONS_DB_FLEET_SUPPLY_LINE_WIDTH",           3.0,        RangedStepValidator<double>(0.25, 0.25, 10.0));
        db.Add("UI.fleet-supply-line-dot-spacing",  "OPTIONS_DB_FLEET_SUPPLY_LINE_DOT_SPACING",     20,         RangedStepValidator<int>(1, 3, 40));
        db.Add("UI.fleet-supply-line-dot-rate",     "OPTIONS_DB_FLEET_SUPPLY_LINE_DOT_RATE",        0.02,       RangedStepValidator<double>(0.01, 0.01, 0.1));
        db.Add("UI.unowned-starlane-colour",        "OPTIONS_DB_UNOWNED_STARLANE_COLOUR",           StreamableColor(GG::Clr(72,  72,  72,  255)),   Validator<StreamableColor>());

        db.Add("UI.show-detection-range",           "OPTIONS_DB_GALAXY_MAP_DETECTION_RANGE",        false,      Validator<bool>());

        db.Add("UI.system-fog-of-war",              "OPTIONS_DB_UI_SYSTEM_FOG",                     true,       Validator<bool>());
        db.Add("UI.system-fog-of-war-spacing",      "OPTIONS_DB_UI_SYSTEM_FOG_SPACING",             4.0,        RangedStepValidator<double>(0.25, 1.5, 8.0));

        db.Add("UI.system-icon-size",               "OPTIONS_DB_UI_SYSTEM_ICON_SIZE",               14,         RangedValidator<int>(8, 50));

        db.Add("UI.system-circles",                 "OPTIONS_DB_UI_SYSTEM_CIRCLES",                 true,       Validator<bool>());
        db.Add("UI.system-circle-size",             "OPTIONS_DB_UI_SYSTEM_CIRCLE_SIZE",             1.0,        RangedStepValidator<double>(0.125, 1.0, 2.5));

        db.Add("UI.system-tiny-icon-size-threshold","OPTIONS_DB_UI_SYSTEM_TINY_ICON_SIZE_THRESHOLD",10,         RangedValidator<int>(1, 16));
        db.Add("UI.system-selection-indicator-size","OPTIONS_DB_UI_SYSTEM_SELECTION_INDICATOR_SIZE",1.625,      RangedStepValidator<double>(0.125, 0.5, 5));
        db.Add("UI.system-selection-indicator-fps", "OPTIONS_DB_UI_SYSTEM_SELECTION_INDICATOR_FPS", 12,         RangedValidator<int>(1, 60));

        db.Add("UI.system-name-unowned-color",      "OPTIONS_DB_UI_SYSTEM_NAME_UNOWNED_COLOR",      StreamableColor(GG::Clr(160, 160, 160, 255)),   Validator<StreamableColor>());

        db.Add("UI.tiny-fleet-button-minimum-zoom", "OPTIONS_DB_UI_TINY_FLEET_BUTTON_MIN_ZOOM",     0.75,       RangedStepValidator<double>(0.125, 0.125, 4.0));
        db.Add("UI.small-fleet-button-minimum-zoom","OPTIONS_DB_UI_SMALL_FLEET_BUTTON_MIN_ZOOM",    1.50,       RangedStepValidator<double>(0.125, 0.125, 4.0));
        db.Add("UI.medium-fleet-button-minimum-zoom","OPTIONS_DB_UI_MEDIUM_FLEET_BUTTON_MIN_ZOOM",  4.00,       RangedStepValidator<double>(0.125, 0.125, 4.0));

        db.Add("UI.map-right-click-popup-menu",     "OPTIONS_DB_UI_GALAXY_MAP_POPUP",               false,      Validator<bool>());
    }
    bool temp_bool = RegisterOptions(&AddOptions);

    // returns an int-int pair that doesn't depend on the order of parameters
    std::pair<int, int> UnorderedIntPair(int one, int two) {
        return std::make_pair(std::min(one, two), std::max(one, two));
    }

    /* Loads background starfield textures int \a background_textures  */
    void InitBackgrounds(std::vector<boost::shared_ptr<GG::Texture> >& background_textures, std::vector<double>& scroll_rates) {
        if (!background_textures.empty())
            return;

        std::vector<boost::shared_ptr<GG::Texture> > starfield_textures = ClientUI::GetClientUI()->GetPrefixedTextures(ClientUI::ArtDir(), "starfield", false);
        double scroll_rate = 1.0;
        for (std::vector<boost::shared_ptr<GG::Texture> >::const_iterator it = starfield_textures.begin(); it != starfield_textures.end(); ++it) {
            scroll_rate *= 0.5;
            background_textures.push_back(*it);
            scroll_rates.push_back(scroll_rate);
            glBindTexture(GL_TEXTURE_2D, (*it)->OpenGLId());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
    }

    /* Returns fractional distance along line segment between two points that a
     * third point between them is.assumes the "mid" point is between the
     * "start" and "end" points, in which case the returned fraction is between
     * 0.0 and 1.0 */
    double FractionalDistanceBetweenPoints(double startX, double startY, double midX, double midY, double endX, double endY) {
        // get magnitudes of vectors
        double full_deltaX = endX - startX, full_deltaY = endY - startY;
        double mid_deltaX = midX - startX, mid_deltaY = midY - startY;
        double full_length = std::sqrt(full_deltaX*full_deltaX + full_deltaY*full_deltaY);
        if (full_length == 0.0) // safety check
            full_length = 1.0;
        double mid_length = std::sqrt(mid_deltaX*mid_deltaX + mid_deltaY*mid_deltaY);
        return mid_length / full_length;
    }

    /* Returns point that is dist ditance away from (X1, Y1) in the direction
     * of (X2, Y2) */
    std::pair<double, double> PositionFractionalAtDistanceBetweenPoints(double X1, double Y1, double X2, double Y2, double dist) {
        double newX = X1 + (X2 - X1) * dist;
        double newY = Y1 + (Y2 - Y1) * dist;
        return std::make_pair(newX, newY);
    }

    /* Returns apparent map X and Y position of an object at universe position
     * \a X and \a Y for an object that is located on a starlane between
     * systems with ids \a lane_start_sys_id and \a lane_end_sys_id
     * The apparent position of an object depends on its actual position, the
     * actual positions of the systems at the ends of the lane, and the
     * apparent positions of the ends of the lanes.  The apparent position of
     * objects on the lane is compressed into the space between the apparent
     * ends of the lane, but is proportional to the distance of the actual
     * position along the lane. */
    std::pair<double, double> ScreenPosOnStarane(double X, double Y, int lane_start_sys_id, int lane_end_sys_id, const LaneEndpoints& screen_lane_endpoints) {
        std::pair<int, int> lane = UnorderedIntPair(lane_start_sys_id, lane_end_sys_id);

        // get endpoints of lane in universe.  may be different because on-
        // screen lanes are drawn between system circles, not system centres
        int empire_id = HumanClientApp::GetApp()->EmpireID();
        const UniverseObject* prev = GetEmpireKnownObject(lane.first, empire_id);
        const UniverseObject* next = GetEmpireKnownObject(lane.second, empire_id);
        if (!next || !prev) {
            Logger().errorStream() << "ScreenPosOnStarane couldn't find next system " << lane.first << " or prev system " << lane.second;
            return std::make_pair(UniverseObject::INVALID_POSITION, UniverseObject::INVALID_POSITION);
        }

        // get fractional distance along lane that fleet's universe position is
        double dist_along_lane = FractionalDistanceBetweenPoints(prev->X(), prev->Y(), X, Y, next->X(), next->Y());

        return PositionFractionalAtDistanceBetweenPoints(screen_lane_endpoints.X1, screen_lane_endpoints.Y1,
                                                         screen_lane_endpoints.X2, screen_lane_endpoints.Y2,
                                                         dist_along_lane);
    }

    /* Updated each frame to shift rendered posistion of dots that are drawn to
     * show fleet move lines. */
    double move_line_animation_shift = 0.0;    // in pixels

    GG::X WndLeft(const GG::Wnd* wnd) { return wnd ? wnd->UpperLeft().x : GG::X0; }
    GG::X WndRight(const GG::Wnd* wnd) { return wnd ? wnd->LowerRight().x : GG::X0; }
    GG::Y WndTop(const GG::Wnd* wnd) { return wnd ? wnd->UpperLeft().y : GG::Y0; }
    GG::Y WndBottom(const GG::Wnd* wnd) { return wnd ? wnd->LowerRight().y : GG::Y0; }
    bool InRect(GG::X left, GG::Y top, GG::X right, GG::Y bottom, const GG::Pt& pt)
    { return pt.x >= left && pt.y >= top && pt.x < right && pt.y < bottom; } //pt >= ul && pt < lr;

    GG::X AppWidth() {
        if (HumanClientApp* app = HumanClientApp::GetApp())
            return app->AppWidth();
        return GG::X0;
    }

    GG::Y AppHeight() {
        if (HumanClientApp* app = HumanClientApp::GetApp())
            return app->AppHeight();
        return GG::Y0;
    }

    GG::X SidePanelWidth()
    { return GG::X(GetOptionsDB().Get<int>("UI.sidepanel-width")); }
}


////////////////////////////////////////////////////////////
// MapWnd::MapScaleLine
////////////////////////////////////////////////////////////
/** Displays a notched line with number labels to indicate Universe distance on the map. */
class MapWnd::MapScaleLine : public GG::Control {
public:
    MapScaleLine(GG::X x, GG::Y y, GG::X w, GG::Y h) :
        GG::Control(x, y, w, h, GG::ONTOP),
        m_scale_factor(1.0),
        m_line_length(GG::X1),
        m_label(0),
        m_enabled(false)
    {
        m_label = new ShadowedTextControl(GG::X0, GG::Y0, GG::X1, h, "", ClientUI::GetFont(), ClientUI::TextColor(), GG::FORMAT_CENTER);
        AttachChild(m_label);
        Update(1.0);
        GG::Connect(GetOptionsDB().OptionChangedSignal("UI.show-galaxy-map-scale"), &MapScaleLine::UpdateEnabled, this);
        UpdateEnabled();
    }

    virtual void Render() {
        if (!m_enabled)
            return;

        // use GL to draw line and ticks and labels to indicte a length on the map
        GG::Pt ul = UpperLeft();
        GG::Pt lr = ul + GG::Pt(m_line_length, Height());

        glColor(GG::CLR_WHITE);
        glLineWidth(2.0);

        glDisable(GL_TEXTURE_2D);
        glBegin(GL_LINES);
            // left border
            glVertex(ul.x, ul.y);
            glVertex(ul.x, lr.y);

            // right border
            glVertex(lr.x, ul.y);
            glVertex(lr.x, lr.y);

            // bottom line
            glVertex(ul.x, lr.y);
            glVertex(lr.x, lr.y);
        glEnd();
        glEnable(GL_TEXTURE_2D);

        glLineWidth(1.0);
    }

    void Update(double zoom_factor) {
        zoom_factor = std::min(std::max(zoom_factor, ZOOM_MIN), ZOOM_MAX);  // sanity range limits to prevent divide by zero
        m_scale_factor = zoom_factor;

        // determine length of line to draw and how long that is in universe units
        double AVAILABLE_WIDTH = static_cast<double>(std::max(Value(Width()), 1));

        // length in universe units that could be shown if full AVAILABLE_WIDTH was used
        double max_shown_length = AVAILABLE_WIDTH / m_scale_factor;

        // select an actual shown length in universe units by reducing max_shown_length to a nice round number,
        // where nice round numbers are numbers beginning with 1, 2 or 5

        // get appropriate power of 10
        double pow10 = floor(log10(max_shown_length));
        double shown_length = std::pow(10.0, pow10);

        // see if next higher multiples of 5 or 2 can be used
        if (shown_length * 5.0 <= max_shown_length)
            shown_length *= 5.0;
        else if (shown_length * 2.0 <= max_shown_length)
            shown_length *= 2.0;

        // determine end of drawn scale line
        m_line_length = GG::X(static_cast<int>(shown_length * m_scale_factor));

        // update text
        std::string label_text = boost::io::str(FlexibleFormat(UserString("MAP_SCALE_INDICATOR")) %
                                                boost::lexical_cast<std::string>(shown_length));
        m_label->Resize(GG::Pt(GG::X(m_line_length), Height()));
        m_label->SetText(label_text);
    }
private:
    void UpdateEnabled() {
        m_enabled = GetOptionsDB().Get<bool>("UI.show-galaxy-map-scale");
        if (m_enabled)
            AttachChild(m_label);
        else
            DetachChild(m_label);
    }

    double              m_scale_factor;
    GG::X               m_line_length;
    GG::TextControl*    m_label;
    bool                m_enabled;
};

////////////////////////////////////////////////////////////
// MapWndPopup
////////////////////////////////////////////////////////////
MapWndPopup::MapWndPopup(const std::string& t, GG::X x, GG::Y y, GG::X w, GG::Y h, GG::Flags<GG::WndFlag> flags) :
    CUIWnd(t, x, y, w, h, flags)
{ ClientUI::GetClientUI()->GetMapWnd()->RegisterPopup(this); }

MapWndPopup::~MapWndPopup()
{ ClientUI::GetClientUI()->GetMapWnd()->RemovePopup(this); }

void MapWndPopup::CloseClicked()
{
    CUIWnd::CloseClicked();
    delete this;
}

void MapWndPopup::Close()
{ CloseClicked(); }


//////////////////////////////////
//LaneEndpoints
//////////////////////////////////
LaneEndpoints::LaneEndpoints() :
    X1(static_cast<float>(UniverseObject::INVALID_POSITION)),
    Y1(static_cast<float>(UniverseObject::INVALID_POSITION)),
    X2(static_cast<float>(UniverseObject::INVALID_POSITION)),
    Y2(static_cast<float>(UniverseObject::INVALID_POSITION))
{}


////////////////////////////////////////////////
// MapWnd::MovementLineData::Vertex
////////////////////////////////////////////////
struct MapWnd::MovementLineData::Vertex {
    Vertex(double x_, double y_, int eta_, bool show_eta_) :
        x(x_), y(y_), eta(eta_), show_eta(show_eta_)
    {}
    double  x, y;       // apparent in-universe position of a point on move line.  not actual universe positions, but rather where the move line vertices are drawn
    int     eta;        // turns taken to reach point by object travelling along move line
    bool    show_eta;   // should an ETA indicator / number be shown over this vertex?
};

////////////////////////////////////////////////
// MapWnd::MovementLineData
////////////////////////////////////////////////
MapWnd::MovementLineData::MovementLineData() :
    path(),
    colour(GG::CLR_ZERO)
{}

MapWnd::MovementLineData::MovementLineData(const std::list<MovePathNode>& path_,
                                           const std::map<std::pair<int, int>, LaneEndpoints>& lane_end_points_map,
                                           GG::Clr colour_/*= GG::CLR_WHITE*/) :
    path(path_),
    colour(colour_)
{
    // generate screen position vertices

    if (path.empty() || path.size() == 1)
        return; // nothing to draw.  need at least two nodes at different locations to draw a line

    //Logger().debugStream() << "move_line path: ";
    //for (std::list<MovePathNode>::const_iterator it = path.begin(); it != path.end(); ++it)
    //    Logger().debugStream() << " ... " << it->object_id << " (" << it->x << ", " << it->y << ") eta: " << it->eta << " turn_end: " << it->turn_end;


    // draw lines connecting points of interest along path.  only draw a line if the previous and
    // current positions of the ends of the line are known.
    const   MovePathNode& first_node =  *(path.begin());
    double  prev_node_x =               first_node.x;
    double  prev_node_y =               first_node.y;
    int     prev_sys_id =               first_node.object_id;
    int     prev_eta =                  first_node.eta;
    int     next_sys_id =               INVALID_OBJECT_ID;

    for (std::list<MovePathNode>::const_iterator path_it = path.begin(); path_it != path.end(); ++path_it) {
        // stop rendering if end of path is indicated
        if (path_it->eta == Fleet::ETA_UNKNOWN || path_it->eta == Fleet::ETA_NEVER || path_it->eta == Fleet::ETA_OUT_OF_RANGE)
            break;

        const MovePathNode& node = *path_it;


        // 1) Get systems at ends of lane on which current node is located.

        if (node.object_id == INVALID_OBJECT_ID) {
            // node is in open space.
            // node should have valid prev_sys_id and node.lane_end_id to get info about starlane this node is on
            prev_sys_id = node.lane_start_id;
            next_sys_id = node.lane_end_id;

        } else {
            // node is at a system.
            // node should / may not have a valid lane_end_id, but this system's id is the end of a lane approaching it
            next_sys_id = node.object_id;
            // if this is the first node of the path, prev_sys_id should be set to node.object_id from pre-loop initialization.
            // if this node is later in the path, prev_sys_id should have been set in previous loop iteration
        }


        // skip invalid line segments
        if (prev_sys_id == next_sys_id || next_sys_id == INVALID_OBJECT_ID || prev_sys_id == INVALID_OBJECT_ID)
            continue;


        // 2) Get apparent universe positions of nodes, which depend on endpoints of lane and actual universe position of nodes

        // get unordered pair with which to lookup lane endpoints
        std::pair<int, int> lane_ids = UnorderedIntPair(prev_sys_id, next_sys_id);

        // get lane end points
        std::map<std::pair<int, int>, LaneEndpoints>::const_iterator ends_it = lane_end_points_map.find(lane_ids);
        if (ends_it == lane_end_points_map.end()) {
            Logger().errorStream() << "couldn't get endpoints of lane for move line";
            break;
        }
        const LaneEndpoints& lane_endpoints = ends_it->second;

        // get on-screen positions of nodes shifted to fit on starlane
        std::pair<double, double> start_xy =    ScreenPosOnStarane(prev_node_x, prev_node_y, prev_sys_id, next_sys_id, lane_endpoints);
        std::pair<double, double> end_xy =      ScreenPosOnStarane(node.x,      node.y,      prev_sys_id, next_sys_id, lane_endpoints);


        // 3) Add points for line segment to list of Vertices
        vertices.push_back(Vertex(start_xy.first,   start_xy.second,    prev_eta,   false));
        vertices.push_back(Vertex(end_xy.first,     end_xy.second,      node.eta,   node.turn_end));


        // 4) prep for next iteration
        prev_node_x = node.x;
        prev_node_y = node.y;
        prev_eta = node.eta;
        if (node.object_id != INVALID_OBJECT_ID) {  // only need to update previous and next sys ids if current node is at a system
            prev_sys_id = node.object_id;                       // to be used in next iteration
            next_sys_id = INVALID_OBJECT_ID;    // to be set in next iteration
        }
    }
}


///////////////////////////
// MapWnd
///////////////////////////
MapWnd::MapWnd() :
    GG::Wnd(-AppWidth(), -AppHeight(),
            static_cast<GG::X>(GetUniverse().UniverseWidth() * ZOOM_MAX + AppWidth() * 1.5),
            static_cast<GG::Y>(GetUniverse().UniverseWidth() * ZOOM_MAX + AppHeight() * 1.5),
            GG::INTERACTIVE | GG::DRAGABLE),
    m_backgrounds(),
    m_bg_scroll_rate(),
    m_selected_fleet_ids(),
    m_selected_ship_ids(),
    m_zoom_steps_in(0.0),
    m_side_panel(0),
    m_system_icons(),
    m_sitrep_panel(0),
    m_research_wnd(0),
    m_production_wnd(0),
    m_design_wnd(0),
    m_pedia_panel(0),
    m_object_list_wnd(0),
    m_moderator_wnd(0),
    m_starlane_endpoints(),
    m_stationary_fleet_buttons(),
    m_departing_fleet_buttons(),
    m_moving_fleet_buttons(),
    m_fleet_buttons(),
    m_fleet_state_change_signals(),
    m_system_fleet_insert_remove_signals(),
    m_keyboard_accelerator_signals(),
    m_fleet_lines(),
    m_projected_fleet_lines(),
    m_star_core_quad_vertices(),
    m_star_halo_quad_vertices(),
    m_galaxy_gas_quad_vertices(),
    m_star_texture_coords(),
    m_starlane_vertices(),
    m_starlane_colors(),
    m_RC_starlane_vertices(),
    m_RC_starlane_colors(),
    m_resourceCenters(),
    m_drag_offset(-GG::X1, -GG::Y1),
    m_dragged(false),
    m_turn_update(0),
    m_popups(),
    m_menu_showing(false),
    m_current_owned_system(INVALID_OBJECT_ID),
    m_current_fleet_id(INVALID_OBJECT_ID),
    m_in_production_view_mode(false),
    m_sidepanel_open_before_showing_other(false),
    m_toolbar(0),
    m_trade(0),
    m_population(0),
    m_research(0),
    m_industry(0),
    m_detection(0),
    m_fleet(0),
    m_industry_wasted(0),
    m_research_wasted(0),
    m_btn_moderator(0),
    m_btn_siterep(0),
    m_btn_research(0),
    m_btn_production(0),
    m_btn_design(0),
    m_btn_pedia(0),
    m_btn_objects(0),
    m_btn_menu(0),
    m_FPS(0),
    m_scale_line(0),
    m_zoom_slider(0)
{
    SetName("MapWnd");

    Connect(GetUniverse().UniverseObjectDeleteSignal, &MapWnd::UniverseObjectDeleted, this);

    // toolbar
    m_toolbar = new CUIToolBar(GG::X0, GG::Y0, AppWidth(), TOOLBAR_HEIGHT);
    m_toolbar->SetName("MapWnd Toolbar");
    GG::GUI::GetGUI()->Register(m_toolbar);
    m_toolbar->Hide();

    GG::Layout* layout = new GG::Layout(m_toolbar->ClientUpperLeft().x, m_toolbar->ClientUpperLeft().y,
                                        m_toolbar->ClientWidth(), m_toolbar->ClientHeight(),
                                        1, 17);
    layout->SetName("Toolbar Layout");
    m_toolbar->SetLayout(layout);

    // system-view side panel
    m_side_panel = new SidePanel(AppWidth() - SidePanelWidth(), m_toolbar->LowerRight().y, AppHeight() - m_toolbar->Height());
    GG::GUI::GetGUI()->Register(m_side_panel);

    GG::Connect(SidePanel::SystemSelectedSignal,            &MapWnd::SelectSystem, this);
    GG::Connect(SidePanel::PlanetSelectedSignal,            &MapWnd::SelectPlanet, this);

    // not strictly necessary, as in principle whenever any ResourceCenter
    // changes, all meter estimates and resource pools should / could be
    // updated.  however, this is a convenience to limit the updates to
    // what is actually being shown in the sidepanel right now, which is
    // useful since most ResourceCenter changes will be due to focus
    // changes on the sidepanel, and most differences in meter estimates
    // and resource pools due to this will be in the same system
    GG::Connect(SidePanel::ResourceCenterChangedSignal,     &MapWnd::UpdateSidePanelSystemObjectMetersAndResourcePools, this);


    // situation report window
    m_sitrep_panel = new SitRepPanel(GG::X0, GG::Y0, SITREP_PANEL_WIDTH, SITREP_PANEL_HEIGHT);
    GG::Connect(m_sitrep_panel->ClosingSignal, boost::bind(&MapWnd::ToggleSitRep, this));   // Wnd is manually closed by user
    GG::GUI::GetGUI()->Register(m_sitrep_panel);
    m_sitrep_panel->Hide();


    // encyclpedia panel
    m_pedia_panel = new EncyclopediaDetailPanel(SITREP_PANEL_WIDTH, SITREP_PANEL_HEIGHT);
    GG::Connect(m_pedia_panel->ClosingSignal, boost::bind(&MapWnd::TogglePedia, this));     // Wnd is manually closed by user
    GG::GUI::GetGUI()->Register(m_pedia_panel);
    m_pedia_panel->Hide();


    // objects list
    m_object_list_wnd = new ObjectListWnd(SITREP_PANEL_WIDTH, SITREP_PANEL_HEIGHT);
    GG::Connect(m_object_list_wnd->ClosingSignal,       boost::bind(&MapWnd::ToggleObjects, this));   // Wnd is manually closed by user
    GG::Connect(m_object_list_wnd->ObjectDumpSignal,    &ClientUI::DumpObject,              ClientUI::GetClientUI());
    GG::GUI::GetGUI()->Register(m_object_list_wnd);
    m_object_list_wnd->Hide();

    // moderator actions
    m_moderator_wnd = new ModeratorActionsWnd(SITREP_PANEL_WIDTH, SITREP_PANEL_HEIGHT);
    GG::Connect(m_moderator_wnd->ClosingSignal,             boost::bind(&MapWnd::ToggleModeratorActions,    this));
    GG::Connect(m_moderator_wnd->NoActionSelectedSignal,                &MapWnd::ModeratorNoActionSelected, this);
    GG::Connect(m_moderator_wnd->CreateSystemActionSelectedSignal,      &MapWnd::ModeratorCreateSystemSelected, this);
    GG::Connect(m_moderator_wnd->CreatePlanetActionSelectedSignal,      &MapWnd::ModeratorCreatePlanetSelected, this);
    GG::Connect(m_moderator_wnd->DeleteObjectActionSelectedSignal,      &MapWnd::ModeratorDeleteObjectSelected, this);
    GG::Connect(m_moderator_wnd->SetOwnerActionSelectedSignal,          &MapWnd::ModeratorSetOwnerSelected, this);
    GG::Connect(m_moderator_wnd->CreateStarlaneActionSelectedSignal,    &MapWnd::ModeratorCreateStarlaneSelected, this);
    GG::GUI::GetGUI()->Register(m_moderator_wnd);
    m_moderator_wnd->Hide();


    // research window
    m_research_wnd = new ResearchWnd(AppWidth(), AppHeight() - m_toolbar->Height());
    m_research_wnd->MoveTo(GG::Pt(GG::X0, m_toolbar->Height()));
    GG::GUI::GetGUI()->Register(m_research_wnd);
    m_research_wnd->Hide();


    // production window
    m_production_wnd = new ProductionWnd(AppWidth(), AppHeight() - m_toolbar->Height());
    m_production_wnd->MoveTo(GG::Pt(GG::X0, m_toolbar->Height()));
    GG::GUI::GetGUI()->Register(m_production_wnd);
    m_production_wnd->Hide();
    GG::Connect(m_production_wnd->SystemSelectedSignal,     &MapWnd::SelectSystem, this);
    GG::Connect(m_production_wnd->PlanetSelectedSignal,     &MapWnd::SelectPlanet, this);


    // design window
    m_design_wnd = new DesignWnd(AppWidth(), AppHeight() - m_toolbar->Height());
    m_design_wnd->MoveTo(GG::Pt(GG::X0, m_toolbar->Height()));
    GG::GUI::GetGUI()->Register(m_design_wnd);
    m_design_wnd->Hide();


    boost::shared_ptr<GG::Font> font = ClientUI::GetFont();
    const GG::X BUTTON_TOTAL_MARGIN(12);


    // turn button
    // determine size from the text that will go into the button, using a test year string
    std::string turn_button_longest_reasonable_text =  boost::io::str(FlexibleFormat(UserString("MAP_BTN_TURN_UPDATE")) % "99999"); // it is unlikely a game will go over 100000 turns
    GG::X button_width = font->TextExtent(turn_button_longest_reasonable_text).x + BUTTON_TOTAL_MARGIN;
    // create button using determined width
    m_turn_update = new CUITurnButton(GG::X0, GG::Y0, button_width, turn_button_longest_reasonable_text);
    GG::Connect(m_turn_update->ClickedSignal, BoolToVoidAdapter(boost::bind(&MapWnd::EndTurn, this)));


    // FPS indicator
    m_FPS = new FPSIndicator(GG::X0, GG::Y0);
    m_FPS->Hide();


    // Zoom scale line
    m_scale_line = new MapScaleLine(GG::X(LAYOUT_MARGIN),   GG::Y(LAYOUT_MARGIN) + TOOLBAR_HEIGHT,
                                    SCALE_LINE_MAX_WIDTH,   SCALE_LINE_HEIGHT);
    GG::GUI::GetGUI()->Register(m_scale_line);
    m_scale_line->Update(ZoomFactor());
    m_scale_line->Hide();


    // Zoom slider
    const int ZOOM_SLIDER_MIN = static_cast<int>(ZOOM_IN_MIN_STEPS),
              ZOOM_SLIDER_MAX = static_cast<int>(ZOOM_IN_MAX_STEPS);
    m_zoom_slider = new CUISlider<double>(m_turn_update->UpperLeft().x, m_scale_line->LowerRight().y + GG::Y(LAYOUT_MARGIN),
                                  GG::X(ClientUI::ScrollWidth()), ZOOM_SLIDER_HEIGHT,
                                  ZOOM_SLIDER_MIN, ZOOM_SLIDER_MAX, GG::VERTICAL, GG::INTERACTIVE | GG::ONTOP);
    m_zoom_slider->SlideTo(m_zoom_steps_in);
    GG::GUI::GetGUI()->Register(m_zoom_slider);
    m_zoom_slider->Hide();


    GG::Connect(m_zoom_slider->SlidSignal,              &MapWnd::ZoomSlid,      this);
    GG::Connect(GetOptionsDB().OptionChangedSignal("UI.show-galaxy-map-zoom-slider"),   &MapWnd::RefreshSliders, this);


    // Subscreen / Menu buttons (placed right to left)


    // Menu button
    button_width = font->TextExtent(UserString("MAP_BTN_MENU")).x + BUTTON_TOTAL_MARGIN;
    m_btn_menu = new SettableInWindowCUIButton(GG::X0, GG::Y0, button_width, UserString("MAP_BTN_MENU"));
    GG::Connect(m_btn_menu->ClickedSignal, BoolToVoidAdapter(boost::bind(&MapWnd::ShowMenu, this)));
    // create custom InWindow function for Menu button that extends its
    // clickable area to the adjacent edges of the toolbar containing it
    boost::function<bool(const GG::Pt&)> in_window_func =
        boost::bind(&InRect, boost::bind(&WndLeft, m_btn_menu), boost::bind(&WndTop, m_toolbar),
                             boost::bind(&WndRight, m_toolbar), boost::bind(&WndBottom, m_btn_menu),
                    _1);
    m_btn_menu->SetInWindow(in_window_func);


    // Encyclo"pedia" button
    button_width = font->TextExtent(UserString("MAP_BTN_PEDIA")).x + BUTTON_TOTAL_MARGIN;
    m_btn_pedia = new SettableInWindowCUIButton(GG::X0, GG::Y0, button_width, UserString("MAP_BTN_PEDIA"));
    GG::Connect(m_btn_pedia->ClickedSignal, BoolToVoidAdapter(boost::bind(&MapWnd::TogglePedia, this)));
    in_window_func =
        boost::bind(&InRect, boost::bind(&WndLeft, m_btn_pedia),   boost::bind(&WndTop, m_toolbar),
                             boost::bind(&WndRight, m_btn_pedia),  boost::bind(&WndBottom, m_btn_pedia),
                    _1);
    m_btn_pedia->SetInWindow(in_window_func);


    // Design button
    button_width = font->TextExtent(UserString("MAP_BTN_DESIGN")).x + BUTTON_TOTAL_MARGIN;
    m_btn_design = new SettableInWindowCUIButton(GG::X0, GG::Y0, button_width, UserString("MAP_BTN_DESIGN"));
    GG::Connect(m_btn_design->ClickedSignal, BoolToVoidAdapter(boost::bind(&MapWnd::ToggleDesign, this)));
    in_window_func =
        boost::bind(&InRect, boost::bind(&WndLeft, m_btn_design),   boost::bind(&WndTop, m_toolbar),
                             boost::bind(&WndRight, m_btn_design),  boost::bind(&WndBottom, m_btn_design),
                    _1);
    m_btn_design->SetInWindow(in_window_func);


    // Production button
    button_width = font->TextExtent(UserString("MAP_BTN_PRODUCTION")).x + BUTTON_TOTAL_MARGIN;
    m_btn_production = new SettableInWindowCUIButton(GG::X0, GG::Y0, button_width, UserString("MAP_BTN_PRODUCTION"));
    GG::Connect(m_btn_production->ClickedSignal, BoolToVoidAdapter(boost::bind(&MapWnd::ToggleProduction, this)));
    in_window_func =
        boost::bind(&InRect, boost::bind(&WndLeft, m_btn_production),   boost::bind(&WndTop, m_toolbar),
                             boost::bind(&WndRight, m_btn_production),  boost::bind(&WndBottom, m_btn_production),
                    _1);
    m_btn_production->SetInWindow(in_window_func);


    // Research button
    button_width = font->TextExtent(UserString("MAP_BTN_RESEARCH")).x + BUTTON_TOTAL_MARGIN;
    m_btn_research = new SettableInWindowCUIButton(GG::X0, GG::Y0, button_width, UserString("MAP_BTN_RESEARCH"));
    GG::Connect(m_btn_research->ClickedSignal, BoolToVoidAdapter(boost::bind(&MapWnd::ToggleResearch, this)));
    in_window_func =
        boost::bind(&InRect, boost::bind(&WndLeft, m_btn_research),   boost::bind(&WndTop, m_toolbar),
                             boost::bind(&WndRight, m_btn_research),  boost::bind(&WndBottom, m_btn_research),
                    _1);
    m_btn_research->SetInWindow(in_window_func);


    // SitRep button
    button_width = font->TextExtent(UserString("MAP_BTN_SITREP")).x + BUTTON_TOTAL_MARGIN;
    m_btn_siterep = new SettableInWindowCUIButton(GG::X0, GG::Y0, button_width, UserString("MAP_BTN_SITREP"));
    GG::Connect(m_btn_siterep->ClickedSignal, BoolToVoidAdapter(boost::bind(&MapWnd::ToggleSitRep, this)));
    in_window_func =
        boost::bind(&InRect, boost::bind(&WndLeft, m_btn_siterep),   boost::bind(&WndTop, m_toolbar),
                             boost::bind(&WndRight, m_btn_siterep),  boost::bind(&WndBottom, m_btn_siterep),
                    _1);
    m_btn_siterep->SetInWindow(in_window_func);


    // Objects button
    button_width = font->TextExtent(UserString("MAP_BTN_OBJECTS")).x + BUTTON_TOTAL_MARGIN;
    m_btn_objects = new SettableInWindowCUIButton(GG::X0, GG::Y0, button_width, UserString("MAP_BTN_OBJECTS"));
    GG::Connect(m_btn_objects->ClickedSignal, BoolToVoidAdapter(boost::bind(&MapWnd::ToggleObjects, this)));
    in_window_func =
        boost::bind(&InRect, boost::bind(&WndLeft, m_btn_objects),   boost::bind(&WndTop, m_toolbar),
                             boost::bind(&WndRight, m_btn_objects),  boost::bind(&WndBottom, m_btn_objects),
                    _1);
    m_btn_objects->SetInWindow(in_window_func);


    // Moderator button
    button_width = font->TextExtent(UserString("MAP_BTN_MODERATOR")).x + BUTTON_TOTAL_MARGIN;
    m_btn_moderator = new SettableInWindowCUIButton(GG::X0, GG::Y0, button_width, UserString("MAP_BTN_MODERATOR"));
    GG::Connect(m_btn_moderator->ClickedSignal, BoolToVoidAdapter(boost::bind(&MapWnd::ToggleModeratorActions, this)));
    in_window_func =
        boost::bind(&InRect, boost::bind(&WndLeft, m_btn_moderator),    boost::bind(&WndTop, m_toolbar),
                             boost::bind(&WndRight, m_btn_moderator),   boost::bind(&WndBottom, m_btn_moderator),
                    _1);
    m_btn_moderator->SetInWindow(in_window_func);


    // resources
    const GG::X ICON_DUAL_WIDTH(100);
    const GG::X ICON_WIDTH(ICON_DUAL_WIDTH - 30);
    m_population = new StatisticIcon(GG::X0, GG::Y0, ICON_DUAL_WIDTH, m_turn_update->Height(),
                                     ClientUI::MeterIcon(METER_POPULATION),
                                     0, 3, false);
    m_population->SetName("Population StatisticIcon");

    m_industry = new StatisticIcon(GG::X0, GG::Y0, ICON_WIDTH, m_turn_update->Height(),
                                   ClientUI::MeterIcon(METER_INDUSTRY),
                                   0, 3, false);
    m_industry->SetName("Industry StatisticIcon");

    m_research = new StatisticIcon(GG::X0, GG::Y0, ICON_WIDTH, m_turn_update->Height(),
                                   ClientUI::MeterIcon(METER_RESEARCH),
                                   0, 3, false);
    m_research->SetName("Research StatisticIcon");

    m_trade = new StatisticIcon(GG::X0, GG::Y0, ICON_DUAL_WIDTH, m_turn_update->Height(),
                                ClientUI::MeterIcon(METER_TRADE),
                                0, 3, false);
    m_trade->SetName("Trade StatisticIcon");

    m_fleet = new StatisticIcon(GG::X0, GG::Y0, ICON_DUAL_WIDTH, m_turn_update->Height(),
                                ClientUI::GetTexture(ClientUI::ArtDir() / "icons" / "sitrep" / "fleet_arrived.png"),
                                0, 3, false);
    m_fleet->SetName("Fleet StatisticIcon");

    m_detection = new StatisticIcon(GG::X0, GG::Y0, ICON_DUAL_WIDTH, m_turn_update->Height(),
                                    ClientUI::MeterIcon(METER_DETECTION),
                                    0, 3, false);
    m_detection->SetName("Detection StatisticIcon");

    m_industry_wasted = new GG::Button(GG::X0, GG::Y0, ICON_WIDTH, GG::Y(Value(ICON_WIDTH)), "", font, GG::CLR_WHITE, GG::CLR_ZERO);
    m_research_wasted = new GG::Button(GG::X0, GG::Y0, ICON_WIDTH, GG::Y(Value(ICON_WIDTH)), "", font, GG::CLR_WHITE, GG::CLR_ZERO);

    boost::shared_ptr<GG::Texture> wasted_ressource_texture = ClientUI::GetTexture(ClientUI::ArtDir() / "icons" /"wasted_resource.png", false);
    GG::SubTexture wasted_ressource_subtexture = GG::SubTexture(wasted_ressource_texture, GG::X(0), GG::Y(0), GG::X(32), GG::Y(32));

    m_industry_wasted->SetUnpressedGraphic(wasted_ressource_subtexture);
    m_industry_wasted->SetPressedGraphic  (wasted_ressource_subtexture);
    m_industry_wasted->SetRolloverGraphic (wasted_ressource_subtexture);
    m_research_wasted->SetUnpressedGraphic(wasted_ressource_subtexture);
    m_research_wasted->SetPressedGraphic  (wasted_ressource_subtexture);
    m_research_wasted->SetRolloverGraphic (wasted_ressource_subtexture);

    m_industry_wasted->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
    m_research_wasted->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));

    GG::Connect(m_industry_wasted->ClickedSignal, BoolToVoidAdapter(boost::bind(&MapWnd::ZoomToSystemWithWastedPP,  this)));
    GG::Connect(m_research_wasted->ClickedSignal, BoolToVoidAdapter(boost::bind(&MapWnd::ToggleResearch,            this)));

    //Set the correct tooltips
    RefreshIndustryResourceIndicator();
    RefreshResearchResourceIndicator();

    m_menu_showing = false;

    int layout_column(0);

    layout->SetMinimumColumnWidth(layout_column, m_turn_update->Width());
    layout->SetColumnStretch(layout_column, 0.0);
    layout->Add(m_turn_update,      0, layout_column, GG::ALIGN_CENTER | GG::ALIGN_VCENTER);
    ++layout_column;

    layout->SetMinimumColumnWidth(layout_column, GG::X(ClientUI::Pts()*4));
    layout->SetColumnStretch(layout_column, 0.0);
    layout->Add(m_FPS,              0, layout_column, GG::ALIGN_CENTER | GG::ALIGN_VCENTER);
    ++layout_column;

    layout->SetMinimumColumnWidth(layout_column, GG::X(Value(layout->Height())));
    layout->SetColumnStretch(layout_column, 0.0);
    layout->Add(m_industry_wasted,  0, layout_column, GG::ALIGN_RIGHT | GG::ALIGN_VCENTER);
    ++layout_column;

    layout->SetColumnStretch(layout_column, 1.0);
    layout->Add(m_industry,         0, layout_column, GG::ALIGN_LEFT | GG::ALIGN_VCENTER);
    ++layout_column;

    layout->SetMinimumColumnWidth(layout_column, GG::X(Value(layout->Height())));
    layout->SetColumnStretch(layout_column, 0.0);
    layout->Add(m_research_wasted,  0, layout_column, GG::ALIGN_RIGHT | GG::ALIGN_VCENTER);
    ++layout_column;

    layout->SetColumnStretch(layout_column, 1.0);
    layout->Add(m_research,         0, layout_column, GG::ALIGN_LEFT | GG::ALIGN_VCENTER);
    ++layout_column;

    //layout->SetColumnStretch(layout_column, 1.0);
    //layout->Add(m_trade,            0, layout_column, GG::ALIGN_LEFT | GG::ALIGN_VCENTER);
    //++layout_column;

    layout->SetColumnStretch(layout_column, 1.0);
    layout->Add(m_fleet,            0, layout_column, GG::ALIGN_LEFT | GG::ALIGN_VCENTER);
    ++layout_column;

    layout->SetColumnStretch(layout_column, 1.0);
    layout->Add(m_population,       0, layout_column, GG::ALIGN_LEFT | GG::ALIGN_VCENTER);
    ++layout_column;

    layout->SetColumnStretch(layout_column, 1.0);
    layout->Add(m_detection,        0, layout_column, GG::ALIGN_LEFT | GG::ALIGN_VCENTER);
    ++layout_column;

    //layout->SetMinimumColumnWidth(layout_column, m_btn_moderator->Width());
    //layout->SetColumnStretch(layout_column, 0.0);
    //layout->Add(m_btn_moderator,    0, layout_column, GG::ALIGN_CENTER | GG::ALIGN_VCENTER);
    //++layout_column;

    layout->SetMinimumColumnWidth(layout_column, m_btn_objects->Width());
    layout->SetColumnStretch(layout_column, 0.0);
    layout->Add(m_btn_objects,      0, layout_column, GG::ALIGN_CENTER | GG::ALIGN_VCENTER);
    ++layout_column;

    layout->SetMinimumColumnWidth(layout_column, m_btn_siterep->Width());
    layout->SetColumnStretch(layout_column, 0.0);
    layout->Add(m_btn_siterep,      0, layout_column, GG::ALIGN_CENTER | GG::ALIGN_VCENTER);
    ++layout_column;

    layout->SetMinimumColumnWidth(layout_column, m_btn_research->Width());
    layout->SetColumnStretch(layout_column, 0.0);
    layout->Add(m_btn_research,     0, layout_column, GG::ALIGN_CENTER | GG::ALIGN_VCENTER);
    ++layout_column;

    layout->SetMinimumColumnWidth(layout_column, m_btn_production->Width());
    layout->SetColumnStretch(layout_column, 0.0);
    layout->Add(m_btn_production,   0, layout_column, GG::ALIGN_CENTER | GG::ALIGN_VCENTER);
    ++layout_column;

    layout->SetMinimumColumnWidth(layout_column, m_btn_design->Width());
    layout->SetColumnStretch(layout_column, 0.0);
    layout->Add(m_btn_design,       0, layout_column, GG::ALIGN_CENTER | GG::ALIGN_VCENTER);
    ++layout_column;

    layout->SetMinimumColumnWidth(layout_column, m_btn_pedia->Width());
    layout->SetColumnStretch(layout_column, 0.0);
    layout->Add(m_btn_pedia,        0, layout_column, GG::ALIGN_CENTER | GG::ALIGN_VCENTER);
    ++layout_column;

    layout->SetMinimumColumnWidth(layout_column, m_btn_menu->Width());
    layout->SetColumnStretch(layout_column, 0.0);
    layout->Add(m_btn_menu,         0, layout_column, GG::ALIGN_CENTER | GG::ALIGN_VCENTER);
    ++layout_column;

    layout->SetCellMargin(5);
    layout->SetBorderMargin(5);

    //clear background images
    m_backgrounds.clear();
    m_bg_scroll_rate.clear();

#if TEST_BROWSE_INFO
    boost::shared_ptr<GG::BrowseInfoWnd> browser_wnd(new BrowseFoo());
    GG::Wnd::SetDefaultBrowseInfoWnd(browser_wnd);
#endif

    Connect(ClientApp::GetApp()->EmpireEliminatedSignal,
            &MapWnd::HandleEmpireElimination,   this);
    Connect(FleetUIManager::GetFleetUIManager().ActiveFleetWndChangedSignal,
            &MapWnd::SelectedFleetsChanged,     this);
    Connect(FleetUIManager::GetFleetUIManager().ActiveFleetWndSelectedFleetsChangedSignal,
            &MapWnd::SelectedFleetsChanged,     this);
    Connect(FleetUIManager::GetFleetUIManager().ActiveFleetWndSelectedShipsChangedSignal,
            &MapWnd::SelectedShipsChanged,      this);

    Connect(ClientUI::GetClientUI()->GetMessageWnd()->TypingSignal,
            &MapWnd::DisableAlphaNumAccels,     this);
    Connect(ClientUI::GetClientUI()->GetMessageWnd()->DoneTypingSignal,
            &MapWnd::EnableAlphaNumAccels,     this);

    DoLayout();
    SetAccelerators();
}

MapWnd::~MapWnd() {
    delete m_toolbar;
    delete m_scale_line;
    delete m_zoom_slider;
    delete m_sitrep_panel;
    delete m_object_list_wnd;
    delete m_pedia_panel;
    delete m_research_wnd;
    delete m_production_wnd;
    delete m_design_wnd;
}

void MapWnd::DoLayout()
{ m_toolbar->Resize(GG::Pt(AppWidth(), TOOLBAR_HEIGHT)); }

GG::Pt MapWnd::ClientUpperLeft() const
{ return UpperLeft() + GG::Pt(AppWidth(), AppHeight()); }

double MapWnd::ZoomFactor() const
{ return ZoomScaleFactor(m_zoom_steps_in); }

GG::Pt MapWnd::ScreenCoordsFromUniversePosition(double universe_x, double universe_y) const {
    GG::Pt cl_ul = ClientUpperLeft();
    GG::X x((universe_x * ZoomFactor()) + cl_ul.x);
    GG::Y y((universe_y * ZoomFactor()) + cl_ul.y);
    return GG::Pt(x, y);
}

std::pair<double, double> MapWnd::UniversePositionFromScreenCoords(GG::Pt screen_coords) const {
    GG::Pt cl_ul = ClientUpperLeft();
    double x = Value((screen_coords - cl_ul).x / ZoomFactor());
    double y = Value((screen_coords - cl_ul).y / ZoomFactor());
    return std::pair<double, double>(x, y);
}

void MapWnd::GetSaveGameUIData(SaveGameUIData& data) const {
    data.map_left = Value(UpperLeft().x);
    data.map_top = Value(UpperLeft().y);
    data.map_zoom_steps_in = m_zoom_steps_in;
    data.fleets_exploring = m_fleets_exploring;
}

bool MapWnd::InProductionViewMode() const
{ return m_in_production_view_mode; }

void MapWnd::Render() {
    // HACK! This is placed here so we can be sure it is executed frequently
    // (every time we render), and before we render any of the
    // FleetWnds.  It doesn't necessarily belong in MapWnd at all.
    FleetUIManager::GetFleetUIManager().CullEmptyWnds();

    // save CPU / GPU activity by skipping rendering when it's not needed
    if (m_design_wnd->Visible())
        return; // as of this writing, the design screen has a fully opaque background

    glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

    RenderStarfields();

    GG::Pt origin_offset = UpperLeft() + GG::Pt(AppWidth(), AppHeight());

    glPushMatrix();
    glLoadIdentity();

    glScalef(static_cast<GLfloat>(ZoomFactor()), static_cast<GLfloat>(ZoomFactor()), 1.0f);
    glTranslatef(static_cast<GLfloat>(Value(origin_offset.x / ZoomFactor())),
                 static_cast<GLfloat>(Value(origin_offset.y / ZoomFactor())),
                 0.0f);

    RenderGalaxyGas();
    RenderVisibilityRadii();
    RenderFields();
    RenderSystemOverlays();
    RenderStarlanes();
    RenderSystems();
    RenderFleetMovementLines();

    glPopMatrix();
    glPopClientAttrib();
}

void MapWnd::RenderStarfields() {
    if (!GetOptionsDB().Get<bool>("UI.galaxy-starfields"))
        return;

    glColor3d(1.0, 1.0, 1.0);

    GG::Pt origin_offset =
        UpperLeft() + GG::Pt(AppWidth(), AppHeight());
    glMatrixMode(GL_TEXTURE);

    for (unsigned int i = 0; i < m_backgrounds.size(); ++i) {
        float texture_coords_per_pixel_x = 1.0f / Value(m_backgrounds[i]->Width());
        float texture_coords_per_pixel_y = 1.0f / Value(m_backgrounds[i]->Height());
        glScalef(static_cast<GLfloat>(Value(texture_coords_per_pixel_x * Width())),
                 static_cast<GLfloat>(Value(texture_coords_per_pixel_y * Height())),
                 1.0f);
        glTranslatef(static_cast<GLfloat>(Value(-texture_coords_per_pixel_x * origin_offset.x / 16.0f * m_bg_scroll_rate[i])),
                     static_cast<GLfloat>(Value(-texture_coords_per_pixel_y * origin_offset.y / 16.0f * m_bg_scroll_rate[i])),
                     0.0f);
        glBindTexture(GL_TEXTURE_2D, m_backgrounds[i]->OpenGLId());
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i(0, 0);
        glTexCoord2f(0.0f, 1.0f);
        glVertex(GG::X0, Height());
        glTexCoord2f(1.0f, 1.0f);
        glVertex(Width(), Height());
        glTexCoord2f(1.0f, 0.0f);
        glVertex(Width(), GG::Y0);
        glEnd();
        glLoadIdentity();
    }

    glMatrixMode(GL_MODELVIEW);
}

void MapWnd::RenderFields() {
    glPushMatrix();
    glLoadIdentity();
    glColor(GG::CLR_WHITE);
    for (std::map<int, FieldIcon*>::const_iterator it = m_field_icons.begin();
         it != m_field_icons.end(); ++it)
    {
        const FieldIcon* icon = it->second;
        GG::Pt ul = icon->UpperLeft();
        GG::Pt lr = icon->LowerRight();
        boost::shared_ptr<GG::Texture> texture = icon->FieldTexture();
        if (texture)
            texture->OrthoBlit(ul, lr);
    }
    glPopMatrix();
}

void MapWnd::RenderGalaxyGas() {
    if (!GetOptionsDB().Get<bool>("UI.galaxy-gas-background"))
        return;
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    for (std::map<boost::shared_ptr<GG::Texture>, GL2DVertexBuffer>::const_iterator it =
            m_galaxy_gas_quad_vertices.begin(); it != m_galaxy_gas_quad_vertices.end(); ++it)
    {
        glBindTexture(GL_TEXTURE_2D, it->first->OpenGLId());
        it->second.activate();
        m_star_texture_coords.activate();
        glDrawArrays(GL_QUADS, 0, it->second.size());
    }

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glPopClientAttrib();
}

void MapWnd::RenderSystemOverlays() {
    glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glPushMatrix();
    glLoadIdentity();
    for (std::map<int, SystemIcon*>::const_iterator it = m_system_icons.begin();
         it != m_system_icons.end(); ++it)
    { it->second->RenderOverlay(ZoomFactor()); }
    glPopMatrix();
    glPopClientAttrib();
}

void MapWnd::RenderSystems() {
    const float HALO_SCALE_FACTOR = static_cast<float>(SystemHaloScaleFactor());
    int empire_id = HumanClientApp::GetApp()->EmpireID();

    glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    if (GetOptionsDB().Get<bool>("UI.optimized-system-rendering")) {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        if (0.5f < HALO_SCALE_FACTOR && m_star_texture_coords.size()) {
            glMatrixMode(GL_TEXTURE);
            glTranslatef(0.5f, 0.5f, 0.0f);
            glScalef(1.0f / HALO_SCALE_FACTOR, 1.0f / HALO_SCALE_FACTOR, 1.0f);
            glTranslatef(-0.5f, -0.5f, 0.0f);
            for (std::map<boost::shared_ptr<GG::Texture>, GL2DVertexBuffer>::const_iterator it = m_star_halo_quad_vertices.begin();
                 it != m_star_halo_quad_vertices.end(); ++it)
            {
                if (!it->second.size())
                    continue;

                glBindTexture(GL_TEXTURE_2D, it->first->OpenGLId());
                it->second.activate();
                m_star_texture_coords.activate();
                glDrawArrays(GL_QUADS, 0, it->second.size());
            }
            glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);
        }

        if (m_star_texture_coords.size() &&
            ClientUI::SystemTinyIconSizeThreshold() < ZoomFactor() * ClientUI::SystemIconSize())
        {
            for (std::map<boost::shared_ptr<GG::Texture>, GL2DVertexBuffer>::const_iterator it = m_star_core_quad_vertices.begin();
                 it != m_star_core_quad_vertices.end(); ++it)
            {
                if (!it->second.size())
                    continue;

                glBindTexture(GL_TEXTURE_2D, it->first->OpenGLId());

                it->second.activate();

                m_star_texture_coords.activate();
                glDrawArrays(GL_QUADS, 0, it->second.size());
            }
        }
    } else {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glPushMatrix();
        glLoadIdentity();
        for (std::map<int, SystemIcon*>::const_iterator it = m_system_icons.begin();
             it != m_system_icons.end(); ++it)
        { it->second->RenderHalo(HALO_SCALE_FACTOR); }
        for (std::map<int, SystemIcon*>::const_iterator it = m_system_icons.begin();
             it != m_system_icons.end(); ++it)
        { it->second->RenderDisc(); }
        glPopMatrix();
    }

    // circles around system icons and fog over unexplored systems
    bool circles = GetOptionsDB().Get<bool>("UI.system-circles");
    bool fog_scanlines = false;
    float fog_scanline_spacing = 4.0f;
    Universe& universe = GetUniverse();

    if (empire_id != ALL_EMPIRES && GetOptionsDB().Get<bool>("UI.system-fog-of-war")) {
        fog_scanlines = true;
        fog_scanline_spacing = static_cast<float>(GetOptionsDB().Get<double>("UI.system-fog-of-war-spacing"));
    }

    if (fog_scanlines || circles) {
        glPushMatrix();
        glLoadIdentity();
        const double TWO_PI = 2.0*3.14159;
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_LINE_SMOOTH);
        glLineWidth(1.5f);
        glColor(GetOptionsDB().Get<StreamableColor>("UI.unowned-starlane-colour").ToClr());

        for (std::map<int, SystemIcon*>::const_iterator it = m_system_icons.begin();
             it != m_system_icons.end(); ++it)
        {
            const SystemIcon* icon = it->second;

            const int ARC_SIZE = icon->EnclosingCircleDiameter();

            GG::Pt ul = icon->UpperLeft(), lr = icon->LowerRight();
            GG::Pt size = lr - ul;
            GG::Pt half_size = GG::Pt(size.x / 2, size.y / 2);
            GG::Pt middle = ul + half_size;

            GG::Pt circle_size = GG::Pt(static_cast<GG::X>(ARC_SIZE),
                                        static_cast<GG::Y>(ARC_SIZE));
            GG::Pt circle_half_size = GG::Pt(circle_size.x / 2, circle_size.y / 2);
            GG::Pt circle_ul = middle - circle_half_size;
            GG::Pt circle_lr = circle_ul + circle_size;

            if (fog_scanlines && m_scanline_shader) {
                if (universe.GetObjectVisibilityByEmpire(it->first, empire_id) <= VIS_BASIC_VISIBILITY) {
                    m_scanline_shader->Use();
                    m_scanline_shader->Bind("scanline_spacing", fog_scanline_spacing);
                    CircleArc(circle_ul, circle_lr, 0.0, TWO_PI, true);
                    m_scanline_shader->stopUse();
                }
            }

            // render circles around systems that have at least one starlane, if circles are enabled.
            if (circles) {
                if (const System* system = GetSystem(it->first))
                    if (system->NumStarlanes() > 0)
                        CircleArc(circle_ul, circle_lr, 0.0, TWO_PI, false);
            }
        }

        glDisable(GL_LINE_SMOOTH);
        glEnable(GL_TEXTURE_2D);
        glPopMatrix();
        glLineWidth(1.0f);
    }

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glPopClientAttrib();
}

void MapWnd::RenderStarlanes() {
    bool coloured = GetOptionsDB().Get<bool>("UI.resource-starlane-colouring");
    float core_multiplier = static_cast<float>(GetOptionsDB().Get<double>("UI.starlane-core-multiplier"));
    RenderStarlanes(m_RC_starlane_vertices, m_RC_starlane_colors, core_multiplier, coloured, false);
    RenderStarlanes(m_starlane_vertices, m_starlane_colors, 1.0, coloured, true);
}

void MapWnd::RenderStarlanes(GL2DVertexBuffer& vertices, GLRGBAColorBuffer& colours,
                             double thickness, bool coloured, bool doBase) {
    if (vertices.size() && (colours.size() || !coloured) && (coloured || doBase)) {
        // render starlanes with vertex buffer (and possibly colour buffer)
        const GG::Clr UNOWNED_LANE_COLOUR = GetOptionsDB().Get<StreamableColor>("UI.unowned-starlane-colour").ToClr();

        glDisable(GL_TEXTURE_2D);
        glEnable(GL_LINE_SMOOTH);
        glEnable(GL_LINE_STIPPLE);

        glLineWidth(static_cast<GLfloat>(thickness * GetOptionsDB().Get<double>("UI.starlane-thickness")));
        glLineStipple(1, 0xffff);   // solid line / no stipple

        glPushAttrib(GL_COLOR_BUFFER_BIT);
        glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
        glEnableClientState(GL_VERTEX_ARRAY);

        if (coloured)
            glEnableClientState(GL_COLOR_ARRAY);
        else
            glColor(UNOWNED_LANE_COLOUR);

        vertices.activate();

        if (coloured)
            colours.activate();

        glDrawArrays(GL_LINES, 0, vertices.size());

        glLineWidth(1.0);

        glPopClientAttrib();
        glPopAttrib();

        glEnable(GL_TEXTURE_2D);
        glDisable(GL_LINE_SMOOTH);
        glDisable(GL_LINE_STIPPLE);
    }

    glLineWidth(1.0);
}

void MapWnd::RenderFleetMovementLines() {
    if (ZoomFactor() < ClientUI::TinyFleetButtonZoomThreshold())
        return;

    // set common animation shift for move lines
    const int       MOVE_LINE_DOT_SPACING = GetOptionsDB().Get<int>("UI.fleet-supply-line-dot-spacing");
    const double    RATE                  = GetOptionsDB().Get<double>("UI.fleet-supply-line-dot-rate");
    move_line_animation_shift = static_cast<double>(static_cast<int>(static_cast<double>(GG::GUI::GetGUI()->Ticks()) * RATE) % MOVE_LINE_DOT_SPACING);


    // render movement lines for all fleets
    for (std::map<int, MovementLineData>::const_iterator it = m_fleet_lines.begin(); it != m_fleet_lines.end(); ++it)
        RenderMovementLine(it->second);

    // re-render selected fleets' movement lines in white
    for (std::set<int>::const_iterator it = m_selected_fleet_ids.begin(); it != m_selected_fleet_ids.end(); ++it) {
        int fleet_id = *it;
        std::map<int, MovementLineData>::const_iterator line_it = m_fleet_lines.find(fleet_id);
        if (line_it != m_fleet_lines.end())
            RenderMovementLine(line_it->second, GG::CLR_WHITE);
    }

    // render move line ETA indicators for selected fleets
    for (std::set<int>::const_iterator it = m_selected_fleet_ids.begin(); it != m_selected_fleet_ids.end(); ++it) {
        int fleet_id = *it;
        std::map<int, MovementLineData>::const_iterator line_it = m_fleet_lines.find(fleet_id);
        if (line_it != m_fleet_lines.end())
            RenderMovementLineETAIndicators(line_it->second);
    }

    // render projected move lines
    for (std::map<int, MovementLineData>::const_iterator it = m_projected_fleet_lines.begin(); it != m_projected_fleet_lines.end(); ++it)
        RenderMovementLine(it->second, GG::CLR_WHITE);

    // render projected move line ETA indicators
    for (std::map<int, MovementLineData>::const_iterator it = m_projected_fleet_lines.begin(); it != m_projected_fleet_lines.end(); ++it)
        RenderMovementLineETAIndicators(it->second, GG::CLR_WHITE);
}

void MapWnd::RenderMovementLine(const MapWnd::MovementLineData& move_line, GG::Clr clr) {
    const std::vector<MovementLineData::Vertex>& vertices = move_line.vertices;
    if (vertices.empty())
        return; // nothing to draw.  need at least two nodes at different locations to draw a line
    if (vertices.size() % 2 == 1) {
        Logger().errorStream() << "RenderMovementLine given an odd number of vertices to render?!";
        return;
    }

    glPushMatrix();
    glLoadIdentity();

    if (clr == GG::CLR_ZERO) {
        glColor(move_line.colour);
    } else {
        glColor(clr);
    }

    boost::shared_ptr<GG::Texture> move_line_dot_texture =
        ClientUI::GetTexture(ClientUI::ArtDir() / "misc" / "move_line_dot.png");
    GG::Pt      texture_half_size = GG::Pt(GG::X(move_line_dot_texture->DefaultWidth() / 2),
                                           GG::Y(move_line_dot_texture->DefaultHeight() / 2));
    const int   MOVE_LINE_DOT_SPACING = GetOptionsDB().Get<int>("UI.fleet-supply-line-dot-spacing");

    double offset = move_line_animation_shift;  // step along line in by move_line_animation_shift to get position of first dot

    // blit a series of coloured dots along move path
    for (std::vector<MovementLineData::Vertex>::const_iterator verts_it = vertices.begin(); verts_it != vertices.end(); ++verts_it) {
        // get next two vertices
        const MovementLineData::Vertex& vert1 = *verts_it;
        verts_it++;
        const MovementLineData::Vertex& vert2 = *verts_it;

        GG::Pt vert1Pt = ScreenCoordsFromUniversePosition(vert1.x, vert1.y) - texture_half_size;
        GG::Pt vert2Pt = ScreenCoordsFromUniversePosition(vert2.x, vert2.y) - texture_half_size;

        // get unit vector along line connecting vertices
        double deltaX = Value(vert2Pt.x - vert1Pt.x), deltaY = Value(vert2Pt.y - vert1Pt.y);
        double length = std::sqrt(deltaX*deltaX + deltaY*deltaY);
        if (length == 0.0) // safety check
            length = 1.0;
        double uVecX = deltaX / length, uVecY = deltaY / length;


        // increment along line, rendering dots, until end of line segment is passed
        while (offset < length) {
            // find position of dot from initial vertex position, offset length and unit vectors
            std::pair<double, double> ul(Value(vert1Pt.x) + offset * uVecX,
                                         Value(vert1Pt.y) + offset * uVecY);

            // blit texture (appropriately coloured) into place
            glBindTexture(GL_TEXTURE_2D, move_line_dot_texture->OpenGLId());
            const GLfloat* tex_coords = move_line_dot_texture->DefaultTexCoords();
            glBegin(GL_TRIANGLE_STRIP);
            glTexCoord2f(tex_coords[0], tex_coords[1]);
            glVertex2f(static_cast<GLfloat>(ul.first), static_cast<GLfloat>(ul.second));
            glTexCoord2f(tex_coords[2], tex_coords[1]);
            glVertex2f(static_cast<GLfloat>(ul.first + Value(move_line_dot_texture->DefaultWidth())), static_cast<GLfloat>(ul.second));
            glTexCoord2f(tex_coords[0], tex_coords[3]);
            glVertex2f(static_cast<GLfloat>(ul.first), static_cast<GLfloat>(ul.second + Value(move_line_dot_texture->DefaultHeight())));
            glTexCoord2f(tex_coords[2], tex_coords[3]);
            glVertex2f(static_cast<GLfloat>(ul.first + Value(move_line_dot_texture->DefaultWidth())),
                       static_cast<GLfloat>(ul.second + Value(move_line_dot_texture->DefaultHeight())));
            glEnd();

            // move offset to that for next dot
            offset += MOVE_LINE_DOT_SPACING;
        }

        offset -= length;   // so next segment's dots meld smoothly into this segment's
    }
    glPopMatrix();
}

void MapWnd::RenderMovementLineETAIndicators(const MapWnd::MovementLineData& move_line, GG::Clr clr) {
    const std::vector<MovementLineData::Vertex>& vertices = move_line.vertices;
    if (vertices.empty())
        return; // nothing to draw.


    const double MARKER_HALF_SIZE = 9;
    const int MARKER_PTS = ClientUI::Pts();
    boost::shared_ptr<GG::Font> font = ClientUI::GetBoldFont(MARKER_PTS);
    const double TWO_PI = 2.0*3.1415926536;
    GG::Flags<GG::TextFormat> flags = GG::FORMAT_CENTER | GG::FORMAT_VCENTER;

    glPushMatrix();
    glLoadIdentity();
    for (std::vector<MovementLineData::Vertex>::const_iterator verts_it = vertices.begin(); verts_it != vertices.end(); ++verts_it) {
        const MovementLineData::Vertex& vert = *verts_it;
        if (!vert.show_eta)
            continue;


        // draw background disc in empire colour, or passed-in colour
        GG::Pt marker_centre = ScreenCoordsFromUniversePosition(vert.x, vert.y);

        if (clr == GG::CLR_ZERO)
            glColor(move_line.colour);
        else
            glColor(clr);

        GG::Pt ul = marker_centre - GG::Pt(GG::X(static_cast<int>(MARKER_HALF_SIZE)), GG::Y(static_cast<int>(MARKER_HALF_SIZE)));
        GG::Pt lr = marker_centre + GG::Pt(GG::X(static_cast<int>(MARKER_HALF_SIZE)), GG::Y(static_cast<int>(MARKER_HALF_SIZE)));

        glDisable(GL_TEXTURE_2D);
        CircleArc(ul, lr, 0.0, TWO_PI, true);
        glEnable(GL_TEXTURE_2D);


        // render ETA number in white with black shadows
        std::string text = boost::lexical_cast<std::string>(vert.eta);

        glColor(GG::CLR_BLACK);
        font->RenderText(ul + GG::Pt(-GG::X1,  GG::Y0), lr + GG::Pt(-GG::X1,  GG::Y0), text, flags);
        font->RenderText(ul + GG::Pt( GG::X1,  GG::Y0), lr + GG::Pt( GG::X1,  GG::Y0), text, flags);
        font->RenderText(ul + GG::Pt( GG::X0, -GG::Y1), lr + GG::Pt( GG::X0, -GG::Y1), text, flags);
        font->RenderText(ul + GG::Pt( GG::X0,  GG::Y1), lr + GG::Pt( GG::X0,  GG::Y1), text, flags);

        glColor(GG::CLR_WHITE);
        font->RenderText(ul, lr, text, flags);
    }
    glPopMatrix();
}

void MapWnd::RenderVisibilityRadii() {
    if (!GetOptionsDB().Get<bool>("UI.show-detection-range"))
        return;

    int                     client_empire_id = HumanClientApp::GetApp()->EmpireID();
    const std::set<int>&    destroyed_object_ids = GetUniverse().DestroyedObjectIds();
    const std::set<int>&    stale_object_ids = GetUniverse().EmpireStaleKnowledgeObjectIDs(client_empire_id);
    const ObjectMap&        objects = GetUniverse().Objects();

    // for each map position and empire, find max value of detection range at that position
    std::map<std::pair<int, std::pair<double, double> >, float> empire_position_max_detection_ranges;

    for (ObjectMap::const_iterator<> it = objects.const_begin(); it != objects.const_end(); ++it) {
        int object_id = it->ID();
        // skip destroyed objects
        if (destroyed_object_ids.find(object_id) != destroyed_object_ids.end())
            continue;
        // skip stale objects
        if (stale_object_ids.find(object_id) != stale_object_ids.end())
            continue;

        const UniverseObject* obj = *it;

        // skip unowned objects
        if (obj->Unowned())
            continue;

        // skip objects not at least partially visible this turn
        if (obj->GetVisibility(client_empire_id) <= VIS_BASIC_VISIBILITY)
            continue;

        // don't show radii for fleets or moving ships
        if (obj->ObjectType() == OBJ_FLEET)
            continue;
        if (obj->ObjectType() == OBJ_SHIP) {
            const Ship* ship = universe_object_cast<const Ship*>(obj);
            if (!ship)
                continue;
            const Fleet* fleet = objects.Object<Fleet>(ship->FleetID());
            if (!fleet)
                continue;
            int next_id = fleet->NextSystemID();
            int cur_id = fleet->SystemID();
            if (next_id != INVALID_OBJECT_ID && next_id != cur_id)
                continue;
        }

        const Meter* detection_meter = obj->GetMeter(METER_DETECTION);
        if (!detection_meter)
            continue;

        // if this object has the largest yet checked visibility range at this location, update the location's range
        double X = obj->X();
        double Y = obj->Y();
        float D = detection_meter->Current();
        // skip objects that don't contribute detection
        if (D <= 0.0f)
            continue;

        // find this empires entry for this location, if any
        std::pair<int, std::pair<double, double> > key = std::make_pair(obj->Owner(), std::make_pair(X, Y));
        std::map<std::pair<int, std::pair<double, double> >, float>::iterator range_it =
            empire_position_max_detection_ranges.find(key);
        if (range_it != empire_position_max_detection_ranges.end()) {
            if (range_it->second < D) range_it->second = D; // update existing entry
        } else {
            empire_position_max_detection_ranges[key] = D;  // add new entry to map
        }
    }

    std::map<GG::Clr, std::vector<std::pair<GG::Pt, GG::Pt> >, ClrLess> circles;
    for (std::map<std::pair<int, std::pair<double, double> >, float>::const_iterator it =
            empire_position_max_detection_ranges.begin();
         it != empire_position_max_detection_ranges.end(); ++it)
    {
        if (const Empire* empire = Empires().Lookup(it->first.first)) {
            GG::Clr circle_colour = empire->Color();
            circle_colour.a = 64;

            GG::Pt circle_centre = ScreenCoordsFromUniversePosition(it->first.second.first, it->first.second.second);
            double radius = it->second*ZoomFactor();
            if (radius < 20.0)
                continue;

            GG::Pt ul = circle_centre - GG::Pt(GG::X(static_cast<int>(radius)), GG::Y(static_cast<int>(radius)));
            GG::Pt lr = circle_centre + GG::Pt(GG::X(static_cast<int>(radius)), GG::Y(static_cast<int>(radius)));

            circles[circle_colour].push_back(std::make_pair(ul, lr));
        }
    }


    glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
    glEnableClientState(GL_VERTEX_ARRAY);

#define USE_STENCILS 1

#if USE_STENCILS
    glPushAttrib(GL_ENABLE_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_STENCIL_TEST);
#endif

    const double TWO_PI = 2.0*3.1415926536;
    const GG::Pt UNIT(GG::X1, GG::Y1);

    glLineWidth(1.5);
    glPushMatrix();
    glLoadIdentity();
    glEnable(GL_LINE_SMOOTH);
    glDisable(GL_TEXTURE_2D);

    for (std::map<GG::Clr, std::vector<std::pair<GG::Pt, GG::Pt> >, ClrLess>::iterator it = circles.begin();
         it != circles.end(); ++it)
    {
        glClear(GL_STENCIL_BUFFER_BIT);
        glStencilOp(GL_INCR, GL_INCR, GL_INCR);
        glStencilFunc(GL_EQUAL, 0x0, 0xff);
        glColor(it->first);
        const std::vector<std::pair<GG::Pt, GG::Pt> >& circles_in_this_colour = it->second;
        for (std::size_t i = 0; i < circles_in_this_colour.size(); ++i) {
            CircleArc(circles_in_this_colour[i].first, circles_in_this_colour[i].second,
                      0.0, TWO_PI, true);
        }
        glStencilFunc(GL_GREATER, 0x2, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        for (std::size_t i = 0; i < circles_in_this_colour.size(); ++i) {
            CircleArc(circles_in_this_colour[i].first + UNIT, circles_in_this_colour[i].second - UNIT,
                      0.0, TWO_PI, false);
        }
    }

#if !USE_STENCILS
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LINE_SMOOTH);
#endif

    glPopMatrix();
    glLineWidth(1.0);

#if USE_STENCILS
    glPopAttrib();
#endif

#undef USE_STENCILS

    glPopClientAttrib();
}

void MapWnd::LButtonDown(const GG::Pt &pt, GG::Flags<GG::ModKey> mod_keys)
{ m_drag_offset = pt - ClientUpperLeft(); }

void MapWnd::LDrag(const GG::Pt &pt, const GG::Pt &move, GG::Flags<GG::ModKey> mod_keys) {
    GG::Pt move_to_pt = pt - m_drag_offset;
    CorrectMapPosition(move_to_pt);

    MoveTo(move_to_pt - GG::Pt(AppWidth(), AppHeight()));
    m_dragged = true;
}

void MapWnd::LButtonUp(const GG::Pt &pt, GG::Flags<GG::ModKey> mod_keys) {
    m_drag_offset = GG::Pt(-GG::X1, -GG::Y1);
    m_dragged = false;
}

void MapWnd::LClick(const GG::Pt &pt, GG::Flags<GG::ModKey> mod_keys) {
    m_drag_offset = GG::Pt(-GG::X1, -GG::Y1);
    if (!m_dragged && !m_in_production_view_mode) {
        SelectSystem(INVALID_OBJECT_ID);
        m_side_panel->Hide();
    }
    m_dragged = false;


    HumanClientApp* app = HumanClientApp::GetApp();
    ClientNetworking& net = app->Networking();
    bool moderator = false;
    if (app->GetPlayerClientType(app->PlayerID()) == Networking::CLIENT_TYPE_HUMAN_MODERATOR)
        moderator = true;
    if (!moderator)
        return;

    std::pair<double, double> u_pos = this->UniversePositionFromScreenCoords(pt);

    net.SendMessage(ModeratorActionMessage(app->PlayerID(),
        Moderator::CreateSystem(u_pos.first, u_pos.second, STAR_BLUE)));
}

void MapWnd::RClick(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys) {
    // Attempt to close open fleet windows (if any are open and this is allowed), then attempt to close the SidePanel (if open);
    // if these fail, go ahead with the context-sensitive popup menu . Note that this enforces a one-close-per-click policy.
    if (GetOptionsDB().Get<bool>("UI.window-quickclose")) {
        if (FleetUIManager::GetFleetUIManager().CloseAll())
            return;

        if (m_side_panel->Visible()) {
            m_side_panel->Hide();
            return;
        }
    }

    if (GetOptionsDB().Get<bool>("UI.map-right-click-popup-menu")) {
        // create popup menu with map options in it.
        GG::MenuItem menu_contents;
        bool fps            = GetOptionsDB().Get<bool>("show-fps");
        bool showPlanets    = GetOptionsDB().Get<bool>("UI.sidepanel-planet-shown");
        bool systemCircles  = GetOptionsDB().Get<bool>("UI.system-circles");
        bool resourceColor  = GetOptionsDB().Get<bool>("UI.resource-starlane-colouring");
        bool fleetSupply    = GetOptionsDB().Get<bool>("UI.fleet-supply-lines");
        bool gas            = GetOptionsDB().Get<bool>("UI.galaxy-gas-background");
        bool starfields     = GetOptionsDB().Get<bool>("UI.galaxy-starfields");
        bool scale          = GetOptionsDB().Get<bool>("UI.show-galaxy-map-scale");
        bool zoomSlider     = GetOptionsDB().Get<bool>("UI.show-galaxy-map-zoom-slider");
        bool detectionRange = GetOptionsDB().Get<bool>("UI.show-detection-range");
        menu_contents.next_level.push_back(GG::MenuItem(UserString("OPTIONS_SHOW_FPS"),            1, false, fps));
        menu_contents.next_level.push_back(GG::MenuItem(UserString("OPTIONS_SHOW_SIDEPANEL_PLANETS"),  3, false, showPlanets));
        menu_contents.next_level.push_back(GG::MenuItem(UserString("OPTIONS_UI_SYSTEM_CIRCLES"),         4, false, systemCircles));
        menu_contents.next_level.push_back(GG::MenuItem(UserString("OPTIONS_RESOURCE_STARLANE_COLOURING"), 5, false, resourceColor));
        menu_contents.next_level.push_back(GG::MenuItem(UserString("OPTIONS_FLEET_SUPPLY_LINES"),    6, false, fleetSupply));
        menu_contents.next_level.push_back(GG::MenuItem(UserString("OPTIONS_GALAXY_MAP_GAS"),         7, false, gas));
        menu_contents.next_level.push_back(GG::MenuItem(UserString("OPTIONS_GALAXY_MAP_STARFIELDS"),   8, false, starfields));
        menu_contents.next_level.push_back(GG::MenuItem(UserString("OPTIONS_GALAXY_MAP_SCALE_LINE"),    9, false, scale));
        menu_contents.next_level.push_back(GG::MenuItem(UserString("OPTIONS_GALAXY_MAP_ZOOM_SLIDER"),    10, false, zoomSlider));
        menu_contents.next_level.push_back(GG::MenuItem(UserString("OPTIONS_GALAXY_MAP_DETECTION_RANGE"), 11, false, detectionRange));
        // display popup menu
        GG::PopupMenu popup(pt.x, pt.y, ClientUI::GetFont(), menu_contents, ClientUI::TextColor(),
                            ClientUI::WndOuterBorderColor(), ClientUI::WndColor());
        if (popup.Run()) {
            switch (popup.MenuID()) {
                case 1: { GetOptionsDB().Set<bool>("show-fps",                       !fps);        break; }
                case 3: { GetOptionsDB().Set<bool>("UI.sidepanel-planet-shown",      !showPlanets);  break; }
                case 4: { GetOptionsDB().Set<bool>("UI.system-circles",              !systemCircles); break; }
                case 5: { GetOptionsDB().Set<bool>("UI.resource-starlane-colouring", !resourceColor);  break; }
                case 6: { GetOptionsDB().Set<bool>("UI.fleet-supply-lines",          !fleetSupply);     break; }
                case 7: { GetOptionsDB().Set<bool>("UI.galaxy-gas-background",       !gas);        break; }
                case 8: { GetOptionsDB().Set<bool>("UI.galaxy-starfields",           !starfields);  break; }
                case 9: { GetOptionsDB().Set<bool>("UI.show-galaxy-map-scale",       !scale);        break; }
                case 10: { GetOptionsDB().Set<bool>("UI.show-galaxy-map-zoom-slider",!zoomSlider);    break; }
                case 11: { GetOptionsDB().Set<bool>("UI.show-detection-range",       !detectionRange); break; }
                default: break;
            }
        }
    }
}

void MapWnd::MouseWheel(const GG::Pt& pt, int move, GG::Flags<GG::ModKey> mod_keys) {
    if (move)
        Zoom(move, pt);
}

void MapWnd::EnableOrderIssuing(bool enable/* = true*/) {
    // disallow order enabling if this client does not have an empire
    // and is not a moderator
    HumanClientApp* app = HumanClientApp::GetApp();
    if (!app) {
        enable = false;
    } else {
        bool have_empire = (app->EmpireID() != ALL_EMPIRES);
        bool moderator = (app->GetPlayerClientType(app->PlayerID()) == Networking::CLIENT_TYPE_HUMAN_MODERATOR);
        if (!have_empire && !moderator)
            enable = false;
    }

    m_turn_update->Disable(!enable);
    m_side_panel->EnableOrderIssuing(enable);
    m_production_wnd->EnableOrderIssuing(enable);
    m_research_wnd->EnableOrderIssuing(enable);
    m_design_wnd->EnableOrderIssuing(enable);
    FleetUIManager::GetFleetUIManager().EnableOrderIssuing(enable);
}

void MapWnd::ProductionUpdate() {
    SidePanel::Update();
    MidTurnUpdate();
}

void MapWnd::InitTurn() {
    int turn_number = CurrentTurn();
    Logger().debugStream() << "Initializing turn " << turn_number;
    ScopedTimer init_timer("MapWnd::InitTurn", true);

    // reset hotkey signals
    DisconnectKeyboardAcceleratorSignals();
    ConnectKeyboardAcceleratorSignals();

    Universe& universe = GetUniverse();
    const ObjectMap& objects = Objects();

    universe.InitializeSystemGraph(HumanClientApp::GetApp()->EmpireID());

    //// DEBUG
    //Logger().debugStream() << Empires().Dump();
    //std::cout << "MapWnd::InitTurn() m_selected_fleet_ids: " << std::endl;
    //for (std::set<int>::const_iterator it = m_selected_fleet_ids.begin(); it != m_selected_fleet_ids.end(); ++it) {
    //    const UniverseObject* obj = const_universe.Object(*it);
    //    if (obj)
    //        std::cout << "    " << obj->Name() << "(" << *it << ")" << std::endl;
    //    else
    //        std::cout << "    [missing object] (" << *it << ")" << std::endl;
    //}
    // DEBUG

    //Logger().debugStream() << "Visible UniverseObjects: ";
    //objects.Dump();

    //// DEBUG
    //for (EmpireManager::const_iterator empire_it = Empires().begin(); empire_it != Empires().end(); ++empire_it)
    //    Logger().debugStream() << "MapWnd::InitTurn: empire id: " << empire_it->first << " named: " << empire_it->second->Name();
    //// END DEBUG

    Empire* this_client_empire = Empires().Lookup(HumanClientApp::GetApp()->EmpireID());


    // update effect accounting and meter estimates
    universe.InitMeterEstimatesAndDiscrepancies();

    // if we've just loaded the game there may be some unexecuted orders, we
    // should reapply them now, so they are reflected in the UI, but do not
    // influence current meters or their discrepancies for this turn
    HumanClientApp::GetApp()->Orders().ApplyOrders();

    // redo meter estimates with unowned planets marked as owned by player, so accurate predictions of planet
    // population is available for currently uncolonized planets
    UpdateMeterEstimates();


    GetUniverse().ApplyAppearanceEffects();


    boost::timer timer;

    const std::set<int>& this_client_known_destroyed_objects = universe.EmpireKnownDestroyedObjectIDs(HumanClientApp::GetApp()->EmpireID());

    //// get ids of not-destroyed systems known to this empire.
    //std::set<int> this_client_known_systems;
    //std::vector<int> all_system_ids = Objects().FindObjectIDs<System>();
    //for (std::vector<int>::const_iterator it = all_system_ids.begin(); it != all_system_ids.end(); ++it)
    //    if (this_client_known_destroyed_objects.find(*it) == this_client_known_destroyed_objects.end())
    //        this_client_known_systems.insert(*it);

    //// get ids of all not-destroyed objects known to this empire.
    //std::set<int> this_client_known_objects;
    //std::vector<int> all_object_ids = Objects().FindObjectIDs();
    //for (std::vector<int>::const_iterator it = all_object_ids.begin(); it != all_object_ids.end(); ++it)
    //    if (this_client_known_destroyed_objects.find(*it) == this_client_known_destroyed_objects.end())
    //        this_client_known_objects.insert(*it);

    Logger().debugStream() << "MapWnd::InitTurn getting known starlanes and visible systems and visible objects time: " << (timer.elapsed() * 1000.0);


    // set up system icons, starlanes, galaxy gas rendering
    InitTurnRendering();


    // connect system fleet add and remove signals
    std::vector<const System*> systems = objects.FindObjects<System>();
    for (std::vector<const System*>::const_iterator it = systems.begin(); it != systems.end(); ++it) {
        const System *system = *it;
        m_system_fleet_insert_remove_signals[system->ID()].push_back(GG::Connect(system->FleetInsertedSignal,   &MapWnd::FleetAddedOrRemoved,   this));
        m_system_fleet_insert_remove_signals[system->ID()].push_back(GG::Connect(system->FleetRemovedSignal,    &MapWnd::FleetAddedOrRemoved,   this));
    }

    RefreshFleetSignals();


    // set turn button to current turn
    m_turn_update->SetText(boost::io::str(FlexibleFormat(UserString("MAP_BTN_TURN_UPDATE")) %
                                          boost::lexical_cast<std::string>(turn_number)));
    MoveChildUp(m_turn_update);


    // are there any sitreps to show?
    if (m_sitrep_panel->NumVisibleSitrepsThisTurn() > 0) {
        m_sitrep_panel->ShowSitRepsForTurn(CurrentTurn());
        ShowSitRep();
    }

    if (m_object_list_wnd->Visible())
        m_object_list_wnd->Refresh();


    EnableAlphaNumAccels();


    // show or hide system names, depending on zoom.  replicates code in MapWnd::Zoom
    if (ZoomFactor() * ClientUI::Pts() < MIN_SYSTEM_NAME_SIZE)
        HideSystemNames();
    else
        ShowSystemNames();


    // empire is recreated each turn based on turn update from server, so connections of signals emitted from
    // the empire must be remade each turn (unlike connections to signals from the sidepanel)
    if (this_client_empire) {
        GG::Connect(this_client_empire->GetResourcePool(RE_TRADE)->ChangedSignal,           &MapWnd::RefreshTradeResourceIndicator,     this, 0);
        GG::Connect(this_client_empire->GetResourcePool(RE_RESEARCH)->ChangedSignal,        &MapWnd::RefreshResearchResourceIndicator,  this, 0);
        GG::Connect(this_client_empire->GetResourcePool(RE_INDUSTRY)->ChangedSignal,        &MapWnd::RefreshIndustryResourceIndicator,  this, 0);
        GG::Connect(this_client_empire->GetPopulationPool().ChangedSignal,                  &MapWnd::RefreshPopulationIndicator,        this, 1);
        GG::Connect(this_client_empire->GetProductionQueue().ProductionQueueChangedSignal,  &MapWnd::ProductionUpdate,                  this, 1);
        GG::Connect(this_client_empire->GetResearchQueue().ResearchQueueChangedSignal,      &MapWnd::RefreshResearchResourceIndicator,  this);
        GG::Connect(this_client_empire->GetProductionQueue().ProductionQueueChangedSignal,  &MapWnd::RefreshIndustryResourceIndicator,  this);
    }

    m_toolbar->Show();
    m_FPS->Show();
    m_scale_line->Show();
    RefreshSliders();


    timer.restart();
    for (EmpireManager::iterator it = Empires().begin(); it != Empires().end(); ++it)
        it->second->UpdateResourcePools();


    Logger().debugStream() << "MapWnd::InitTurn getting known starlanes and visible systems time: " << (timer.elapsed() * 1000.0);


    timer.restart();
    m_research_wnd->Refresh();
    Logger().debugStream() << "MapWnd::InitTurn research wnd refresh time: " << (timer.elapsed() * 1000.0);


    timer.restart();
    SidePanel::Refresh();       // recreate contents of all SidePanels.  ensures previous turn's objects and signals are disposed of
    Logger().debugStream() << "MapWnd::InitTurn sidepanel refresh time: " << (timer.elapsed() * 1000.0);


    timer.restart();
    m_production_wnd->Refresh();
    Logger().debugStream() << "MapWnd::InitTurn m_production_wnd refresh time: " << (timer.elapsed() * 1000.0);


    if (turn_number == 1 && this_client_empire) {
        // start first turn with player's system selected
        if (const UniverseObject* obj = objects.Object(this_client_empire->CapitalID())) {
            SelectSystem(obj->SystemID());
            CenterOnMapCoord(obj->X(), obj->Y());
        }

        // default the tech tree to be centred on something interesting
        m_research_wnd->Reset();
    } else if (turn_number == 1 && !this_client_empire) {
        CenterOnMapCoord(0.0, 0.0);
    }

    RefreshIndustryResourceIndicator();
    RefreshResearchResourceIndicator();
    RefreshTradeResourceIndicator();
    RefreshFleetResourceIndicator();
    RefreshPopulationIndicator();
    RefreshDetectionIndicator();

    FleetUIManager::GetFleetUIManager().RefreshAll();

    DispatchFleetsExploring();
}

void MapWnd::MidTurnUpdate() {
    Logger().debugStream() << "MapWnd::MidTurnUpdate";
    ScopedTimer timer("MapWnd::MidTurnUpdate", true);

    GetUniverse().InitializeSystemGraph(HumanClientApp::GetApp()->EmpireID());

    // set up system icons, starlanes, galaxy gas rendering
    InitTurnRendering();

    // show or hide system names, depending on zoom.  replicates code in MapWnd::Zoom
    if (ZoomFactor() * ClientUI::Pts() < MIN_SYSTEM_NAME_SIZE)
        HideSystemNames();
    else
        ShowSystemNames();
}

void MapWnd::InitTurnRendering() {
    Logger().debugStream() << "MapWnd::InitTurnRendering";
    ScopedTimer timer("MapWnd::InitTurnRendering", true);

    if (!m_scanline_shader && GetOptionsDB().Get<bool>("UI.system-fog-of-war")) {
        boost::filesystem::path shader_path = GetRootDataDir() / "default" / "shaders" / "scanlines.frag";
        std::string shader_text;
        ReadFile(shader_path, shader_text);
        if (!shader_text.empty()) {
            m_scanline_shader = boost::shared_ptr<ShaderProgram>(
                ShaderProgram::shaderProgramFactory("", shader_text));
        }
    }

    // adjust size of map window for universe and application size
    Resize(GG::Pt(static_cast<GG::X>(GetUniverse().UniverseWidth() * ZOOM_MAX + AppWidth() * 1.5),
                  static_cast<GG::Y>(GetUniverse().UniverseWidth() * ZOOM_MAX + AppHeight() * 1.5)));


    // set up backgrounds on first turn.  if m_backgrounds already contains textures, does nothing
    InitBackgrounds(m_backgrounds, m_bg_scroll_rate);


    // remove any existing fleet movement lines or projected movement lines.  this gets cleared
    // here instead of with the movement line stuff because that would clear some movement lines
    // that come from the SystemIcons
    m_fleet_lines.clear();
    ClearProjectedFleetMovementLines();


    const std::set<int>& this_client_known_destroyed_objects = GetUniverse().EmpireKnownDestroyedObjectIDs(HumanClientApp::GetApp()->EmpireID());
    const ObjectMap& objects = Objects();

    // remove old system icons
    for (std::map<int, SystemIcon*>::iterator it = m_system_icons.begin(); it != m_system_icons.end(); ++it)
        DeleteChild(it->second);
    m_system_icons.clear();

    // create system icons
    std::vector<const System*> systems = objects.FindObjects<System>();
    for (std::vector<const System*>::const_iterator sys_it = systems.begin(); sys_it != systems.end(); ++sys_it) {
        const System* sys = *sys_it;
        int sys_id = sys->ID();

        // skip known destroyed objects
        if (this_client_known_destroyed_objects.find(sys_id) != this_client_known_destroyed_objects.end())
            continue;

        // create new system icon
        SystemIcon* icon = new SystemIcon(GG::X0, GG::Y0, GG::X(10), sys_id);
        m_system_icons[sys_id] = icon;
        icon->InstallEventFilter(this);
        if (SidePanel::SystemID() == sys_id)
            icon->SetSelected(true);
        AttachChild(icon);

        // connect UI response signals.  TODO: Make these configurable in GUI?
        GG::Connect(icon->LeftClickedSignal,        &MapWnd::SystemLeftClicked,         this);
        GG::Connect(icon->RightClickedSignal,       &MapWnd::SystemRightClicked,        this);
        GG::Connect(icon->LeftDoubleClickedSignal,  &MapWnd::SystemDoubleClicked,       this);
        GG::Connect(icon->MouseEnteringSignal,      &MapWnd::MouseEnteringSystem,       this);
        GG::Connect(icon->MouseLeavingSignal,       &MapWnd::MouseLeavingSystem,        this);
    }

    // create buffers for system icon and galaxy gas rendering, and starlane rendering
    InitSystemRenderingBuffers();
    InitStarlaneRenderingBuffers();

    // position system icons
    DoSystemIconsLayout();


    // remove old field icons
    for (std::map<int, FieldIcon*>::iterator it = m_field_icons.begin(); it != m_field_icons.end(); ++it)
        DeleteChild(it->second);
    m_field_icons.clear();

    // create field icons
    std::vector<const Field*> fields = objects.FindObjects<Field>();
    for (std::vector<const Field*>::const_iterator fld_it = fields.begin(); fld_it != fields.end(); ++fld_it) {
        const Field* field = *fld_it;
        int fld_id = field->ID();

        // skip known destroyed objects
        if (this_client_known_destroyed_objects.find(fld_id) != this_client_known_destroyed_objects.end())
            continue;
        if (field->GetVisibility(HumanClientApp::GetApp()->EmpireID()) <= VIS_NO_VISIBILITY)
            continue;

        // create new system icon
        FieldIcon* icon = new FieldIcon(GG::X0, GG::Y0, fld_id);
        m_field_icons[fld_id] = icon;
        icon->InstallEventFilter(this);
        //if (SidePanel::SystemID() == systems[i]->ID())
        //    icon->SetSelected(true);
        AttachChild(icon);

        // connect UI response signals.  TODO: Make these configurable in GUI?
        //GG::Connect(icon->LeftClickedSignal,        &MapWnd::SystemLeftClicked,         this);
        //GG::Connect(icon->RightClickedSignal,       &MapWnd::SystemRightClicked,        this);
        //GG::Connect(icon->LeftDoubleClickedSignal,  &MapWnd::SystemDoubleClicked,       this);
        //GG::Connect(icon->MouseEnteringSignal,      &MapWnd::MouseEnteringSystem,       this);
        //GG::Connect(icon->MouseLeavingSignal,       &MapWnd::MouseLeavingSystem,        this);
    }

    // position field icons
    DoFieldIconsLayout();


    // create fleet buttons and move lines.  needs to be after InitStarlaneRenderingBuffers so that m_starlane_endpoints is populated
    RefreshFleetButtons();


    // move field icons to bottom of child stack so that other icons can be moused over with a field
    for (std::map<int, FieldIcon*>::iterator it = m_field_icons.begin(); it != m_field_icons.end(); ++it)
        MoveChildDown(it->second);
}

void MapWnd::InitSystemRenderingBuffers() {
    Logger().debugStream() << "MapWnd::InitSystemRenderingBuffers";
    ScopedTimer timer("MapWnd::InitSystemRenderingBuffers", true);

    // clear out all the old buffers
    ClearSystemRenderingBuffers();

    // Generate texture coordinates to be used for subsequent vertex buffer creation.
    // Note these coordinates assume the texture is twice as large as it should
    // be.  This allows us to use one set of texture coords for everything, even
    // though the star-halo textures must be rendered at sizes as much as twice
    // as large as the star-disc textures.
    for (std::size_t i = 0; i < m_system_icons.size(); ++i) {
        m_star_texture_coords.store(1.5,-0.5);
        m_star_texture_coords.store(-0.5,-0.5);
        m_star_texture_coords.store(-0.5,1.5);
        m_star_texture_coords.store(1.5,1.5);
    }


    for (std::map<int, SystemIcon*>::const_iterator it = m_system_icons.begin(); it != m_system_icons.end(); ++it) {
        const SystemIcon* icon = it->second;
        int system_id = it->first;
        const System* system = GetSystem(system_id);
        if (!system) {
            Logger().errorStream() << "MapWnd::InitSystemRenderingBuffers couldn't get system with id " << system_id;
            continue;
        }

        // Add disc and halo textures for system icon
        // See note above texture coords for why we're making coordinate sets that are 2x too big.
        double icon_size = ClientUI::SystemIconSize();
        float icon_ul_x = static_cast<float>(system->X() - icon_size);
        float icon_ul_y = static_cast<float>(system->Y() - icon_size);
        float icon_lr_x = static_cast<float>(system->X() + icon_size);
        float icon_lr_y = static_cast<float>(system->Y() + icon_size);

        if (icon->DiscTexture()) {
            glBindTexture(GL_TEXTURE_2D, icon->DiscTexture()->OpenGLId());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            GL2DVertexBuffer& core_vertices = m_star_core_quad_vertices[icon->DiscTexture()];
            core_vertices.store(icon_lr_x,icon_ul_y);
            core_vertices.store(icon_ul_x,icon_ul_y);
            core_vertices.store(icon_ul_x,icon_lr_y);
            core_vertices.store(icon_lr_x,icon_lr_y);
        }

        if (icon->HaloTexture()) {
            glBindTexture(GL_TEXTURE_2D, icon->HaloTexture()->OpenGLId());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            GL2DVertexBuffer& halo_vertices = m_star_halo_quad_vertices[icon->HaloTexture()];
            halo_vertices.store(icon_lr_x,icon_ul_y);
            halo_vertices.store(icon_ul_x,icon_ul_y);
            halo_vertices.store(icon_ul_x,icon_lr_y);
            halo_vertices.store(icon_lr_x,icon_lr_y);
        }


        // add (rotated) gaseous substance around system
        if (boost::shared_ptr<GG::Texture> gaseous_texture = ClientUI::GetClientUI()->GetModuloTexture(ClientUI::ArtDir() / "galaxy_decoration", "gaseous", system_id)) {
            glBindTexture(GL_TEXTURE_2D, gaseous_texture->OpenGLId());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            const double GAS_SIZE = ClientUI::SystemIconSize() * 12.0;
            const double ROTATION = system_id * 27.0; // arbitrary rotation in radians ("27.0" is just a number that produces pleasing results)
            const double COS_THETA = std::cos(ROTATION);
            const double SIN_THETA = std::sin(ROTATION);

            // Components of corner points of a quad
            const double X1 =  1.0, Y1 =  1.0;  // upper right corner (X1, Y1)
            const double X2 = -1.0, Y2 =  1.0;  // upper left corner  (X2, Y2)
            const double X3 = -1.0, Y3 = -1.0;  // lower left corner  (X3, Y3)
            const double X4 =  1.0, Y4 = -1.0;  // lower right corner (X4, Y4)

            // Calculate rotated corner point components after CCW ROTATION radians around origin.
            const double X1r =  COS_THETA*X1 + SIN_THETA*Y1;
            const double Y1r = -SIN_THETA*X1 + COS_THETA*Y1;
            const double X2r =  COS_THETA*X2 + SIN_THETA*Y2;
            const double Y2r = -SIN_THETA*X2 + COS_THETA*Y2;
            const double X3r =  COS_THETA*X3 + SIN_THETA*Y3;
            const double Y3r = -SIN_THETA*X3 + COS_THETA*Y3;
            const double X4r =  COS_THETA*X4 + SIN_THETA*Y4;
            const double Y4r = -SIN_THETA*X4 + COS_THETA*Y4;

            // Multiply all coords by GAS_SIZE to get relative scaled rotated quad corner components
            // See note above texture coords for why we're making coordinate sets that are 2x too big.

            // add to system position to get translated scaled rotated quad corner
            const float GAS_X1 = static_cast<float>(system->X() + (X1r * GAS_SIZE));
            const float GAS_Y1 = static_cast<float>(system->Y() + (Y1r * GAS_SIZE));
            const float GAS_X2 = static_cast<float>(system->X() + (X2r * GAS_SIZE));
            const float GAS_Y2 = static_cast<float>(system->Y() + (Y2r * GAS_SIZE));
            const float GAS_X3 = static_cast<float>(system->X() + (X3r * GAS_SIZE));
            const float GAS_Y3 = static_cast<float>(system->Y() + (Y3r * GAS_SIZE));
            const float GAS_X4 = static_cast<float>(system->X() + (X4r * GAS_SIZE));
            const float GAS_Y4 = static_cast<float>(system->Y() + (Y4r * GAS_SIZE));

            GL2DVertexBuffer& gas_vertices = m_galaxy_gas_quad_vertices[gaseous_texture];

            gas_vertices.store(GAS_X1,GAS_Y1); // rotated upper right
            gas_vertices.store(GAS_X2,GAS_Y2); // rotated upper left
            gas_vertices.store(GAS_X3,GAS_Y3); // rotated lower left
            gas_vertices.store(GAS_X4,GAS_Y4); // rotated lower right
        }
    }

    // create new buffers

    // star cores
    for (std::map<boost::shared_ptr<GG::Texture>, GL2DVertexBuffer>::const_iterator it =
             m_star_core_quad_vertices.begin(); it != m_star_core_quad_vertices.end(); ++it)
    { m_star_core_quad_vertices[it->first].createServerBuffer(); }

    // star halos
    for (std::map<boost::shared_ptr<GG::Texture>, GL2DVertexBuffer>::const_iterator it = m_star_halo_quad_vertices.begin();
         it != m_star_halo_quad_vertices.end(); ++it)
    { m_star_halo_quad_vertices[it->first].createServerBuffer(); }

    // galaxy gas
    for (std::map<boost::shared_ptr<GG::Texture>, GL2DVertexBuffer>::const_iterator it = m_galaxy_gas_quad_vertices.begin();
         it != m_galaxy_gas_quad_vertices.end(); ++it)
    { m_galaxy_gas_quad_vertices[it->first].createServerBuffer(); }

    // fill buffers with star textures
    m_star_texture_coords.createServerBuffer();
}

void MapWnd::ClearSystemRenderingBuffers() {
    m_star_core_quad_vertices.clear();
    m_star_halo_quad_vertices.clear();
    m_galaxy_gas_quad_vertices.clear();
    m_star_texture_coords.clear();
}

std::vector<int> MapWnd::GetLeastJumps(int startSys, int endSys, const std::set<int>& resGroup,
                                       const std::set<std::pair<int, int> >& supplylanes,
                                       const ObjectMap& objMap)
{
    //std::map<int,bool> sysChecked;
    std::map<int,int> ancestor;
    std::deque<int> tryNext;
    std::vector<int> path;
    path.push_back(startSys);
    if (startSys==endSys)
        return path;

    for (std::set<int>::const_iterator sysIt = resGroup.begin(); sysIt!=resGroup.end(); sysIt++)
        ancestor[*sysIt] = -1;
    ancestor[startSys] = startSys;
    tryNext.push_back(startSys);
    while (!tryNext.empty() ) {
        int sysID = tryNext.front();
        //Logger().debugStream() << "MapWnd::InitStarlaneRenderingBuffers  ==> GetLeastJumps, checking system "<< sysID;
        for (System::const_lane_iterator laneIt= universe_object_cast<System*>(objMap.Object(sysID))->begin_lanes(); 
             laneIt!= universe_object_cast<System*>(objMap.Object(sysID))->end_lanes(); laneIt++)
        {
            int newSys = laneIt->first;
            //Logger().debugStream() << "MapWnd::InitStarlaneRenderingBuffers         ==> GetLeastJumps, considering lane/WH to system "<< newSys;
            if (!laneIt->second && ( ancestor[newSys] == -1 )) { //is a starlane, and not yet visited newSys //TODO: should allow wormholes here?
                ancestor[newSys] = sysID;
                if (newSys==endSys) {
                    int iSys = newSys;
                    while ((ancestor[iSys] !=-1)&&( ancestor[iSys] != iSys )) {
                        path.push_back(iSys);
                        iSys = ancestor[iSys];
                    }
                    return path;
                }
                tryNext.push_back(newSys);
            }
        }
        tryNext.pop_front();
    }
    //found no path, return empty vec
    std::vector<int> nullPath;
    return nullPath;
}

void MapWnd::InitStarlaneRenderingBuffers() {
    Logger().debugStream() << "MapWnd::InitStarlaneRenderingBuffers";
    ScopedTimer timer("MapWnd::InitStarlaneRenderingBuffers", true);

    // clear old buffers
    ClearStarlaneRenderingBuffers();
    m_resourceCenters.clear();

    // temp storage
    std::set<std::pair<int, int> >  rendered_half_starlanes;    // stored as unaltered pairs, so that a each direction of traversal can be shown separately

    const GG::Clr UNOWNED_LANE_COLOUR = GetOptionsDB().Get<StreamableColor>("UI.unowned-starlane-colour").ToClr();

    int empire_id = HumanClientApp::GetApp()->EmpireID();

    const std::set<int>& this_client_known_destroyed_objects = GetUniverse().EmpireKnownDestroyedObjectIDs(HumanClientApp::GetApp()->EmpireID());
    const Empire* this_client_empire = Empires().Lookup(empire_id);
    std::set<int> underAllocResSys;

    std::map<std::set<int>, std::set<int> > resPoolSystems;//map keyed by ResourcePool (set of objects) to the corresponding set of SysIDs
    std::map<std::set<int>, std::set<int> > resPoolToGroupMap;//map keyed by ResourcePool (set of objects) to the corresponding ResourceGroup (which may be larger than the above resPoolSystem set)
    std::map<std::set<int>, std::set<int> > resGroupCores;// map keyed by ResourcePool to the set of systems considered the core of the corresponding ResGroup
    std::set<int> resGrpCoreMembers;
    std::map<int, std::set<int> > memberToPool;
    std::set<int> underAllocResGrpCoreMembers;

    if (this_client_empire) {
        const std::set<std::set<int> >& resGroups = this_client_empire->ResourceSupplyGroups();
        const ProductionQueue& queue = this_client_empire->GetProductionQueue();
        std::map<std::set<int>, double> allocatedPP(queue.AllocatedPP());
        std::map<std::set<int>, double> availablePP(this_client_empire->GetResourcePool(RE_INDUSTRY)->Available());

        for (std::map<std::set<int>, double>::const_iterator it = availablePP.begin(); it != availablePP.end(); ++it) {
            double group_pp = it->second;
            if (group_pp < 1e-4)
                continue;

            std::string thisPool = "( ";
            for (std::set<int>::const_iterator objIt = it->first.begin(); objIt != it->first.end(); ++objIt) {
                int object_id = *objIt;
                thisPool += boost::lexical_cast<std::string>(object_id) +", ";

                const Planet* planet = GetPlanet(object_id);
                if (!planet)
                    continue;

                //Logger().debugStream() << "Empire " << empire_id << "; Planet (" << object_id << ") is named " << planet->Name();

                int system_id = planet->SystemID();
                resPoolSystems[it->first].insert(system_id);
                m_resourceCenters.insert(system_id);
                if (group_pp > allocatedPP[it->first] + 1e-4)
                    underAllocResSys.insert(system_id);
            }
            thisPool += ")";
            //Logger().debugStream() << "Empire " << empire_id << "; ResourcePool[RE_INDUSTRY] resourceGroup (" << thisPool << ") has (" << it->second << " PP available";
            //Logger().debugStream() << "Empire " << empire_id << "; ResourcePool[RE_INDUSTRY] resourceGroup (" << thisPool << ") has (" << allocatedPP[it->first] << " PP allocated";
        }
        //Logger().debugStream() << "           MapWnd::InitStarlaneRenderingBuffers  finished empire Info collection Round 1";
        for (std::map<std::set<int>, std::set<int> >::iterator resPoolSysIt = resPoolSystems.begin(); resPoolSysIt != resPoolSystems.end(); resPoolSysIt++){
            for (std::set<std::set<int> >::const_iterator rgIt = resGroups.begin(); rgIt != resGroups.end(); ++rgIt) {
                bool placedPool = false;
                for (std::set<int>::iterator sysIt=resPoolSysIt->second.begin(); sysIt!=resPoolSysIt->second.end(); sysIt++) {
                    if (rgIt->find(*sysIt) != rgIt->end()) {
                        resPoolToGroupMap[resPoolSysIt->first] = *rgIt;
                        placedPool = true;
                        break;
                    }
                }
                if (placedPool)
                    break;
            }
        }//TODO: could add double checking that pool was successfully linked to a group, but *shouldn't* be necessary I think
        //Logger().debugStream() << "           MapWnd::InitStarlaneRenderingBuffers  finished empire Info collection Round 2";

        std::set<std::pair<int, int> > resource_supply_lanes (this_client_empire->SupplyStarlaneTraversals()) ;
        for (std::map<std::set<int>, std::set<int> >::iterator resPoolSysIt = resPoolSystems.begin(); resPoolSysIt != resPoolSystems.end(); resPoolSysIt++){
            std::string thisPoolCtrs = "( ";
            for (std::set<int>::iterator startSys=resPoolSysIt->second.begin(); startSys != resPoolSysIt->second.end(); startSys++) 
                thisPoolCtrs += boost::lexical_cast<std::string>(*startSys) +", ";
            thisPoolCtrs += ")";
            //Logger().debugStream() << "           MapWnd::InitStarlaneRenderingBuffers  getting resGrpCore for ResPool Ctrs  (" << thisPoolCtrs << ")";

            resGroupCores[ resPoolSysIt->first ].insert(*(resPoolSysIt->second.begin())); // if pool only has one sys, ensure it is added to core
            resGrpCoreMembers.insert(*(resPoolSysIt->second.begin()));
            std::set<int>::iterator lastSys = resPoolSysIt->second.end();
            lastSys--;
            for (std::set<int>::iterator startSys=resPoolSysIt->second.begin(); startSys != lastSys; startSys++) {//ok since resPoolSysIt->second cannot be empty
                std::set<int>::iterator nextSys = startSys;
                for (std::set<int>::iterator endSys=++nextSys; endSys!=resPoolSysIt->second.end(); endSys++) {
                    //Logger().debugStream() << "                 MapWnd::InitStarlaneRenderingBuffers getting path from sys "<< (*startSys) << " to "<< (*endSys) ;
                    std::vector<int> path = GetLeastJumps(*startSys, *endSys, resPoolToGroupMap[resPoolSysIt->first], resource_supply_lanes, Objects());
                    //Logger().debugStream() << "                 MapWnd::InitStarlaneRenderingBuffers got path, length: "<< path.size();
                    for (std::vector<int>::iterator pathSys = path.begin(); pathSys!= path.end(); pathSys++) {
                        resGroupCores[ resPoolSysIt->first ].insert(*pathSys);
                        resGrpCoreMembers.insert(*pathSys);
                        memberToPool[*pathSys] = resPoolSysIt->first;
                    }
                }
            }
        }
        //Logger().debugStream() << "           MapWnd::InitStarlaneRenderingBuffers  finished empire Info collection Round 3";

        for (std::map<std::set<int>, std::set<int> >::iterator resPoolSysIt = resPoolSystems.begin(); resPoolSysIt != resPoolSystems.end(); resPoolSysIt++)
            if (underAllocResSys.find( *(resPoolSysIt->second.begin())  ) != underAllocResSys.end())
                underAllocResGrpCoreMembers.insert( resGroupCores[ resPoolSysIt->first ].begin(), resGroupCores[ resPoolSysIt->first ].end() );
    }
    //Logger().debugStream() << "           MapWnd::InitStarlaneRenderingBuffers  finished empire Info collection";

    // calculate in-universe apparent starlane endpoints and create buffers for starlane rendering
    m_starlane_endpoints.clear();

    for (std::map<int, SystemIcon*>::const_iterator it = m_system_icons.begin(); it != m_system_icons.end(); ++it) {
        int system_id = it->first;

        // skip systems that don't actually exist
        if (this_client_known_destroyed_objects.find(system_id) != this_client_known_destroyed_objects.end())
            continue;

        const System* start_system = GetSystem(system_id);
        if (!start_system) {
            Logger().errorStream() << "MapWnd::InitStarlaneRenderingBuffers couldn't get system with id " << system_id;
            continue;
        }

        // add system's starlanes
        for (System::const_lane_iterator lane_it = start_system->begin_lanes(); lane_it != start_system->end_lanes(); ++lane_it) {
            bool lane_is_wormhole = lane_it->second;
            if (lane_is_wormhole) continue; // at present, not rendering wormholes

            int lane_end_sys_id = lane_it->first;

            // skip lanes to systems that don't actually exist
            if (this_client_known_destroyed_objects.find(lane_end_sys_id) != this_client_known_destroyed_objects.end())
                continue;

            const System* dest_system = GetSystem(lane_it->first);
            if (!dest_system)
                continue;
            //std::cout << "colouring lanes between " << start_system->Name() << " and " << dest_system->Name() << std::endl;


            // check that this lane isn't already in map / being rendered.
            std::pair<int, int> lane = UnorderedIntPair(start_system->ID(), dest_system->ID());     // get "unordered pair" indexing lane

            if (m_starlane_endpoints.find(lane) == m_starlane_endpoints.end()) {
                //std::cout << "adding full length lane" << std::endl;

                // get and store universe position endpoints for this starlane.  make sure to store in the same order
                // as the system ids in the lane id pair
                if (start_system->ID() == lane.first)
                    m_starlane_endpoints[lane] = StarlaneEndPointsFromSystemPositions(start_system->X(), start_system->Y(), dest_system->X(), dest_system->Y());
                else
                    m_starlane_endpoints[lane] = StarlaneEndPointsFromSystemPositions(dest_system->X(), dest_system->Y(), start_system->X(), start_system->Y());


                // add vertices for this full-length starlane
                m_starlane_vertices.store(static_cast<float>(m_starlane_endpoints[lane].X1),
                                          static_cast<float>(m_starlane_endpoints[lane].Y1));
                m_starlane_vertices.store(static_cast<float>(m_starlane_endpoints[lane].X2),
                                          static_cast<float>(m_starlane_endpoints[lane].Y2));

                // determine colour(s) for lane based on which empire(s) can transfer resources along the lane.
                // todo: multiple rendered lanes (one for each empire) when multiple empires use the same lane.
                GG::Clr lane_colour = UNOWNED_LANE_COLOUR;    // default colour if no empires transfer resources along starlane
                for (EmpireManager::iterator empire_it = Empires().begin(); empire_it != Empires().end(); ++empire_it) {
                    Empire* empire = empire_it->second;
                    const std::set<std::pair<int, int> >& resource_supply_lanes = empire->SupplyStarlaneTraversals();

                    //std::cout << "resource supply starlane traversals for empire " << empire->Name() << ": " << resource_supply_lanes.size() << std::endl;

                    std::pair<int, int> lane_forward = std::make_pair(start_system->ID(), dest_system->ID());
                    std::pair<int, int> lane_backward = std::make_pair(dest_system->ID(), start_system->ID());

                    // see if this lane exists in this empire's supply propegation lanes set.  either direction accepted.
                    if (resource_supply_lanes.find(lane_forward) != resource_supply_lanes.end() || resource_supply_lanes.find(lane_backward) != resource_supply_lanes.end()) {
                        lane_colour = empire->Color();
                        //std::cout << "selected colour of empire " << empire->Name() << " for this full lane" << std::endl;
                        break;
                    }
                }

                // vertex colours for starlane
                m_starlane_colors.store(lane_colour.r,
                                        lane_colour.g,
                                        lane_colour.b,
                                        lane_colour.a);
                m_starlane_colors.store(lane_colour.r,
                                        lane_colour.g,
                                        lane_colour.b,
                                        lane_colour.a);

                //Logger().debugStream() << "adding full lane from " << start_system->Name() << " to " << dest_system->Name();
            }


            // render half-starlane from the current start_system to the current dest_system?

            // check that this lane isn't already going to be rendered.  skip it if it is.
            if (rendered_half_starlanes.find(std::make_pair(start_system->ID(), dest_system->ID())) == rendered_half_starlanes.end()) {
                // NOTE: this will never find a preexisting half lane   NOTE LATER: I probably wrote that comment, but have no idea what it means...
                //std::cout << "half lane not found... considering possible half lanes to add" << std::endl;

                // scan through possible empires to have a half-lane here and add a half-lane if one is found
                std::pair<int, int> lane_forward = std::make_pair(start_system->ID(), dest_system->ID());
                //std::pair<int, int> lane_backward = std::make_pair(dest_system->ID(), start_system->ID());
                LaneEndpoints lane_endpoints = StarlaneEndPointsFromSystemPositions(start_system->X(), start_system->Y(), dest_system->X(), dest_system->Y());
                GG::Clr lane_colour;
                if ( (this_client_empire) &&(resGrpCoreMembers.find(start_system->ID()) != resGrpCoreMembers.end()))  {//start system is a res Grp core member for this_client_empire -- highlight
                    lane_colour = this_client_empire->Color();
                    float indicatorExtent = 0.5f;
                    if (underAllocResGrpCoreMembers.find(start_system->ID()) != underAllocResGrpCoreMembers.end() ) {
                        GG::Clr eclr= this_client_empire->Color();
                        lane_colour = GG::DarkColor( GG::Clr(255-eclr.r, 255-eclr.g, 255-eclr.b, eclr.a));
                    }
                    /*if ((this_client_empire->SupplyOstructedStarlaneTraversals().find(lane_forward) != this_client_empire->SupplyOstructedStarlaneTraversals().end()) ||
                        (this_client_empire->SupplyOstructedStarlaneTraversals().find(lane_backward) != this_client_empire->SupplyOstructedStarlaneTraversals().end()) ||
                        !( (this_client_empire->SupplyStarlaneTraversals().find(lane_forward) != this_client_empire->SupplyStarlaneTraversals().end()) ||
                        (this_client_empire->SupplyStarlaneTraversals().find(lane_backward) != this_client_empire->SupplyStarlaneTraversals().end())   )  ) */
                    if (resGroupCores[ memberToPool[start_system->ID()]] != resGroupCores[ memberToPool[dest_system->ID()]])
                        indicatorExtent = 0.2f;
                    m_RC_starlane_vertices.store(lane_endpoints.X1,
                                                 lane_endpoints.Y1);
                    m_RC_starlane_vertices.store((lane_endpoints.X2 - lane_endpoints.X1) * indicatorExtent + lane_endpoints.X1,   // part way along starlane
                                                 (lane_endpoints.Y2 - lane_endpoints.Y1) * indicatorExtent + lane_endpoints.Y1);

                    m_RC_starlane_colors.store(lane_colour.r,
                                               lane_colour.g,
                                               lane_colour.b,
                                               lane_colour.a);
                    m_RC_starlane_colors.store(lane_colour.r,
                                               lane_colour.g,
                                               lane_colour.b,
                                               lane_colour.a);
                }

                for (EmpireManager::iterator empire_it = Empires().begin(); empire_it != Empires().end(); ++empire_it) {
                    Empire* empire = empire_it->second;
                    const std::set<std::pair<int, int> >& resource_obstructed_supply_lanes = empire->SupplyOstructedStarlaneTraversals();

                    // see if this lane exists in this empire's supply propegation lanes set.  either direction accepted.
                    if (resource_obstructed_supply_lanes.find(lane_forward) != resource_obstructed_supply_lanes.end()) {
                        // found an empire that has a half lane here, so add it.
                        rendered_half_starlanes.insert(std::make_pair(start_system->ID(), dest_system->ID()));  // inserted as ordered pair, so both directions can have different half-lanes

                        m_starlane_vertices.store(lane_endpoints.X1,
                                                  lane_endpoints.Y1);
                        m_starlane_vertices.store((lane_endpoints.X1 + lane_endpoints.X2) * 0.5f,   // half way along starlane
                                                  (lane_endpoints.Y1 + lane_endpoints.Y2) * 0.5f);

                        lane_colour = empire->Color();
                        m_starlane_colors.store(lane_colour.r,
                                                lane_colour.g,
                                                lane_colour.b,
                                                lane_colour.a);
                        m_starlane_colors.store(lane_colour.r,
                                                lane_colour.g,
                                                lane_colour.b,
                                                lane_colour.a);

                        //std::cout << "Adding half lane between " << start_system->Name() << " to " << dest_system->Name() << " with colour of empire " << empire->Name() << std::endl;

                        break;
                    }
                }
            }
        }
    }


    // fill new buffers
    m_starlane_vertices.createServerBuffer();
    m_starlane_colors.createServerBuffer();
    m_starlane_vertices.harmonizeBufferType(m_starlane_colors);
    m_RC_starlane_vertices.createServerBuffer();
    m_RC_starlane_colors.createServerBuffer();
    m_RC_starlane_vertices.harmonizeBufferType(m_RC_starlane_colors);
}

void MapWnd::ClearStarlaneRenderingBuffers() {
    m_starlane_vertices.clear();
    m_starlane_colors.clear();
    m_RC_starlane_vertices.clear();
    m_RC_starlane_colors.clear();
    m_resourceCenters.clear();
}

LaneEndpoints MapWnd::StarlaneEndPointsFromSystemPositions(double X1, double Y1, double X2, double Y2) {
    LaneEndpoints retval;

    // get unit vector
    double deltaX = X2 - X1, deltaY = Y2 - Y1;
    double mag = std::sqrt(deltaX*deltaX + deltaY*deltaY);

    double ring_radius = ClientUI::SystemCircleSize() / 2.0 + 0.5;

    // safety check.  don't modify original coordinates if they're too close togther
    if (mag > 2*ring_radius) {
        // rescale vector to length of ring radius
        double offsetX = deltaX / mag * ring_radius;
        double offsetY = deltaY / mag * ring_radius;

        // move start and end points inwards by rescaled vector
        X1 += offsetX;
        Y1 += offsetY;
        X2 -= offsetX;
        Y2 -= offsetY;
    }

    retval.X1 = static_cast<float>(X1);
    retval.Y1 = static_cast<float>(Y1);
    retval.X2 = static_cast<float>(X2);
    retval.Y2 = static_cast<float>(Y2);
    return retval;
}

void MapWnd::RestoreFromSaveData(const SaveGameUIData& data) {
    m_zoom_steps_in = data.map_zoom_steps_in;

    GG::Pt ul = UpperLeft();
    GG::Pt map_ul = GG::Pt(GG::X(data.map_left), GG::Y(data.map_top));
    GG::Pt map_move = map_ul - ul;
    OffsetMove(map_move);

    // this correction ensures that zooming in doesn't leave too large a margin to the side
    GG::Pt move_to_pt = ul = ClientUpperLeft();
    CorrectMapPosition(move_to_pt);
    //GG::Pt final_move = move_to_pt - ul;

    MoveTo(move_to_pt - GG::Pt(AppWidth(), AppHeight()));

    m_fleets_exploring = data.fleets_exploring;
}

void MapWnd::ShowSystemNames() {
    for (std::map<int, SystemIcon*>::iterator it = m_system_icons.begin(); it != m_system_icons.end(); ++it) {
        it->second->ShowName();
    }
}

void MapWnd::HideSystemNames() {
    for (std::map<int, SystemIcon*>::iterator it = m_system_icons.begin(); it != m_system_icons.end(); ++it) {
        it->second->HideName();
    }
}

void MapWnd::CenterOnMapCoord(double x, double y) {
    GG::Pt ul = ClientUpperLeft();
    GG::X_d current_x = (AppWidth() / 2 - ul.x) / ZoomFactor();
    GG::Y_d current_y = (AppHeight() / 2 - ul.y) / ZoomFactor();
    GG::Pt map_move = GG::Pt(static_cast<GG::X>((current_x - x) * ZoomFactor()),
                             static_cast<GG::Y>((current_y - y) * ZoomFactor()));
    OffsetMove(map_move);

    // this correction ensures that the centering doesn't leave too large a margin to the side
    GG::Pt move_to_pt = ul = ClientUpperLeft();
    CorrectMapPosition(move_to_pt);

    MoveTo(move_to_pt - GG::Pt(AppWidth(), AppHeight()));
}

void MapWnd::ShowPlanet(int planet_id) {
    if (!m_pedia_panel->Visible())
        TogglePedia();
    m_pedia_panel->SetPlanet(planet_id);
}

void MapWnd::ShowCombatLog(int log_id) {
    if (!m_pedia_panel->Visible())
        TogglePedia();
    m_pedia_panel->SetCombatLog(log_id);
}

void MapWnd::ShowTech(const std::string& tech_name) {
    if (!m_research_wnd->Visible())
        ToggleResearch();
    m_research_wnd->ShowTech(tech_name);
}

void MapWnd::ShowBuildingType(const std::string& building_type_name) {
    if (!m_production_wnd->Visible())
        ToggleProduction();
    m_production_wnd->ShowBuildingTypeInEncyclopedia(building_type_name);
}

void MapWnd::ShowPartType(const std::string& part_type_name) {
    if (!m_design_wnd->Visible())
        ToggleDesign();
    m_design_wnd->ShowPartTypeInEncyclopedia(part_type_name);
}

void MapWnd::ShowHullType(const std::string& hull_type_name) {
    if (!m_design_wnd->Visible())
        ToggleDesign();
    m_design_wnd->ShowHullTypeInEncyclopedia(hull_type_name);
}

void MapWnd::ShowShipDesign(int design_id) {
    if (!m_production_wnd->Visible())
        ToggleProduction();
    m_production_wnd->ShowShipDesignInEncyclopedia(design_id);
}

void MapWnd::ShowSpecial(const std::string& special_name) {
    if (!m_pedia_panel->Visible())
        TogglePedia();
    m_pedia_panel->SetSpecial(special_name);
}

void MapWnd::ShowSpecies(const std::string& species_name) {
    if (!m_pedia_panel->Visible())
        TogglePedia();
    m_pedia_panel->SetSpecies(species_name);
}

void MapWnd::ShowEmpire(int empire_id) {
    if (!m_pedia_panel->Visible())
        TogglePedia();
    m_pedia_panel->SetEmpire(empire_id);
}

void MapWnd::ShowEncyclopediaEntry(const std::string& str) {
    if (!m_pedia_panel->Visible())
        TogglePedia();
    m_pedia_panel->SetText(str);
}

void MapWnd::CenterOnObject(int id) {
    if (UniverseObject* obj = GetUniverseObject(id))
        CenterOnMapCoord(obj->X(), obj->Y());
}

void MapWnd::CenterOnObject(const UniverseObject* obj) {
    if (!obj) return;
    CenterOnMapCoord(obj->X(), obj->Y());
}

void MapWnd::ReselectLastSystem() {
    if (SidePanel::SystemID() != INVALID_OBJECT_ID)
        SelectSystem(SidePanel::SystemID());
}

void MapWnd::SelectSystem(int system_id) {
    //std::cout << "MapWnd::SelectSystem(" << system_id << ")" << std::endl;
    const System* system = GetSystem(system_id);
    if (!system && system_id != INVALID_OBJECT_ID) {
        Logger().errorStream() << "MapWnd::SelectSystem couldn't find system with id " << system_id << " so is selected no system instead";
        system_id = INVALID_OBJECT_ID;
    }


    if (system) {
        // ensure meter estimates are up to date, particularly for which ship is selected
        UpdateMeterEstimates(system_id, true);
    }


    if (SidePanel::SystemID() != system_id) {
        // remove map selection indicator from previously selected system
        if (SidePanel::SystemID() != INVALID_OBJECT_ID) {
            std::map<int, SystemIcon*>::iterator it = m_system_icons.find(SidePanel::SystemID());
            if (it != m_system_icons.end())
                it->second->SetSelected(false);
        }

        // set selected system on sidepanel and production screen, as appropriate
        if (m_in_production_view_mode)
            m_production_wnd->SelectSystem(system_id);  // calls SidePanel::SetSystem
        else
            SidePanel::SetSystem(system_id);

        // place map selection indicator on newly selected system
        if (SidePanel::SystemID() != INVALID_OBJECT_ID) {
            std::map<int, SystemIcon*>::iterator it = m_system_icons.find(SidePanel::SystemID());
            if (it != m_system_icons.end())
                it->second->SetSelected(true);
        }
    }


    if (m_in_production_view_mode) {
        // don't need to do anything to ensure this->m_side_panel is visible,
        // since it should be hidden if in production view mode.
        return;
    }


    // even if selected system hasn't changed, it may be nessary to show or
    // hide this mapwnd's sidepanel, in case it was hidden at some point and
    // should be visible, or is visible and should be hidden.

    if (SidePanel::SystemID() == INVALID_OBJECT_ID) {
        // no selected system.  hide sidepanel.
        m_side_panel->Hide();
    } else {
        // selected a valid system, show sidepanel
        m_side_panel->Show();
    }
}

void MapWnd::ReselectLastFleet() {
    //// DEBUG
    //std::cout << "MapWnd::ReselectLastFleet m_selected_fleet_ids: " << std::endl;
    //for (std::set<int>::const_iterator it = m_selected_fleet_ids.begin(); it != m_selected_fleet_ids.end(); ++it) {
    //    const UniverseObject* obj = GetUniverse().Object(*it);
    //    if (obj)
    //        std::cout << "    " << obj->Name() << "(" << *it << ")" << std::endl;
    //    else
    //        std::cout << "    [missing object] (" << *it << ")" << std::endl;
    //}

    const ObjectMap& objects = GetUniverse().Objects();

    // search through stored selected fleets' ids and remove ids of missing fleets
    std::set<int> missing_fleets;
    for (std::set<int>::const_iterator it = m_selected_fleet_ids.begin(); it != m_selected_fleet_ids.end(); ++it) {
        const Fleet* fleet = objects.Object<Fleet>(*it);
        if (!fleet)
            missing_fleets.insert(*it);
    }
    for (std::set<int>::const_iterator it = missing_fleets.begin(); it != missing_fleets.end(); ++it)
        m_selected_fleet_ids.erase(*it);


    // select a not-missing fleet, if any
    for (std::set<int>::const_iterator it = m_selected_fleet_ids.begin(); it != m_selected_fleet_ids.end(); ++it) {
        SelectFleet(*it);
        break;              // abort after first fleet selected... don't need to do more
    }
}

void MapWnd::SelectPlanet(int planetID)
{ m_production_wnd->SelectPlanet(planetID); }   // calls SidePanel::SelectPlanet()

void MapWnd::SelectFleet(int fleet_id)
{ SelectFleet(GetFleet(fleet_id)); }

void MapWnd::SelectFleet(Fleet* fleet) {
    FleetUIManager& manager = FleetUIManager::GetFleetUIManager();

    if (!fleet) {
        //std::cout << "MapWnd::SelectFleet selecting no fleet: deselecting all selected fleets." << std::endl;

        // first deselect any selected fleets in non-active fleet wnd.  this should
        // not emit any signals about the active fleet wnd's fleets changing
        FleetWnd* active_fleet_wnd = manager.ActiveFleetWnd();

        for (FleetUIManager::iterator it = manager.begin(); it != manager.end(); ++it) {
            FleetWnd* wnd = *it;
            if (wnd != active_fleet_wnd)
                wnd->SelectFleet(0);
        }

        // and finally deselect active fleet wnd fleets.  this might emit a signal
        // which will update this->m_selected_Fleets
        if (active_fleet_wnd)
            active_fleet_wnd->SelectFleet(0);

        return;
    }

    //std::cout << "MapWnd::SelectFleet " << fleet->ID() << std::endl;


    // if indicated fleet is already the only selected fleet in the active FleetWnd, don't need to do anything
    if (m_selected_fleet_ids.size() == 1 && m_selected_fleet_ids.find(fleet->ID()) != m_selected_fleet_ids.end())
        return;


    // find if there is a FleetWnd for this fleet already open.
    FleetWnd* fleet_wnd = manager.WndForFleet(fleet);

    // if there isn't a FleetWnd for this fleet open, need to open one
    if (!fleet_wnd) {
        //std::cout << "SelectFleet couldn't find fleetwnd for fleet " << std::endl;
        System* system = GetSystem(fleet->SystemID());

        // create fleetwnd to show fleet to be selected (actual selection occurs below).
        if (system) {
            fleet_wnd = manager.NewFleetWnd(system->ID(), fleet->Owner());
        } else {
            // get all (moving) fleets represented by fleet button for this fleet
            std::map<int, FleetButton*>::iterator it = m_fleet_buttons.find(fleet->ID());
            if (it == m_fleet_buttons.end()) {
                Logger().errorStream() << "Couldn't find a FleetButton for fleet in MapWnd::SelectFleet";
                return;
            }
            const std::vector<int>& wnd_fleet_ids = it->second->Fleets();
            // create new fleetwnd in which to show selected fleet
            fleet_wnd = manager.NewFleetWnd(wnd_fleet_ids);
        }


        // opened a new FleetWnd, so play sound
        FleetButton::PlayFleetButtonOpenSound();


        // position new FleetWnd.  default to last user-set position...
        GG::Pt wnd_position = FleetWnd::LastPosition();
        // unless the user hasn't opened and closed a FleetWnd yet, in which case use the lower-left
        if (wnd_position == GG::Pt())
            wnd_position = GG::Pt(GG::X(5), AppHeight() - fleet_wnd->Height() - 5);

        fleet_wnd->MoveTo(wnd_position);


        // safety check to ensure window is on screen... may be redundant
        if (AppWidth() - 5 < fleet_wnd->LowerRight().x)
            fleet_wnd->OffsetMove(GG::Pt(AppWidth() - 5 - fleet_wnd->LowerRight().x, GG::Y0));
        if (AppHeight() - 5 < fleet_wnd->LowerRight().y)
            fleet_wnd->OffsetMove(GG::Pt(GG::X0, AppHeight() - 5 - fleet_wnd->LowerRight().y));
    }


    // make sure selected fleet's FleetWnd is active
    manager.SetActiveFleetWnd(fleet_wnd);


    // select fleet in FleetWnd.  this deselects all other fleets in the FleetWnd.  
    // this->m_selected_fleet_ids will be updated by ActiveFleetWndSelectedFleetsChanged or ActiveFleetWndChanged
    // signals being emitted and connected to MapWnd::SelectedFleetsChanged
    fleet_wnd->SelectFleet(fleet->ID());
}

void MapWnd::SetFleetMovementLine(const FleetButton* fleet_button) {
    assert(fleet_button);
    // each fleet represented by button could have different move path
    for (std::vector<int>::const_iterator it = fleet_button->Fleets().begin(); it != fleet_button->Fleets().end(); ++it)
        SetFleetMovementLine(*it);
}

void MapWnd::SetFleetMovementLine(int fleet_id) {
    if (fleet_id == INVALID_OBJECT_ID)
        return;

    const Fleet* fleet = GetFleet(fleet_id);
    if (!fleet) {
        Logger().errorStream() << "MapWnd::SetFleetMovementLine was passed invalid fleet id " << fleet_id;
        return;
    }
    //std::cout << "creating fleet movement line for fleet at (" << fleet->X() << ", " << fleet->Y() << ")" << std::endl;

    // get colour: empire colour, or white if no single empire applicable
    GG::Clr line_colour = GG::CLR_WHITE;
    if (const Empire* empire = Empires().Lookup(fleet->Owner()))
        line_colour = empire->Color();
    else if (fleet->Unowned() && fleet->HasMonsters())
        line_colour = GG::CLR_RED;

    // create and store line
    m_fleet_lines[fleet_id] = MovementLineData(fleet->MovePath(), m_starlane_endpoints, line_colour);
}

void MapWnd::SetProjectedFleetMovementLine(int fleet_id, const std::list<int>& travel_route) {
    if (fleet_id == INVALID_OBJECT_ID)
        return;

    // ensure passed fleet exists
    const Fleet* fleet = GetFleet(fleet_id);
    if (!fleet) {
        Logger().errorStream() << "MapWnd::SetProjectedFleetMovementLine was passed invalid fleet id " << fleet_id;
        return;
    }

    // if route is empty, no projected line to show
    if (travel_route.empty()) {
        RemoveProjectedFleetMovementLine(fleet_id);
        return;
    }

    // get move path to show.  if there isn't one, show nothing
    std::list<MovePathNode> path = fleet->MovePath(travel_route);
    if (path.empty()) {
        // no route to display
        RemoveProjectedFleetMovementLine(fleet_id);
        return;
    }

    // get colour: empire colour, or white if no single empire applicable
    GG::Clr line_colour = GG::CLR_WHITE;
    if (const Empire* empire = Empires().Lookup(fleet->Owner()))
        line_colour = empire->Color();

    // create and store line
    m_projected_fleet_lines[fleet_id] = MovementLineData(path, m_starlane_endpoints, line_colour);
}

void MapWnd::SetProjectedFleetMovementLines(const std::vector<int>& fleet_ids,
                                            const std::list<int>& travel_route)
{
    for (std::vector<int>::const_iterator it = fleet_ids.begin(); it != fleet_ids.end(); ++it)
        SetProjectedFleetMovementLine(*it, travel_route);
}

void MapWnd::RemoveProjectedFleetMovementLine(int fleet_id) {
    std::map<int, MovementLineData>::iterator it = m_projected_fleet_lines.find(fleet_id);
    if (it != m_projected_fleet_lines.end())
        m_projected_fleet_lines.erase(it);
}

void MapWnd::ClearProjectedFleetMovementLines()
{ m_projected_fleet_lines.clear(); }

bool MapWnd::EventFilter(GG::Wnd* w, const GG::WndEvent& event) {
    if (event.Type() == GG::WndEvent::RClick && FleetUIManager::GetFleetUIManager().empty()) {
        // Attempt to close the SidePanel (if open); if this fails, just let Wnd w handle it.  
        // Note that this enforces a one-close-per-click policy.

        if (GetOptionsDB().Get<bool>("UI.window-quickclose")) {
            if (m_side_panel->Visible()) {
                m_side_panel->Hide();
                DetachChild(m_side_panel);
                return true;
            }
        }
    }
    return false;
}

void MapWnd::DoSystemIconsLayout() {
    // position and resize system icons and gaseous substance
    const int SYSTEM_ICON_SIZE = SystemIconSize();
    for (std::map<int, SystemIcon*>::iterator it = m_system_icons.begin(); it != m_system_icons.end(); ++it) {
        const System* system = GetSystem(it->first);
        if (!system) {
            Logger().errorStream() << "MapWnd::DoSystemIconsLayout couldn't get system with id " << it->first;
            continue;
        }

        GG::Pt icon_ul(GG::X(static_cast<int>(system->X()*ZoomFactor() - SYSTEM_ICON_SIZE / 2.0)),
                       GG::Y(static_cast<int>(system->Y()*ZoomFactor() - SYSTEM_ICON_SIZE / 2.0)));
        it->second->SizeMove(icon_ul, icon_ul + GG::Pt(GG::X(SYSTEM_ICON_SIZE), GG::Y(SYSTEM_ICON_SIZE)));
    }
}

void MapWnd::DoFieldIconsLayout() {
    // position and resize field icons
    for (std::map<int, FieldIcon*>::const_iterator field_it = m_field_icons.begin();
         field_it != m_field_icons.end(); ++field_it)
    {
        const Field* field = GetField(field_it->first);
        if (!field) {
            Logger().errorStream() << "MapWnd::DoFieldIconsLayout couldn't get field with id " << field_it->first;
            continue;
        }

        double RADIUS = ZoomFactor()*field->CurrentMeterValue(METER_SIZE);

        GG::Pt icon_ul(GG::X(static_cast<int>(field->X()*ZoomFactor() - RADIUS)),
                       GG::Y(static_cast<int>(field->Y()*ZoomFactor() - RADIUS)));
        field_it->second->SizeMove(icon_ul, icon_ul + GG::Pt(GG::X(2*RADIUS), GG::Y(2*RADIUS)));
    }
}

void MapWnd::DoFleetButtonsLayout() {
    const ObjectMap& objects = GetUniverse().Objects();
    const int SYSTEM_ICON_SIZE = SystemIconSize();

    // position departing fleet buttons
    for (std::map<int, std::set<FleetButton*> >::iterator it = m_departing_fleet_buttons.begin(); it != m_departing_fleet_buttons.end(); ++it) {
        // calculate system icon position
        const System* system = GetSystem(it->first);
        if (!system) {
            Logger().errorStream() << "MapWnd::DoFleetButtonsLayout couldn't find system with id " << it->first;
            continue;
        }

        GG::Pt icon_ul(GG::X(static_cast<int>(system->X()*ZoomFactor() - SYSTEM_ICON_SIZE / 2.0)),
                       GG::Y(static_cast<int>(system->Y()*ZoomFactor() - SYSTEM_ICON_SIZE / 2.0)));

        // get system icon itself.  can't use the system icon's UpperLeft to position fleet button due to weirdness that results that I don't want to figure out
        std::map<int, SystemIcon*>::const_iterator sys_it = m_system_icons.find(system->ID());
        if (sys_it == m_system_icons.end()) {
            Logger().errorStream() << "couldn't find system icon for fleet button in DoFleetButtonsLayout";
            continue;
        }
        const SystemIcon* system_icon = sys_it->second;

        // place all buttons
        int n = 1;
        std::set<FleetButton*>& buttons = it->second;
        for (std::set<FleetButton*>::iterator button_it = buttons.begin(); button_it != buttons.end(); ++button_it) {
            GG::Pt ul = system_icon->NthFleetButtonUpperLeft(n, true);
            ++n;
            (*button_it)->MoveTo(ul + icon_ul);
        }
    }

    // position stationary fleet buttons
    for (std::map<int, std::set<FleetButton*> >::iterator it = m_stationary_fleet_buttons.begin(); it != m_stationary_fleet_buttons.end(); ++it) {
        // calculate system icon position
        const System* system = GetSystem(it->first);
        if (!system) {
            Logger().errorStream() << "MapWnd::DoFleetButtonsLayout couldn't find system with id " << it->first;
            continue;
        }

        GG::Pt icon_ul(GG::X(static_cast<int>(system->X()*ZoomFactor() - SYSTEM_ICON_SIZE / 2.0)),
                       GG::Y(static_cast<int>(system->Y()*ZoomFactor() - SYSTEM_ICON_SIZE / 2.0)));

        // get system icon itself.  can't use the system icon's UpperLeft to position fleet button due to weirdness that results that I don't want to figure out
        std::map<int, SystemIcon*>::const_iterator sys_it = m_system_icons.find(system->ID());
        if (sys_it == m_system_icons.end()) {
            Logger().errorStream() << "couldn't find system icon for fleet button in DoFleetButtonsLayout";
            continue;
        }
        const SystemIcon* system_icon = sys_it->second;

        // place all buttons
        int n = 1;
        std::set<FleetButton*>& buttons = it->second;
        for (std::set<FleetButton*>::iterator button_it = buttons.begin(); button_it != buttons.end(); ++button_it) {
            GG::Pt ul = system_icon->NthFleetButtonUpperLeft(n, false);
            ++n;
            (*button_it)->MoveTo(ul + icon_ul);
        }
    }

    // position moving fleet buttons
    for (std::set<FleetButton*>::iterator it = m_moving_fleet_buttons.begin(); it != m_moving_fleet_buttons.end(); ++it) {
        FleetButton* fb = *it;

        const GG::Pt FLEET_BUTTON_SIZE = fb->Size();
        const Fleet* fleet = 0;

        // skip button if it has no fleets (somehow...?) or if the first fleet in the button is 0
        if (fb->Fleets().empty() || !(fleet = objects.Object<Fleet>(*fb->Fleets().begin()))) {
            Logger().errorStream() << "DoFleetButtonsLayout couldn't get first fleet for button";
            continue;
        }

        std::pair<double, double> button_pos = MovingFleetMapPositionOnLane(fleet);
        if (button_pos == std::make_pair(UniverseObject::INVALID_POSITION, UniverseObject::INVALID_POSITION))
            continue;   // skip positioning flees for which problems occurred...

        // position button
        GG::Pt button_ul(button_pos.first  * ZoomFactor() - FLEET_BUTTON_SIZE.x / 2.0,
                         button_pos.second * ZoomFactor() - FLEET_BUTTON_SIZE.y / 2.0);

        fb->MoveTo(button_ul);
    }
}

std::pair<double, double> MapWnd::MovingFleetMapPositionOnLane(const Fleet* fleet) const {
    if (!fleet) {
        return std::make_pair<double, double>(double(UniverseObject::INVALID_POSITION), double(UniverseObject::INVALID_POSITION));
    }

    // get endpoints of lane on screen, store in UnorderedIntPair which can be looked up in MapWnd's map of starlane endpoints
    int sys1_id = fleet->PreviousSystemID(), sys2_id = fleet->NextSystemID();
    std::pair<int, int> lane = UnorderedIntPair(sys1_id, sys2_id);

    // get apparent positions of endpoints for this lane that have been pre-calculated
    std::map<std::pair<int, int>, LaneEndpoints>::const_iterator endpoints_it = m_starlane_endpoints.find(lane);
    if (endpoints_it == m_starlane_endpoints.end()) {
        // couldn't find an entry for the lane this fleet is one, so just
        // return actual position of fleet on starlane - ignore the distance
        // away from the star centre at which starlane endpoints should appear
        return std::make_pair<double, double>(fleet->X(), fleet->Y());
    }

    // return apparent position of fleet on starlane
    const LaneEndpoints& screen_lane_endpoints = endpoints_it->second;
    return ScreenPosOnStarane(fleet->X(), fleet->Y(), lane.first, lane.second, screen_lane_endpoints);
}

void MapWnd::RefreshFleetButtons() {
    ScopedTimer timer("RefreshFleetButtons()");
    // determine fleets that need buttons so that fleets at the same location can
    // be grouped by empire owner and buttons created
    const ObjectMap& objects = GetUniverse().Objects();

    bool verbose_logging = GetOptionsDB().Get<bool>("verbose-logging");

    int client_empire_id = HumanClientApp::GetApp()->EmpireID();
    const std::set<int>& this_client_known_destroyed_objects = GetUniverse().EmpireKnownDestroyedObjectIDs(client_empire_id);
    const std::set<int>& this_client_stale_object_info = GetUniverse().EmpireStaleKnowledgeObjectIDs(client_empire_id);

    // for each system, each empire's fleets that are ordered to move,
    // but still at the system: "departing fleets"
    std::map<const System*, std::map<int, std::vector<const Fleet*> > > departing_fleets;
    std::vector<const UniverseObject*> departing_fleet_objects = objects.FindObjects(OrderedMovingFleetVisitor());
    for (std::vector<const UniverseObject*>::iterator it = departing_fleet_objects.begin();
        it != departing_fleet_objects.end(); ++it)
    {
        const UniverseObject* obj = *it;
        int object_id = obj->ID();

        // skip known destroyed and stale info objects
        if (this_client_known_destroyed_objects.find(object_id) != this_client_known_destroyed_objects.end())
            continue;
        if (this_client_stale_object_info.find(object_id) != this_client_stale_object_info.end())
            continue;

        // skip fleets outside systems
        if (obj->SystemID() == INVALID_OBJECT_ID)
            continue;

        const System* system = GetSystem(obj->SystemID());
        if (!system) {
            Logger().errorStream() << "couldn't get system with id " << obj->SystemID() << " of an departing fleet named " << obj->Name() << " in RefreshFleetButtons()";
            continue;
        }

        // skip empty fleets
        const Fleet* fleet = universe_object_cast<const Fleet*>(obj);
        if (fleet->Empty())
            continue;

        // store in map for this system and the fleet's owner empire
        departing_fleets[system][obj->Owner()].push_back(fleet);
    }
    departing_fleet_objects.clear();


    // for each system, each empire's fleets in a system, not
    // ordered to move: "stationary fleets"
    std::map<const System*, std::map<int, std::vector<const Fleet*> > > stationary_fleets;
    std::vector<const UniverseObject*> stationary_fleet_objects = objects.FindObjects(StationaryFleetVisitor());
    for (std::vector<const UniverseObject*>::iterator it = stationary_fleet_objects.begin();
         it != stationary_fleet_objects.end(); ++it)
    {
        const UniverseObject* obj = *it;
        int object_id = obj->ID();

        if (verbose_logging)
            Logger().debugStream() << "stationary fleet id: " << object_id;

        // skip known destroyed and stale info objects
        if (this_client_known_destroyed_objects.find(object_id) != this_client_known_destroyed_objects.end())
            continue;
        if (this_client_stale_object_info.find(object_id) != this_client_stale_object_info.end())
            continue;

        if (verbose_logging)
            Logger().debugStream() << " ... not stale, not destroyed";

        // skip fleets outside systems
        if (obj->SystemID() == INVALID_OBJECT_ID)
            continue;

        const System* system = GetSystem(obj->SystemID());
        if (!system) {
            Logger().errorStream() << "couldn't get system of a stationary fleet in RefreshFleetButtons()";
            continue;
        }

        // skip empty fleets
        const Fleet* fleet = universe_object_cast<const Fleet*>(obj);
        if (fleet->Empty())
            continue;

        // store in map for the system and fleet's owner empire
        stationary_fleets[system][obj->Owner()].push_back(fleet);
    }
    stationary_fleet_objects.clear();


    // for each universe location, map from empire id to fleets
    // moving along starlanes: "moving fleets"
    std::map<std::pair<double, double>, std::map<int, std::vector<const Fleet*> > > moving_fleets;
    std::vector<const UniverseObject*> moving_fleet_objects = objects.FindObjects(MovingFleetVisitor());
    for (std::vector<const UniverseObject*>::iterator it = moving_fleet_objects.begin();
         it != moving_fleet_objects.end(); ++it)
    {
        const UniverseObject* obj = *it;
        int object_id = obj->ID();

        // skip known destroyed and stale info objects
        if (this_client_known_destroyed_objects.find(object_id) != this_client_known_destroyed_objects.end())
            continue;
        if (this_client_stale_object_info.find(object_id) != this_client_stale_object_info.end())
            continue;

        if (obj->SystemID() != INVALID_OBJECT_ID) {
            Logger().errorStream() << "a fleet that was supposed to be moving had a valid system in RefreshFleetButtons()";
            continue;
        }

        // skip empty fleets
        const Fleet* fleet = universe_object_cast<const Fleet*>(obj);
        if (fleet->Empty())
            continue;

        // store in map
        moving_fleets[std::make_pair(obj->X(), obj->Y())][obj->Owner()].push_back(fleet);
    }
    moving_fleet_objects.clear();



    // clear old fleet buttons
    m_fleet_buttons.clear();            // duplicates pointers in following containers

    for (std::map<int, std::set<FleetButton*> >::iterator it = m_stationary_fleet_buttons.begin(); it != m_stationary_fleet_buttons.end(); ++it)
        for (std::set<FleetButton*>::iterator set_it = it->second.begin(); set_it != it->second.end(); ++set_it)
            delete *set_it;
    m_stationary_fleet_buttons.clear();

    for (std::map<int, std::set<FleetButton*> >::iterator it = m_departing_fleet_buttons.begin(); it != m_departing_fleet_buttons.end(); ++it)
        for (std::set<FleetButton*>::iterator set_it = it->second.begin(); set_it != it->second.end(); ++set_it)
            delete *set_it;
    m_departing_fleet_buttons.clear();

    for (std::set<FleetButton*>::iterator set_it = m_moving_fleet_buttons.begin(); set_it != m_moving_fleet_buttons.end(); ++set_it)
        delete *set_it;
    m_moving_fleet_buttons.clear();


    // create new fleet buttons for fleets...
    const FleetButton::SizeType FLEETBUTTON_SIZE = FleetButtonSizeType();

    // departing fleets
    for (std::map<const System*, std::map<int, std::vector<const Fleet*> > >::iterator
         departing_fleets_it = departing_fleets.begin();
         departing_fleets_it != departing_fleets.end(); ++departing_fleets_it)
    {
        const System* system = departing_fleets_it->first;
        int system_id = system->ID();
        const std::map<int, std::vector<const Fleet*> >& empires_map = departing_fleets_it->second;

        // create button for each empire's fleets
        for (std::map<int, std::vector<const Fleet*> >::const_iterator empire_it = empires_map.begin(); empire_it != empires_map.end(); ++empire_it) {
            const std::vector<const Fleet*> fleets = empire_it->second;
            if (fleets.empty())
                continue;

            // buttons need fleet IDs
            std::vector<int> fleet_IDs;
            for (std::vector<const Fleet*>::const_iterator fleet_it = fleets.begin(); fleet_it != fleets.end(); ++fleet_it)
                fleet_IDs.push_back((*fleet_it)->ID());

            // create new fleetbutton for this cluster of fleets
            FleetButton* fb = new FleetButton(fleet_IDs, FLEETBUTTON_SIZE);

            // store
            m_departing_fleet_buttons[system_id].insert(fb);

            for (std::vector<const Fleet*>::const_iterator fleet_it = fleets.begin(); fleet_it != fleets.end(); ++fleet_it)
                m_fleet_buttons[(*fleet_it)->ID()] = fb;

            AttachChild(fb);
            MoveChildDown(fb);  // so fleet buttons won't show over sidepanel or sitrep window
            GG::Connect(fb->ClickedSignal, boost::bind(&MapWnd::FleetButtonClicked, this, fb));
        }
    }

    // stationary fleets
    for (std::map<const System*, std::map<int, std::vector<const Fleet*> > >::iterator
         stationary_fleets_it = stationary_fleets.begin();
         stationary_fleets_it != stationary_fleets.end(); ++stationary_fleets_it)
    {
        const System* system = stationary_fleets_it->first;
        int system_id = system->ID();
        const std::map<int, std::vector<const Fleet*> >& empires_map = stationary_fleets_it->second;

        // create button for each empire's fleets
        for (std::map<int, std::vector<const Fleet*> >::const_iterator empire_it = empires_map.begin(); empire_it != empires_map.end(); ++empire_it) {
            const std::vector<const Fleet*> fleets = empire_it->second;
            if (fleets.empty())
                continue;

            // buttons need fleet IDs
            std::vector<int> fleet_IDs;
            for (std::vector<const Fleet*>::const_iterator fleet_it = fleets.begin(); fleet_it != fleets.end(); ++fleet_it)
                fleet_IDs.push_back((*fleet_it)->ID());

            // create new fleetbutton for this cluster of fleets
            FleetButton* fb = new FleetButton(fleet_IDs, FLEETBUTTON_SIZE);

            // store
            m_stationary_fleet_buttons[system_id].insert(fb);

            for (std::vector<const Fleet*>::const_iterator fleet_it = fleets.begin(); fleet_it != fleets.end(); ++fleet_it)
                m_fleet_buttons[(*fleet_it)->ID()] = fb;

            AttachChild(fb);
            MoveChildDown(fb);  // so fleet buttons won't show over sidepanel or sitrep window
            GG::Connect(fb->ClickedSignal, boost::bind(&MapWnd::FleetButtonClicked, this, fb));
        }
    }

    // moving fleets
    for (std::map<std::pair<double, double>, std::map<int, std::vector<const Fleet*> > >::iterator
         moving_fleets_it = moving_fleets.begin();
         moving_fleets_it != moving_fleets.end(); ++moving_fleets_it)
    {
        const std::map<int, std::vector<const Fleet*> >& empires_map = moving_fleets_it->second;
        //std::cout << "creating moving fleet buttons at location (" << moving_fleets_it->first.first << ", " << moving_fleets_it->first.second << ")" << std::endl;

        // create button for each empire's fleets
        for (std::map<int, std::vector<const Fleet*> >::const_iterator empire_it = empires_map.begin(); empire_it != empires_map.end(); ++empire_it) {
            const std::vector<const Fleet*>& fleets = empire_it->second;
            if (fleets.empty())
                continue;

            //std::cout << " ... creating moving fleet buttons for empire " << empire->Name() << std::endl;

            // buttons need fleet IDs
            std::vector<int> fleet_IDs;
            for (std::vector<const Fleet*>::const_iterator fleet_it = fleets.begin(); fleet_it != fleets.end(); ++fleet_it)
                fleet_IDs.push_back((*fleet_it)->ID());

            // create new fleetbutton for this cluster of fleets
            FleetButton* fb = new FleetButton(fleet_IDs, FLEETBUTTON_SIZE);

            // store
            m_moving_fleet_buttons.insert(fb);

            for (std::vector<const Fleet*>::const_iterator fleet_it = fleets.begin(); fleet_it != fleets.end(); ++fleet_it)
                m_fleet_buttons[(*fleet_it)->ID()] = fb;

            AttachChild(fb);
            MoveChildDown(fb);  // so fleet buttons won't show over sidepanel or sitrep window
            GG::Connect(fb->ClickedSignal, boost::bind(&MapWnd::FleetButtonClicked, this, fb));
        }
    }


    // position fleetbuttons
    DoFleetButtonsLayout();


    // add selection indicators to fleetbuttons
    RefreshFleetButtonSelectionIndicators();


    // create movement lines (after positioning buttons, so lines will originate from button location)
    for (std::map<int, FleetButton*>::iterator it = m_fleet_buttons.begin(); it != m_fleet_buttons.end(); ++it)
        SetFleetMovementLine(it->second);
}

void MapWnd::FleetAddedOrRemoved(Fleet& fleet) {
    RefreshFleetButtons();
    RefreshFleetSignals();
}

void MapWnd::RefreshFleetSignals() {
    const ObjectMap& objects = GetUniverse().Objects();

    // disconnect old fleet statechangedsignal connections
    for (std::map<int, boost::signals::connection>::iterator it = m_fleet_state_change_signals.begin(); it != m_fleet_state_change_signals.end(); ++it)
        it->second.disconnect();
    m_fleet_state_change_signals.clear();


    // connect fleet change signals to update fleet movement lines, so that ordering
    // fleets to move updates their displayed path and rearranges fleet buttons (if necessary)
    std::vector<const Fleet*> fleets = objects.FindObjects<Fleet>();
    for (std::vector<const Fleet*>::const_iterator it = fleets.begin(); it != fleets.end(); ++it) {
        const Fleet *fleet = *it;
        m_fleet_state_change_signals[fleet->ID()] = GG::Connect(fleet->StateChangedSignal, &MapWnd::RefreshFleetButtons, this);
    }
}

void MapWnd::RefreshSliders() {
    if (m_zoom_slider) {
        if (GetOptionsDB().Get<bool>("UI.show-galaxy-map-zoom-slider"))
            m_zoom_slider->Show();
        else
            m_zoom_slider->Hide();
    }
}

int MapWnd::SystemIconSize() const
{ return static_cast<int>(ClientUI::SystemIconSize() * ZoomFactor()); }

int MapWnd::SystemNamePts() const {
    const int       SYSTEM_NAME_MINIMUM_PTS = 6;    // limit to absolute minimum point size
    const double    MAX_NAME_ZOOM_FACTOR = 1.5;     // limit to relative max above standard UI font size
    const double    NAME_ZOOM_FACTOR = std::min(MAX_NAME_ZOOM_FACTOR, ZoomFactor());
    const int       ZOOMED_PTS = static_cast<int>(ClientUI::Pts() * NAME_ZOOM_FACTOR);
    return std::max(ZOOMED_PTS, SYSTEM_NAME_MINIMUM_PTS);
}

double MapWnd::SystemHaloScaleFactor() const
{ return 1.0 + log10(ZoomFactor()); }

FleetButton::SizeType MapWnd::FleetButtonSizeType() const {
    // no FLEET_BUTTON_LARGE as these icons are too big for the map.  (they can be used in the FleetWnd, however)
    if      (ZoomFactor() > ClientUI::MediumFleetButtonZoomThreshold())
        return FleetButton::FLEET_BUTTON_MEDIUM;

    else if (ZoomFactor() > ClientUI::SmallFleetButtonZoomThreshold())
        return FleetButton::FLEET_BUTTON_SMALL;

    else if (ZoomFactor() > ClientUI::TinyFleetButtonZoomThreshold())
        return FleetButton::FLEET_BUTTON_TINY;

    else
        return FleetButton::FLEET_BUTTON_NONE;
}

void MapWnd::Zoom(int delta) {
    GG::Pt center = GG::Pt(AppWidth() / 2.0, AppHeight() / 2.0);
    Zoom(delta, center);
}

void MapWnd::Zoom(int delta, const GG::Pt& position) {
    if (delta == 0)
        return;

    // increment zoom steps in by delta steps
    double new_zoom_steps_in = m_zoom_steps_in + static_cast<double>(delta);
    SetZoom(new_zoom_steps_in, true, position);
}

void MapWnd::SetZoom(double steps_in, bool update_slide) {
    GG::Pt center = GG::Pt(AppWidth() / 2.0, AppHeight() / 2.0);
    SetZoom(steps_in, update_slide, center);
}

void MapWnd::SetZoom(double steps_in, bool update_slide, const GG::Pt& position) {
    // impose range limits on zoom steps
    double new_steps_in = std::max(std::min(steps_in, ZOOM_IN_MAX_STEPS), ZOOM_IN_MIN_STEPS);

    // abort if no change
    if (new_steps_in == m_zoom_steps_in)
        return;


    // save position offsets and old zoom factors
    GG::Pt                      ul =                    ClientUpperLeft();
    const GG::X_d               center_x =              AppWidth() / 2.0;
    const GG::Y_d               center_y =              AppHeight() / 2.0;
    GG::X_d                     ul_offset_x =           ul.x - center_x;
    GG::Y_d                     ul_offset_y =           ul.y - center_y;
    const double                OLD_ZOOM =              ZoomFactor();
    const FleetButton::SizeType OLD_FLEETBUTTON_SIZE =  FleetButtonSizeType();


    // set new zoom level
    m_zoom_steps_in = new_steps_in;


    // keeps position the same after zooming
    // used to keep the mouse at the same position when doing mouse wheel zoom
    const GG::Pt position_center_delta = GG::Pt(position.x - center_x, position.y - center_y); 
    ul_offset_x -= position_center_delta.x;
    ul_offset_y -= position_center_delta.y;

    // correct map offsets for zoom changes
    ul_offset_x *= (ZoomFactor() / OLD_ZOOM);
    ul_offset_y *= (ZoomFactor() / OLD_ZOOM);

    // now add the zoom position offset at the new zoom level
    ul_offset_x += position_center_delta.x;
    ul_offset_y += position_center_delta.y;

    // show or hide system names, depending on zoom.  replicates code in MapWnd::Zoom
    if (ZoomFactor() * ClientUI::Pts() < MIN_SYSTEM_NAME_SIZE)
        HideSystemNames();
    else
        ShowSystemNames();


    DoSystemIconsLayout();
    DoFieldIconsLayout();


    // if fleet buttons need to change size, need to fully refresh them (clear
    // and recreate).  If they are the same size as before the zoom, then can
    // just reposition them without recreating
    const FleetButton::SizeType NEW_FLEETBUTTON_SIZE = FleetButtonSizeType();
    if (OLD_FLEETBUTTON_SIZE != NEW_FLEETBUTTON_SIZE)
        RefreshFleetButtons();
    else
        DoFleetButtonsLayout();


    // move field icons to bottom of child stack so that other icons can be moused over with a field
    for (std::map<int, FieldIcon*>::iterator it = m_field_icons.begin(); it != m_field_icons.end(); ++it)
        MoveChildDown(it->second);


    // translate map and UI widgets to account for the change in upper left due to zooming
    GG::Pt map_move(static_cast<GG::X>((center_x + ul_offset_x) - ul.x),
                    static_cast<GG::Y>((center_y + ul_offset_y) - ul.y));
    OffsetMove(map_move);

    // this correction ensures that zooming in doesn't leave too large a margin to the side
    GG::Pt move_to_pt = ul = ClientUpperLeft();
    CorrectMapPosition(move_to_pt);

    MoveTo(move_to_pt - GG::Pt(AppWidth(), AppHeight()));

    if (m_scale_line)
        m_scale_line->Update(ZoomFactor());
    if (update_slide && m_zoom_slider)
        m_zoom_slider->SlideTo(m_zoom_steps_in);

    ZoomedSignal(ZoomFactor());
}

void MapWnd::ZoomSlid(double pos, double low, double high)
{ SetZoom(pos, false); }

void MapWnd::CorrectMapPosition(GG::Pt &move_to_pt) {
    GG::X contents_width(static_cast<int>(ZoomFactor() * GetUniverse().UniverseWidth()));
    GG::X app_width =  AppWidth();
    GG::Y app_height = AppHeight();
    GG::X map_margin_width(app_width / 2.0);

    //std::cout << "MapWnd::CorrectMapPosition appwidth: " << Value(app_width) << " appheight: " << Value(app_height)
    //          << " to_x: " << Value(move_to_pt.x) << " to_y: " << Value(move_to_pt.y) << std::endl;;

    // restrict map positions to prevent map from being dragged too far off screen.
    // add extra padding to restrictions when universe to be shown is larger than
    // the screen area in which to show it.
    if (app_width - map_margin_width < contents_width || Value(app_height) - map_margin_width < contents_width) {
        if (map_margin_width < move_to_pt.x)
            move_to_pt.x = map_margin_width;
        if (move_to_pt.x + contents_width < app_width - map_margin_width)
            move_to_pt.x = app_width - map_margin_width - contents_width;
        if (map_margin_width < Value(move_to_pt.y))
            move_to_pt.y = GG::Y(Value(map_margin_width));
        if (Value(move_to_pt.y) + contents_width < Value(app_height) - map_margin_width)
            move_to_pt.y = app_height - Value(map_margin_width) - Value(contents_width);
    } else {
        if (move_to_pt.x < 0)
            move_to_pt.x = GG::X0;
        if (app_width < move_to_pt.x + contents_width)
            move_to_pt.x = app_width - contents_width;
        if (move_to_pt.y < GG::Y0)
            move_to_pt.y = GG::Y0;
        if (app_height < move_to_pt.y + Value(contents_width))
            move_to_pt.y = app_height - Value(contents_width);
    }
}

void MapWnd::SystemDoubleClicked(int system_id) {
    if (!m_in_production_view_mode) {
        if (!m_production_wnd->Visible())
            ToggleProduction();
        CenterOnObject(system_id);
        m_production_wnd->SelectSystem(system_id);
    }
}

void MapWnd::SystemLeftClicked(int system_id) {
    SelectSystem(system_id);
    SystemLeftClickedSignal(system_id);
}

void MapWnd::SystemRightClicked(int system_id) {
    if (!m_in_production_view_mode && FleetUIManager::GetFleetUIManager().ActiveFleetWnd()) {
        if (system_id == INVALID_OBJECT_ID)
            ClearProjectedFleetMovementLines();
        else
            PlotFleetMovement(system_id, true);
    }
    SystemRightClickedSignal(system_id);
}

void MapWnd::MouseEnteringSystem(int system_id) {
    if (!m_in_production_view_mode && FleetUIManager::GetFleetUIManager().ActiveFleetWnd())
        PlotFleetMovement(system_id, false);
    SystemBrowsedSignal(system_id);
}

void MapWnd::MouseLeavingSystem(int system_id)
{ MouseEnteringSystem(INVALID_OBJECT_ID); }

void MapWnd::PlotFleetMovement(int system_id, bool execute_move) {
    if (!FleetUIManager::GetFleetUIManager().ActiveFleetWnd())
        return;

    int empire_id = HumanClientApp::GetApp()->EmpireID();

    std::set<int> fleet_ids = FleetUIManager::GetFleetUIManager().ActiveFleetWnd()->SelectedFleetIDs();

    // apply to all this-player-owned fleets in currently-active FleetWnd
    for (std::set<int>::iterator it = fleet_ids.begin(); it != fleet_ids.end(); ++it) {
        int fleet_id = *it;

        const Fleet* fleet = GetFleet(fleet_id);
        if (!fleet) {
            Logger().errorStream() << "MapWnd::PlotFleetMovementLine couldn't get fleet with id " << *it;
            continue;
        }

        // only give orders / plot prospective move paths of fleets owned by player
        if (!(fleet->OwnedBy(empire_id)) || !(fleet->NumShips()))
            continue;

        // plot empty move pathes if destination is not a known system
        if (system_id == INVALID_OBJECT_ID) {
            RemoveProjectedFleetMovementLine(fleet_id);
            continue;
        }

        int fleet_sys_id = fleet->SystemID();

        int start_system = fleet_sys_id;
        if (fleet_sys_id == INVALID_OBJECT_ID)
            start_system = fleet->NextSystemID();

        // get path to destination...
        std::list<int> route = GetUniverse().ShortestPath(start_system, system_id, empire_id).first;

        // disallow "offroad" (direct non-starlane non-wormhole) travel
        if (route.size() == 2 && *route.begin() != *route.rbegin()) {
            int begin_id = *route.begin();
            const System* begin_sys = GetSystem(begin_id);
            int end_id = *route.rbegin();
            const System* end_sys = GetSystem(end_id);

            if (!begin_sys->HasStarlaneTo(end_id) && !begin_sys->HasWormholeTo(end_id) &&
                !end_sys->HasStarlaneTo(begin_id) && !end_sys->HasWormholeTo(begin_id))
            {
                continue;
            }
        }

        // if actually ordering fleet movement, not just prospectively previewing, ... do so
        if (execute_move && !route.empty()){
            HumanClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new FleetMoveOrder(empire_id, fleet_id, start_system, system_id)));
            StopFleetExploring(fleet_id);
        }

        // show route on map
        SetProjectedFleetMovementLine(fleet_id, route);
    }
}

void MapWnd::FleetButtonClicked(const FleetButton* fleet_btn) {
    //std::cout << "MapWnd::FleetButtonClicked" << std::endl;
    if (!fleet_btn)
        return;

    // allow switching to fleetView even when in production mode
    if (m_in_production_view_mode) {
        HideProduction();
    }


    // get possible fleets to select from, and a pointer to one of those fleets
    const std::vector<int>& btn_fleets = fleet_btn->Fleets();
    if (btn_fleets.empty()) {
        Logger().errorStream() << "Clicked FleetButton contained no fleets!";
        return;
    }
    const Fleet* first_fleet = GetFleet(btn_fleets[0]);


    // find if a FleetWnd for this FleetButton's fleet(s) is already open, and if so, if there
    // is a single selected fleet in the window, and if so, what fleet that is
    FleetWnd* wnd_for_button = FleetUIManager::GetFleetUIManager().WndForFleet(first_fleet);
    int already_selected_fleet_id = INVALID_OBJECT_ID;
    if (wnd_for_button) {
        //std::cout << "FleetButtonClicked found open fleetwnd for fleet" << std::endl;
        // there is already FleetWnd for this button open.

        // check which fleet(s) is/are selected in the button's FleetWnd
        std::set<int> selected_fleet_ids = wnd_for_button->SelectedFleetIDs();

        // record selected fleet if just one fleet is selected.  otherwise, keep default
        // INVALID_OBJECT_ID to indicate that no single fleet is selected
        if (selected_fleet_ids.size() == 1)
            already_selected_fleet_id = *(selected_fleet_ids.begin());
    } else {
        //std::cout << "FleetButtonClicked did not find open fleetwnd for fleet" << std::endl;
    }


    // pick fleet to select from fleets represented by the clicked FleetButton.
    int fleet_to_select_id = INVALID_OBJECT_ID;


    if (already_selected_fleet_id == INVALID_OBJECT_ID || btn_fleets.size() == 1) {
        // no (single) fleet is already selected, or there is only one selectable fleet,
        // so select first fleet in button
        fleet_to_select_id = *btn_fleets.begin();

    } else {
        // select next fleet after already-selected fleet, or first fleet if already-selected
        // fleet is the last fleet in the button.

        // to do this, scan through button's fleets to find already_selected_fleet
        bool found_already_selected_fleet = false;
        for (std::vector<int>::const_iterator it = btn_fleets.begin(); it != btn_fleets.end(); ++it) {
            if (*it == already_selected_fleet_id) {
                // found already selected fleet.  get NEXT fleet.  don't need to worry about
                // there not being enough fleets to do this because if above checks for case
                // of there being only one fleet in this button
                ++it;
                // if next fleet iterator is past end of fleets, loop around to first fleet
                if (it == btn_fleets.end())
                    it = btn_fleets.begin();
                // get fleet to select out of iterator
                fleet_to_select_id = *it;
                found_already_selected_fleet = true;
                break;
            }
        }

        if (!found_already_selected_fleet) {
            // didn't find already-selected fleet.  the selected fleet might have been moving when the
            // click button was for stationary fleets, or vice versa.  regardless, just default back
            // to selecting the first fleet for this button
            fleet_to_select_id = *btn_fleets.begin();
        }
    }


    // select chosen fleet
    if (fleet_to_select_id != INVALID_OBJECT_ID)
        SelectFleet(fleet_to_select_id);
}

void MapWnd::SelectedFleetsChanged() {
    // get selected fleets
    std::set<int> selected_fleet_ids;
    if (const FleetWnd* fleet_wnd = FleetUIManager::GetFleetUIManager().ActiveFleetWnd())
        selected_fleet_ids = fleet_wnd->SelectedFleetIDs();

    // if old and new sets of selected fleets are the same, don't need to change anything
    if (selected_fleet_ids == m_selected_fleet_ids)
        return;

    // set new selected fleets
    m_selected_fleet_ids = selected_fleet_ids;

    // update fleetbutton selection indicators
    RefreshFleetButtonSelectionIndicators();
}

void MapWnd::SelectedShipsChanged() {
    Logger().debugStream() << "SelectedShipsChanged starting...";
    ScopedTimer timer("MapWnd::SelectedShipsChanged", true);

    // get selected ships
    std::set<int> selected_ship_ids;
    if (const FleetWnd* fleet_wnd = FleetUIManager::GetFleetUIManager().ActiveFleetWnd())
        selected_ship_ids = fleet_wnd->SelectedShipIDs();

    // if old and new sets of selected fleets are the same, don't need to change anything
    if (selected_ship_ids == m_selected_fleet_ids)
        return;

    // set new selected fleets
    m_selected_ship_ids = selected_ship_ids;


    // refresh meters of planets in currently selected system, as changing selected fleets
    // may have changed which species a planet should have population estimates shown for

    int sidepanel_system_id = SidePanel::SystemID();
    if (sidepanel_system_id == INVALID_OBJECT_ID)
        return;

    // todo: update only target population meters

    int this_client_empire_id = HumanClientApp::GetApp()->EmpireID();
    const std::set<int>& this_client_known_destroyed_objects = GetUniverse().EmpireKnownDestroyedObjectIDs(this_client_empire_id);

    std::vector<int> planets = Objects().FindObjectIDs<Planet>();
    std::vector<int> system_planets;    system_planets.reserve(16); // arbitrary number big enough for all planets in a system
    for (std::vector<int>::const_iterator it = planets.begin(); it != planets.end(); ++it) {
        if (this_client_known_destroyed_objects.find(*it) != this_client_known_destroyed_objects.end())
            continue;
        const Planet* planet = GetPlanet(*it);
        if (!planet)
            continue;
        if (planet->SystemID() != sidepanel_system_id)
            continue;
        system_planets.push_back(*it);
    }

    UpdateMeterEstimates(system_planets);
    SidePanel::Update();
}

void MapWnd::RefreshFleetButtonSelectionIndicators() {
    //std::cout << "MapWnd::RefreshFleetButtonSelectionIndicators()" << std::endl;

    // clear old selection indicators
    for (std::map<int, std::set<FleetButton*> >::iterator it = m_stationary_fleet_buttons.begin(); it != m_stationary_fleet_buttons.end(); ++it) {
        std::set<FleetButton*>& set = it->second;
        for (std::set<FleetButton*>::iterator button_it = set.begin(); button_it != set.end(); ++button_it)
            (*button_it)->SetSelected(false);
    }

    for (std::map<int, std::set<FleetButton*> >::iterator it = m_departing_fleet_buttons.begin(); it != m_departing_fleet_buttons.end(); ++it) {
        std::set<FleetButton*>& set = it->second;
        for (std::set<FleetButton*>::iterator button_it = set.begin(); button_it != set.end(); ++button_it)
            (*button_it)->SetSelected(false);
    }

    for (std::set<FleetButton*>::iterator it = m_moving_fleet_buttons.begin(); it != m_moving_fleet_buttons.end(); ++it) {
        (*it)->SetSelected(false);
    }


    // add new selection indicators
    for (std::set<int>::const_iterator it = m_selected_fleet_ids.begin(); it != m_selected_fleet_ids.end(); ++it) {
        int fleet_id = *it;
        std::map<int, FleetButton*>::iterator button_it = m_fleet_buttons.find(fleet_id);
        if (button_it != m_fleet_buttons.end())
            button_it->second->SetSelected(true);
    }
}

void MapWnd::HandleEmpireElimination(int empire_id)
{}

void MapWnd::UniverseObjectDeleted(const UniverseObject *obj) {
    Logger().debugStream() << "MapWnd::UniverseObjectDeleted";
    if (const Fleet* fleet = universe_object_cast<const Fleet*>(obj)) {
        std::map<int, MovementLineData>::iterator it1 = m_fleet_lines.find(fleet->ID());
        if (it1 != m_fleet_lines.end())
            m_fleet_lines.erase(it1);

        std::map<int, MovementLineData>::iterator it2 = m_projected_fleet_lines.find(fleet->ID());
        if (it2 != m_projected_fleet_lines.end())
            m_projected_fleet_lines.erase(it2);
    }
}

void MapWnd::RegisterPopup(MapWndPopup* popup) {
    if (popup)
        m_popups.push_back(popup);
}

void MapWnd::RemovePopup(MapWndPopup* popup) {
    if (popup) {
        std::list<MapWndPopup*>::iterator it = std::find(m_popups.begin(), m_popups.end(), popup);
        if (it != m_popups.end())
            m_popups.erase(it);
    }
}

void MapWnd::Cleanup() {
    CloseAllPopups();
    DisconnectKeyboardAcceleratorSignals();
    HideResearch();
    HideProduction();
    HideDesign();
    HideSitRep();
    HidePedia();
    HideObjects();
    m_pedia_panel->ClearItems();    // deletes all pedia items in the memory
    m_toolbar->Hide();
    m_FPS->Hide();
    m_scale_line->Hide();
    m_zoom_slider->Hide();
}

void MapWnd::Sanitize() {
    //std::cout << "MapWnd::Sanitize()" << std::endl;
    Cleanup();

    SelectSystem(INVALID_OBJECT_ID);

    ClearSystemRenderingBuffers();
    ClearStarlaneRenderingBuffers();

    if (ClientUI* cui = ClientUI::GetClientUI()) {
        // clearing of message window commented out because scrollbar has quirks
        // after doing so until enough messages are added to 
        //if (MessageWnd* msg_wnd = cui->GetMessageWnd())
        //    msg_wnd->Clear();
        if (PlayerListWnd* plr_wnd = cui->GetPlayerListWnd())
            plr_wnd->Clear();
    }

    GG::Pt sp_ul = GG::Pt(AppWidth() - SidePanelWidth(), m_toolbar->LowerRight().y);
    GG::Pt sp_lr = sp_ul + GG::Pt(SidePanelWidth(), AppHeight() - m_toolbar->Height());
    m_side_panel->SizeMove(sp_ul, sp_lr);

    m_sitrep_panel->MoveTo(GG::Pt(SCALE_LINE_MAX_WIDTH + LAYOUT_MARGIN, m_toolbar->LowerRight().y));
    m_sitrep_panel->Resize(GG::Pt(SITREP_PANEL_WIDTH, SITREP_PANEL_HEIGHT));

    m_object_list_wnd->MoveTo(GG::Pt(GG::X0, m_scale_line->LowerRight().y + GG::Y(LAYOUT_MARGIN)));

    m_pedia_panel->MoveTo(GG::Pt(m_sitrep_panel->UpperLeft().x, m_sitrep_panel->LowerRight().y));

    MoveTo(GG::Pt(-AppWidth(), -AppHeight()));
    m_zoom_steps_in = 0.0;
    m_research_wnd->Sanitize();
    m_production_wnd->Sanitize();
    m_design_wnd->Sanitize();

    m_selected_fleet_ids.clear();
    m_selected_ship_ids.clear();

    m_starlane_endpoints.clear();

    for (std::map<int, std::set<FleetButton*> >::iterator it = m_stationary_fleet_buttons.begin(); it != m_stationary_fleet_buttons.end(); ++it) {
        std::set<FleetButton*>& set = it->second;
        for (std::set<FleetButton*>::iterator set_it = set.begin(); set_it != set.end(); ++set_it)
            delete *set_it;
        set.clear();
    }
    m_stationary_fleet_buttons.clear();

    for (std::map<int, std::set<FleetButton*> >::iterator it = m_departing_fleet_buttons.begin(); it != m_departing_fleet_buttons.end(); ++it) {
        std::set<FleetButton*>& set = it->second;
        for (std::set<FleetButton*>::iterator set_it = set.begin(); set_it != set.end(); ++set_it)
            delete *set_it;
        set.clear();
    }
    m_departing_fleet_buttons.clear();

    for (std::set<FleetButton*>::iterator set_it = m_moving_fleet_buttons.begin(); set_it != m_moving_fleet_buttons.end(); ++set_it)
        delete *set_it;
    m_moving_fleet_buttons.clear();

    m_fleet_buttons.clear();    // contains duplicate pointers of those in moving, departing and stationary set / maps, so don't need to delete again

    for (std::map<int, boost::signals::connection>::iterator it = m_fleet_state_change_signals.begin(); it != m_fleet_state_change_signals.end(); ++it)
        it->second.disconnect();
    m_fleet_state_change_signals.clear();

    for (std::map<int, std::vector<boost::signals::connection> >::iterator it = m_system_fleet_insert_remove_signals.begin(); it != m_system_fleet_insert_remove_signals.end(); ++it) {
        std::vector<boost::signals::connection>& vec = it->second;
        for (std::vector<boost::signals::connection>::iterator vec_it = vec.begin(); vec_it != vec.end(); ++vec_it)
            vec_it->disconnect();
        vec.clear();
    }
    m_system_fleet_insert_remove_signals.clear();

    for (std::set<boost::signals::connection>::iterator it = m_keyboard_accelerator_signals.begin(); it != m_keyboard_accelerator_signals.end(); ++it)
        it->disconnect();
    m_keyboard_accelerator_signals.clear();

    m_fleet_lines.clear();

    m_projected_fleet_lines.clear();

    for (std::map<int, SystemIcon*>::iterator it = m_system_icons.begin(); it != m_system_icons.end(); ++it)
        delete it->second;
    m_system_icons.clear();

    m_scanline_shader.reset();

    m_fleets_exploring.clear();
}

bool MapWnd::ReturnToMap() {
    if (m_sitrep_panel->Visible())
        ToggleSitRep();

    if (m_research_wnd->Visible())
        ToggleResearch();

    if (m_design_wnd->Visible())
        ToggleDesign();

    if (m_production_wnd->Visible())
        ToggleProduction();

    return true;
}

bool MapWnd::OpenChatWindow() {
    std::cout << "open chat window" << std::endl;
    ClientUI* cui = ClientUI::GetClientUI();
    if (!cui)
        return false;
    MessageWnd* msg_wnd = cui->GetMessageWnd();
    if (!msg_wnd)
        return false;
    GG::GUI* gui = GG::GUI::GetGUI();
    if (!gui)
        return false;
    gui->Register(msg_wnd); // GG comment for Register says re-registering same Wnd twice is a no-op.
    msg_wnd->OpenForInput();
    return true;
}

bool MapWnd::EndTurn() {
    Logger().debugStream() << "MapWnd::EndTurn";
    const Empire *empire = Empires().Lookup(HumanClientApp::GetApp()->EmpireID());
    if (empire) {
        double RP = empire->ResourceProduction(RE_RESEARCH);
        double PP = empire->ResourceProduction(RE_INDUSTRY);
        int turn_number = CurrentTurn();
        float ratio = (RP/(PP+0.0001));
        const GG::Clr color = empire->Color();
        Logger().debugStream() << "Current Output (turn " << turn_number << ") RP/PP: " << ratio << " (" << RP << "/" << PP << ")";
        Logger().debugStream() << "EmpireColors: " << static_cast<int>(color.r)
                                            << " " << static_cast<int>(color.g)
                                            << " " << static_cast<int>(color.b)
                                            << " " << static_cast<int>(color.a);
    }
    HumanClientApp::GetApp()->StartTurn();
    return true;
}

void MapWnd::ShowModeratorActions() {
    // hide other "competing" windows
    HideResearch();
    HideProduction();
    HideDesign();

    m_moderator_wnd->Refresh();
    m_moderator_wnd->Show();
    m_btn_moderator->MarkSelectedGray();
}

void MapWnd::HideModeratorActions() {
    m_moderator_wnd->Hide();
    m_btn_moderator->MarkNotSelected();
}

bool MapWnd::ToggleModeratorActions() {
    if (m_moderator_wnd->Visible())
        HideModeratorActions();
    else
        ShowModeratorActions();
    return true;
}

void MapWnd::ShowObjects() {
    ClearProjectedFleetMovementLines();

    // hide other "competing" windows
    HideResearch();
    HideProduction();
    HideDesign();

    // update objects window
    m_object_list_wnd->Refresh();

    // show the objects window
    m_object_list_wnd->Show();

    // indicate selection on button
    m_btn_objects->MarkSelectedGray();
}

void MapWnd::HideObjects() {
    m_object_list_wnd->Hide(); // necessary so it won't be visible when next toggled
    m_btn_objects->MarkNotSelected();
}

bool MapWnd::ToggleObjects() {
    if (m_object_list_wnd->Visible())
        HideObjects();
    else
        ShowObjects();
    return true;
}

void MapWnd::ShowSitRep() {
    ClearProjectedFleetMovementLines();

    // hide other "competing" windows
    HideResearch();
    HideProduction();
    HideDesign();

    // update sitrep window
    m_sitrep_panel->Update();

    // show the sitrep window
    m_sitrep_panel->Show();

    // indicate selection on button
    m_btn_siterep->MarkSelectedGray();
}

void MapWnd::HideSitRep() {
    m_sitrep_panel->Hide(); // necessary so it won't be visible when next toggled
    m_btn_siterep->MarkNotSelected();
}

bool MapWnd::ToggleSitRep() {
    if (m_sitrep_panel->Visible())
        HideSitRep();
    else
        ShowSitRep();
    return true;
}

void MapWnd::ShowPedia() {
    ClearProjectedFleetMovementLines();

    // hide other "competing" windows
    HideResearch();
    HideProduction();
    HideDesign();

    if (m_pedia_panel->GetItemsSize() == 0)
        m_pedia_panel->SetIndex();
    m_pedia_panel->Show();
    m_pedia_panel->Refresh();
    GG::GUI::GetGUI()->MoveUp(m_pedia_panel);

    // indicate selection on button
    m_btn_pedia->MarkSelectedGray();
}

void MapWnd::HidePedia() {
    m_pedia_panel->Hide();
    m_btn_pedia->MarkNotSelected();
}

bool MapWnd::TogglePedia() {
    if (m_pedia_panel->Visible())
        HidePedia();
    else
        ShowPedia();
    return true;
}

void MapWnd::HideSidePanel() {
    m_sidepanel_open_before_showing_other = m_side_panel->Visible();   // a kludge, so the sidepanel will reappear after opening and closing a full screen wnd
    m_side_panel->Hide();
}

void MapWnd::RestoreSidePanel() {
    if (m_sidepanel_open_before_showing_other)
        ReselectLastSystem();
}

void MapWnd::ShowResearch() {
    ClearProjectedFleetMovementLines();

    // hide other "competing" windows
    HideObjects();
    HideSitRep();
    HideProduction();
    HideDesign();
    HideSidePanel();

    // show the research window
    m_research_wnd->Show();
    GG::GUI::GetGUI()->MoveUp(m_research_wnd);

    // indicate selection on button
    m_btn_research->MarkSelectedGray();

    m_pedia_panel->SetText("ENC_TECH", false);
}

void MapWnd::HideResearch() {
    m_research_wnd->Hide();
    m_btn_research->MarkNotSelected();
    ShowAllPopups();
    RestoreSidePanel();
}

bool MapWnd::ToggleResearch() {
    if (m_research_wnd->Visible())
        HideResearch();
    else
        ShowResearch();
    return true;
}

void MapWnd::ShowProduction() {
    ClearProjectedFleetMovementLines();

    // hide other "competing" windows
    HideObjects();
    HideSitRep();
    HideResearch();
    HideDesign();
    HideSidePanel();
    m_pedia_panel->SetIndex();

    // show the production window
    m_production_wnd->Show();
    m_in_production_view_mode = true;
    HideAllPopups();
    GG::GUI::GetGUI()->MoveUp(m_production_wnd);

    // indicate selection on button
    m_btn_production->MarkSelectedGray();

    // if no system is currently shown in sidepanel, default to this empire's
    // home system (ie. where the capital is)
    if (SidePanel::SystemID() == INVALID_OBJECT_ID) {
        if (const Empire* empire = HumanClientApp::GetApp()->Empires().Lookup(HumanClientApp::GetApp()->EmpireID()))
            if (const UniverseObject* obj = GetUniverseObject(empire->CapitalID()))
                SelectSystem(obj->SystemID());
    } else {
        // if a system is already shown, make sure a planet gets selected by
        // default when the production screen opens up
        m_production_wnd->SelectDefaultPlanet();
    }
}

void MapWnd::HideProduction() {
    m_production_wnd->Hide();
    m_in_production_view_mode = false;
    m_btn_production->MarkNotSelected();
    ShowAllPopups();
    RestoreSidePanel();
}

bool MapWnd::ToggleProduction() {
    if (m_in_production_view_mode)
        HideProduction();
    else
        ShowProduction();

    // make info panels in production/map window's side panel update their expand-collapse state
    m_side_panel->Update();

    return true;
}

void MapWnd::ShowDesign() {
    ClearProjectedFleetMovementLines();

    // hide other "competing" windows
    HideObjects();
    HideSitRep();
    HideResearch();
    HideProduction();
    HideSidePanel();

    // show the design window
    m_design_wnd->Show();
    GG::GUI::GetGUI()->MoveUp(m_design_wnd);
    //GG::GUI::GetGUI()->SetFocusWnd(m_design_wnd);
    DisableAlphaNumAccels();
    m_design_wnd->Reset();

    // indicate selection on button
    m_btn_design->MarkSelectedGray();
}

void MapWnd::HideDesign() {
    m_design_wnd->Hide();
    m_btn_design->MarkNotSelected();
    EnableAlphaNumAccels();
    RestoreSidePanel();
}

bool MapWnd::ToggleDesign() {
    if (m_design_wnd->Visible())
        HideDesign();
    else
        ShowDesign();
    return true;
}

bool MapWnd::ShowMenu() {
    if (!m_menu_showing) {
        ClearProjectedFleetMovementLines();
        m_menu_showing = true;
        m_btn_menu->MarkSelectedGray();
        InGameMenu menu;
        menu.Run();
        m_menu_showing = false;
        m_btn_menu->MarkNotSelected();
    }
    return true;
}

bool MapWnd::CloseSystemView() {
    SelectSystem(INVALID_OBJECT_ID);
    m_side_panel->Hide();   // redundant, but safer to keep in case the behavior of SelectSystem changes
    return true;
}

bool MapWnd::KeyboardZoomIn() {
    Zoom(1);
    return true;
}

bool MapWnd::KeyboardZoomOut() {
    Zoom(-1);
    return true;
}

void MapWnd::RefreshTradeResourceIndicator() {
    Empire* empire = HumanClientApp::GetApp()->Empires().Lookup(HumanClientApp::GetApp()->EmpireID());
    if (!empire) {
        m_trade->SetValue(0.0);
        return;
    }
    m_trade->SetValue(empire->ResourceStockpile(RE_TRADE));
    m_trade->ClearBrowseInfoWnd();
    m_trade->SetBrowseInfoWnd(boost::shared_ptr<GG::BrowseInfoWnd>(
        new TextBrowseWnd(UserString("MAP_TRADE_TITLE"), UserString("MAP_TRADE_TEXT"))));
}

void MapWnd::RefreshFleetResourceIndicator() {
    int empire_id = HumanClientApp::GetApp()->EmpireID();
    Empire* empire = HumanClientApp::GetApp()->Empires().Lookup(empire_id);
    if (!empire) {
        m_fleet->SetValue(0.0);
        return;
    }

    const std::set<int>& this_client_known_destroyed_objects = GetUniverse().EmpireKnownDestroyedObjectIDs(empire_id);

    int total_fleet_count = 0;
    const ObjectMap& objects = Objects();
    std::vector<const Ship*> ships = objects.FindObjects<Ship>();
    for (std::vector<const Ship*>::const_iterator it = ships.begin(); it != ships.end(); ++it) {
        const Ship* ship = *it;
        if (ship->OwnedBy(empire_id) && this_client_known_destroyed_objects.find(ship->ID()) == this_client_known_destroyed_objects.end())
            total_fleet_count++;
    }

    m_fleet->SetValue(total_fleet_count);
    m_fleet->ClearBrowseInfoWnd();
    m_fleet->SetBrowseInfoWnd(boost::shared_ptr<GG::BrowseInfoWnd>(
        new TextBrowseWnd(UserString("MAP_FLEET_TITLE"), UserString("MAP_FLEET_TEXT"))));
}

void MapWnd::RefreshResearchResourceIndicator() {
    const Empire* empire = HumanClientApp::GetApp()->Empires().Lookup(HumanClientApp::GetApp()->EmpireID());
    if (!empire) {
        m_research->SetValue(0.0);
        m_research_wasted->Hide();
        return;
    }
    m_research->SetValue(empire->ResourceProduction(RE_RESEARCH));
    m_research->ClearBrowseInfoWnd();
    m_research->SetBrowseInfoWnd(boost::shared_ptr<GG::BrowseInfoWnd>(
        new TextBrowseWnd(UserString("MAP_RESEARCH_TITLE"), UserString("MAP_RESEARCH_TEXT"))));

    //Logger().debugStream() << "Research spend: " << empire->GetResearchQueue().TotalRPsSpent() << " output: " << empire->ResourceProduction(RE_RESEARCH);
    double totalRPSpent = empire->GetResearchQueue().TotalRPsSpent();
    double totalProduction = empire->ResourceProduction(RE_RESEARCH);
    double totalWastedRP = totalProduction - totalRPSpent;
    if (totalWastedRP > 1E-6) {
        m_research_wasted->Show();
        m_research_wasted->ClearBrowseInfoWnd();
        m_research_wasted->SetBrowseInfoWnd(boost::shared_ptr<GG::BrowseInfoWnd>(
            new TextBrowseWnd(UserString("MAP_RES_WASTED_TITLE"),
                              boost::io::str(FlexibleFormat(UserString("MAP_RES_WASTED_TEXT"))
                                % DoubleToString(totalProduction, 3, false)
                                % DoubleToString(totalWastedRP, 3, false)))));
    } else {
        m_research_wasted->Hide();
    }
}

void MapWnd::RefreshDetectionIndicator() {
    const Empire* empire = HumanClientApp::GetApp()->Empires().Lookup(HumanClientApp::GetApp()->EmpireID());
    if (!empire)
        return;
    m_detection->SetValue(empire->GetMeter("METER_DETECTION_STRENGTH")->Current());
    m_detection->ClearBrowseInfoWnd();
    m_detection->SetBrowseInfoWnd(boost::shared_ptr<GG::BrowseInfoWnd>(
        new TextBrowseWnd(UserString("MAP_DETECTION_TITLE"), UserString("MAP_DETECTION_TEXT"))));
}

void MapWnd::RefreshIndustryResourceIndicator() {
    const Empire* empire = HumanClientApp::GetApp()->Empires().Lookup(HumanClientApp::GetApp()->EmpireID());
    if (!empire) {
        m_industry->SetValue(0.0);
        m_industry_wasted->Hide();
        return;
    }
    m_industry->SetValue(empire->ResourceProduction(RE_INDUSTRY));
    m_industry->ClearBrowseInfoWnd();
    m_industry->SetBrowseInfoWnd(boost::shared_ptr<GG::BrowseInfoWnd>(
        new TextBrowseWnd(UserString("MAP_PRODUCTION_TITLE"), UserString("MAP_PRODUCTION_TEXT"))));

    //Logger().debugStream() << "Industry spend: " << empire->GetProductionQueue().TotalPPsSpent() << " output: " << empire->ResourceProduction(RE_INDUSTRY);
    double totalPPSpent = empire->GetProductionQueue().TotalPPsSpent();
    double totalProduction = empire->ResourceProduction(RE_INDUSTRY);
    double totalWastedPP = totalProduction - totalPPSpent;
    if (totalWastedPP > 1E-6) {
        m_industry_wasted->Show();
        m_industry_wasted->ClearBrowseInfoWnd();
        m_industry_wasted->SetBrowseInfoWnd(boost::shared_ptr<GG::BrowseInfoWnd>(
            new TextBrowseWnd(UserString("MAP_PROD_WASTED_TITLE"),
                              boost::io::str(FlexibleFormat(UserString("MAP_PROD_WASTED_TEXT"))
                                % DoubleToString(totalProduction, 3, false) 
                                % DoubleToString(totalWastedPP, 3, false)))));
    } else {
        m_industry_wasted->Hide();
    }
}

void MapWnd::RefreshPopulationIndicator() {
    Empire* empire = HumanClientApp::GetApp()->Empires().Lookup(HumanClientApp::GetApp()->EmpireID());
    if (!empire) {
        m_population->SetValue(0.0);
        return;
    }
    m_population->SetValue(empire->GetPopulationPool().Population());
    m_population->ClearBrowseInfoWnd();

    const std::vector<int> pop_center_ids = empire->GetPopulationPool().PopCenterIDs();
    std::map<std::string, float> population_counts;
    const ObjectMap& objects = Objects();

    //tally up all species population counts
    for (std::vector<int>::const_iterator it = pop_center_ids.begin(); it != pop_center_ids.end(); it++) {
        const UniverseObject* obj = objects.Object(*it);
        const PopCenter* pc = dynamic_cast<const PopCenter*>(obj);
        if (!pc)
            continue;

        const std::string& species_name = pc->SpeciesName();
        if (species_name.empty())
            continue;

        population_counts[species_name] += pc->CurrentMeterValue(METER_POPULATION);
    }

    m_population->SetBrowseInfoWnd(boost::shared_ptr<GG::BrowseInfoWnd>(
        new CensusBrowseWnd(UserString("MAP_POPULATION_DISTRIBUTION"), population_counts)));
}

void MapWnd::UpdateMetersAndResourcePools() {
    UpdateMeterEstimates();
    UpdateEmpireResourcePools();
}

void MapWnd::UpdateMetersAndResourcePools(const std::vector<int>& objects_vec) {
    UpdateMeterEstimates(objects_vec);
    UpdateEmpireResourcePools();
}

void MapWnd::UpdateMetersAndResourcePools(int object_id, bool update_contained_objects) {
    //std::cout << "MapWnd::UpdateMetersAndResourcePools(" << object_id << ", " << update_contained_objects << ")" << std::endl;
    UpdateMeterEstimates(object_id, update_contained_objects);
    UpdateEmpireResourcePools();
}

void MapWnd::UpdateSidePanelSystemObjectMetersAndResourcePools()
{ UpdateMetersAndResourcePools(SidePanel::SystemID(), true); }

void MapWnd::UpdateMeterEstimates()
{ UpdateMeterEstimates(INVALID_OBJECT_ID, false); }

void MapWnd::UpdateMeterEstimates(int object_id, bool update_contained_objects) {
    //Logger().debugStream() << "MapWnd::UpdateMeterEstimates";

    const ObjectMap& objects = GetUniverse().Objects();
    int this_client_empire_id = HumanClientApp::GetApp()->EmpireID();
    const std::set<int>& this_client_known_destroyed_objects = GetUniverse().EmpireKnownDestroyedObjectIDs(this_client_empire_id);
    //const std::set<int>& this_client_stale_object_info = GetUniverse().EmpireStaleKnowledgeObjectIDs(this_client_empire_id);


    if (object_id == INVALID_OBJECT_ID) {
        // update meters for all objects.  Value of updated_contained_objects is
        // irrelivant and is ignored in this case.
        std::vector<int> object_ids;
        for (ObjectMap::const_iterator<> obj_it = objects.const_begin(); obj_it != objects.const_end(); ++obj_it) {
            int object_id = obj_it->ID();
            // skip known destroyed objects, but do update for stale objects
            if (this_client_known_destroyed_objects.find(object_id) != this_client_known_destroyed_objects.end())
                continue;
            object_ids.push_back(object_id);
        }

        UpdateMeterEstimates(object_ids);
        return;
    }

    // collect objects to update meters of.  this may be a single object, a
    // group of related objects, or all objects in the (known) universe.
    // also clear effect accounting for meters that are to be updated.
    std::set<int> objects_set;
    std::list<int> objects_list;
    objects_list.push_back(object_id);

    for (std::list<int>::iterator list_it = objects_list.begin(); list_it !=  objects_list.end(); ++list_it) {
        int cur_object_id = *list_it;

        const UniverseObject* cur_object = objects.Object(cur_object_id);
        if (!cur_object) {
            Logger().errorStream() << "MapWnd::UpdateMeterEstimates tried to get an invalid object with id " << cur_object_id;
            continue;
        }

        // skip known destroyed objects, but do update for stale objects
        if (this_client_known_destroyed_objects.find(cur_object_id) != this_client_known_destroyed_objects.end())
            continue;

        // add current object to list
        objects_set.insert(cur_object_id);

        // add contained objects within current object to list of objects to
        // process, if requested.  assumes no objects contain themselves (which
        // could cause infinite loops)
        if (update_contained_objects) {
            std::vector<int> contained_objects = cur_object->FindObjectIDs(); // get all contained objects
            std::copy(contained_objects.begin(), contained_objects.end(), std::back_inserter(objects_list));
        }
    }
    std::vector<int> objects_vec;
    objects_vec.reserve(objects_set.size());
    std::copy(objects_set.begin(), objects_set.end(), std::back_inserter(objects_vec));
    UpdateMeterEstimates(objects_vec);
}

void MapWnd::UpdateMeterEstimates(const std::vector<int>& objects_vec) {
    // add this player ownership to all planets in the objects_vec that aren't
    // currently colonized.  this way, any effects the player knows about that
    // would act on those planets if the player colonized them include those
    // planets in their scope.  This lets effects from techs the player knows
    // alter the max population of planet that is displayed to the player, even
    // if those effects have a condition that causes them to only act on
    // planets the player owns (so as to not improve enemy planets if a player
    // reseraches a tech that should only benefit him/herself)

    Logger().debugStream() << "MapWnd::UpdateMeterEstimates";

    int empire_id = HumanClientApp::GetApp()->EmpireID();
    GetUniverse().InhibitUniverseObjectSignals(true);

    std::string             colony_ship_species;
    // remember which planes are temporarily modified so it can be undone after updating meter estimates
    std::vector<Planet*>    ownership_modified_planets;
    std::vector<Planet*>    species_modified_planets;


    if (const Ship* ship = ValidSelectedColonyShip(SidePanel::SystemID())) {
        // there is a selected colony ship suitable for the visible system
        if (ship->CanColonize() && ship->OwnedBy(empire_id) && !ship->SpeciesName().empty()) {
            // selected ship: exists, is a colony ship, is owned by this client's player
            //                is in a system, and has a usable species.

            const std::string& species_name = ship->SpeciesName();
            int ship_system_id = ship->SystemID();

            // get all planets the player knows about that aren't yet colonized
            // (aren't owned by anyone) and are otherwise valid colonization
            // targets for the selected ship. Add the current player's ownership
            // to all, while remembering which planets this is done to (so it
            // can be undone later)
            for (std::vector<int>::const_iterator it = objects_vec.begin(); it != objects_vec.end(); ++it) {
                Planet* planet = GetPlanet(*it);
                if (!planet ||
                    (!planet->Unowned() && !planet->OwnedBy(empire_id)) ||
                    planet->SystemID() != ship_system_id ||
                    planet->CurrentMeterValue(METER_POPULATION) > 0.0)
                { continue; }

                PlanetEnvironment planet_env_for_colony_species = planet->EnvironmentForSpecies(species_name);
                if (planet_env_for_colony_species > PE_GOOD || planet_env_for_colony_species < PE_HOSTILE)
                    continue;

                if (planet->Unowned()) {
                    ownership_modified_planets.push_back(planet);
                    planet->SetOwner(empire_id);
                }
                if (planet->SpeciesName().empty()) {
                    species_modified_planets.push_back(planet);
                    planet->SetSpecies(species_name);
                }
            }
        }
    } else if (FleetUIManager::GetFleetUIManager().SelectedShipIDs().empty()) {
        // no suitable colony ship selected.  instead, for each planet being
        // updated, attempt to find a colony ship for it, and use that ship's
        // species for meter estimates.
        for (std::vector<int>::const_iterator it = objects_vec.begin(); it != objects_vec.end(); ++it) {
            Planet* planet = GetPlanet(*it);
            if (!planet ||
                (!planet->Unowned() && !planet->OwnedBy(empire_id)) ||
                planet->CurrentMeterValue(METER_POPULATION) > 0.0)
            { continue; }

            // attempt to find colony ship for this planet
            const Ship* ship = GetShip(AutomaticallyChosenColonyShip(*it));
            if (!ship)
                continue;

            const std::string& species_name = ship->SpeciesName();

            if (planet->Unowned()) {
                ownership_modified_planets.push_back(planet);
                planet->SetOwner(empire_id);
            }
            if (planet->SpeciesName().empty()) {
                species_modified_planets.push_back(planet);
                planet->SetSpecies(species_name);
            }
        }
    }


    // update meter estimates with temporary ownership / species set
    GetUniverse().UpdateMeterEstimates(objects_vec);


    // undo any temporary changes from above
    for (std::vector<Planet*>::iterator it = ownership_modified_planets.begin();
         it != ownership_modified_planets.end(); ++it)
    { (*it)->SetOwner(ALL_EMPIRES); }
    for (std::vector<Planet*>::iterator it = species_modified_planets.begin();
         it != species_modified_planets.end(); ++it)
    { (*it)->SetSpecies(""); }

    GetUniverse().InhibitUniverseObjectSignals(false);
}

void MapWnd::UpdateEmpireResourcePools() {
    //std::cout << "MapWnd::UpdateEmpireResourcePools" << std::endl;
    Empire *empire = HumanClientApp::GetApp()->Empires().Lookup( HumanClientApp::GetApp()->EmpireID() );
    /* Recalculate stockpile, available, production, predicted change of resources.  When resourcepools
       update, they emit ChangeSignal, which is connected to MapWnd::Refresh???ResourceIndicator, which
       updates the empire resource pool indicators of the MapWnd. */
    empire->UpdateResourcePools();
    MidTurnUpdate();

    // Update indicators on sidepanel, which are not directly connected to from the ResourcePool ChangedSignal
    SidePanel::Update();
}

bool MapWnd::ZoomToHomeSystem() {
    int id = Empires().Lookup(HumanClientApp::GetApp()->EmpireID())->CapitalID();

    if (id != INVALID_OBJECT_ID) {
        const UniverseObject *object = GetUniverseObject(id);
        if (!object) return false;
        CenterOnObject(object->SystemID());
        SelectSystem(object->SystemID());
    }

    return true;
}

bool MapWnd::ZoomToPrevOwnedSystem() {
    // TODO: go through these in some sorted order (the sort method used in the SidePanel system name drop-list)
    std::vector<int> vec = GetUniverse().Objects().FindObjectIDs(OwnedVisitor<System>(HumanClientApp::GetApp()->EmpireID()));
    std::vector<int>::iterator it = std::find(vec.begin(), vec.end(), m_current_owned_system);
    if (it == vec.end()) {
        m_current_owned_system = vec.empty() ? INVALID_OBJECT_ID : vec.back();
    } else {
        m_current_owned_system = it == vec.begin() ? vec.back() : *--it;
    }

    if (m_current_owned_system != INVALID_OBJECT_ID) {
        CenterOnObject(m_current_owned_system);
        SelectSystem(m_current_owned_system);
    }

    return true;
}

bool MapWnd::ZoomToNextOwnedSystem() {
    // TODO: go through these in some sorted order (the sort method used in the SidePanel system name drop-list)
    std::vector<int> vec = GetUniverse().Objects().FindObjectIDs(OwnedVisitor<System>(HumanClientApp::GetApp()->EmpireID()));
    std::vector<int>::iterator it = std::find(vec.begin(), vec.end(), m_current_owned_system);
    if (it == vec.end()) {
        m_current_owned_system = vec.empty() ? INVALID_OBJECT_ID : vec.front();
    } else {
        std::vector<int>::iterator next_it = it;
        ++next_it;
        m_current_owned_system = next_it == vec.end() ? vec.front() : *next_it;
    }

    if (m_current_owned_system != INVALID_OBJECT_ID) {
        CenterOnObject(m_current_owned_system);
        SelectSystem(m_current_owned_system);
    }

    return true;
}

bool MapWnd::ZoomToPrevIdleFleet() {
    std::vector<int> vec = GetUniverse().Objects().FindObjectIDs(StationaryFleetVisitor(HumanClientApp::GetApp()->EmpireID()));
    std::vector<int>::iterator it = std::find(vec.begin(), vec.end(), m_current_fleet_id);
    if (it == vec.end()) {
        m_current_fleet_id = vec.empty() ? INVALID_OBJECT_ID : vec.back();
    } else {
        m_current_fleet_id = it == vec.begin() ? vec.back() : *--it;
    }

    if (m_current_fleet_id != INVALID_OBJECT_ID) {
        CenterOnObject(m_current_fleet_id);
        SelectFleet(m_current_fleet_id);
    }

    return true;
}

bool MapWnd::ZoomToNextIdleFleet() {
    std::vector<int> vec = GetUniverse().Objects().FindObjectIDs(StationaryFleetVisitor(HumanClientApp::GetApp()->EmpireID()));
    std::vector<int>::iterator it = std::find(vec.begin(), vec.end(), m_current_fleet_id);
    if (it == vec.end()) {
        m_current_fleet_id = vec.empty() ? INVALID_OBJECT_ID : vec.front();
    } else {
        std::vector<int>::iterator next_it = it;
        ++next_it;
        m_current_fleet_id = next_it == vec.end() ? vec.front() : *next_it;
    }

    if (m_current_fleet_id != INVALID_OBJECT_ID) {
        CenterOnObject(m_current_fleet_id);
        SelectFleet(m_current_fleet_id);
    }

    return true;
}

bool MapWnd::ZoomToPrevFleet() {
    std::vector<int> vec = GetUniverse().Objects().FindObjectIDs(OwnedVisitor<Fleet>(HumanClientApp::GetApp()->EmpireID()));
    std::vector<int>::iterator it = std::find(vec.begin(), vec.end(), m_current_fleet_id);
    if (it == vec.end()) {
        m_current_fleet_id = vec.empty() ? INVALID_OBJECT_ID : vec.back();
    } else {
        m_current_fleet_id = it == vec.begin() ? vec.back() : *--it;
    }

    if (m_current_fleet_id != INVALID_OBJECT_ID) {
        CenterOnObject(m_current_fleet_id);
        SelectFleet(m_current_fleet_id);
    }

    return true;
}

bool MapWnd::ZoomToNextFleet() {
    std::vector<int> vec = GetUniverse().Objects().FindObjectIDs(OwnedVisitor<Fleet>(HumanClientApp::GetApp()->EmpireID()));
    std::vector<int>::iterator it = std::find(vec.begin(), vec.end(), m_current_fleet_id);
    if (it == vec.end()) {
        m_current_fleet_id = vec.empty() ? INVALID_OBJECT_ID : vec.front();
    } else {
        std::vector<int>::iterator next_it = it;
        ++next_it;
        if (next_it == vec.end())
            m_current_fleet_id = vec.front();
        else
            m_current_fleet_id = *next_it;
    }

    if (m_current_fleet_id != INVALID_OBJECT_ID) {
        CenterOnObject(m_current_fleet_id);
        SelectFleet(m_current_fleet_id);
    }

    return true;
}

bool MapWnd::ZoomToSystemWithWastedPP() {
    int empire_id = HumanClientApp::GetApp()->EmpireID();
    const Empire* empire = HumanClientApp::GetApp()->Empires().Lookup(empire_id);
    if (!empire)
        return false;

    const ProductionQueue& queue = empire->GetProductionQueue();
    const boost::shared_ptr<ResourcePool> pool = empire->GetResourcePool(RE_INDUSTRY);
    if (!pool)
        return false;
    std::set<std::set<int> > wasted_PP_objects(queue.ObjectsWithWastedPP(pool));
    if (wasted_PP_objects.empty())
        return false;

    // pick first object in first group
    const std::set<int>& obj_group = *wasted_PP_objects.begin();
    if (obj_group.empty())
        return false; // shouldn't happen?
    for (std::set<std::set<int> >::const_iterator set_set_it = wasted_PP_objects.begin();
         set_set_it != wasted_PP_objects.end(); ++set_set_it)
    {
        const std::set<int>& objs = *set_set_it;
        for (std::set<int>::const_iterator set_it = objs.begin(); set_it != objs.end(); ++set_it) {
            const UniverseObject* obj = GetUniverseObject(*set_it);
            if (obj && obj->SystemID() != INVALID_OBJECT_ID) {
                // found object with wasted PP that is in a system.  zoom there.
                CenterOnObject(obj->SystemID());
                SelectSystem(obj->SystemID());
                ShowProduction();
                return true;
            }
        }
    }
    return false;
}

void MapWnd::ConnectKeyboardAcceleratorSignals() {
    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_ESCAPE),
                    &MapWnd::ReturnToMap, this));

    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_RETURN),
                    &MapWnd::OpenChatWindow, this));
    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_KP_ENTER),
                    &MapWnd::OpenChatWindow, this));

    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_RETURN,   GG::MOD_KEY_CTRL),
                    &MapWnd::EndTurn, this));
    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_KP_ENTER, GG::MOD_KEY_CTRL),
                    &MapWnd::EndTurn, this));

    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_F2),
                    &MapWnd::ToggleSitRep, this));
    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_F3),
                    &MapWnd::ToggleResearch, this));
    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_F4),
                    &MapWnd::ToggleProduction, this));
    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_F5),
                    &MapWnd::ToggleDesign, this));
    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_F10),
                    &MapWnd::ShowMenu, this));
    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_s),
                    &MapWnd::CloseSystemView, this));

    // Keys for zooming
    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_e),
                    &MapWnd::KeyboardZoomIn, this));
    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_KP_PLUS),
                    &MapWnd::KeyboardZoomIn, this));
    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_r),
                    &MapWnd::KeyboardZoomOut, this));
    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_KP_MINUS),
                    &MapWnd::KeyboardZoomOut, this));

    // Keys for showing systems
    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_d),
                    &MapWnd::ZoomToHomeSystem, this));
    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_x),
                    &MapWnd::ZoomToPrevOwnedSystem, this));
    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_c),
                    &MapWnd::ZoomToNextOwnedSystem, this));

    // Keys for showing fleets
    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_f),
                    &MapWnd::ZoomToPrevIdleFleet, this));
    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_g),
                    &MapWnd::ZoomToNextIdleFleet, this));
    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_v),
                    &MapWnd::ZoomToPrevFleet, this));
    m_keyboard_accelerator_signals.insert(
        GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_b),
                    &MapWnd::ZoomToNextFleet, this));
}

void MapWnd::DisconnectKeyboardAcceleratorSignals() {
    for (std::set<boost::signals::connection>::iterator it = m_keyboard_accelerator_signals.begin();
         it != m_keyboard_accelerator_signals.end(); ++it)
    { it->disconnect(); }
    m_keyboard_accelerator_signals.clear();
}

void MapWnd::SetAccelerators() {
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_ESCAPE);

    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_RETURN);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_KP_ENTER);

    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_RETURN, GG::MOD_KEY_CTRL);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_KP_ENTER, GG::MOD_KEY_CTRL);

    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_F2);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_F3);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_F4);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_F5);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_F10);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_s);

    // Keys for zooming
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_e);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_r);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_KP_PLUS);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_KP_MINUS);

    // Keys for showing systems
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_d);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_x);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_c);

    // Keys for showing fleets
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_f);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_g);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_v);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_b);
}

void MapWnd::DisableAlphaNumAccels() {
    for (GG::GUI::const_accel_iterator i = GG::GUI::GetGUI()->accel_begin();
         i != GG::GUI::GetGUI()->accel_end(); ++i) {
        if (i->second != 0) // we only want to disable mod_keys without modifiers
            continue; 
        GG::Key key = i->first;
        if ((key >= GG::GGK_a && key <= GG::GGK_z) || 
            (key >= GG::GGK_0 && key <= GG::GGK_9)) {
            m_disabled_accels_list.insert(key);
        }
        m_disabled_accels_list.insert(GG::GGK_KP_ENTER);
        m_disabled_accels_list.insert(GG::GGK_RETURN);
    }
    for (std::set<GG::Key>::iterator i = m_disabled_accels_list.begin();
         i != m_disabled_accels_list.end(); ++i) {
        GG::GUI::GetGUI()->RemoveAccelerator(*i);
    }
}

void MapWnd::EnableAlphaNumAccels() {
    for (std::set<GG::Key>::iterator i = m_disabled_accels_list.begin();
         i != m_disabled_accels_list.end(); ++i) {
        GG::GUI::GetGUI()->SetAccelerator(*i);
    }
    m_disabled_accels_list.clear();
}

void MapWnd::ChatMessageSentSlot()
{}

void MapWnd::CloseAllPopups() {
    for (std::list<MapWndPopup*>::iterator it = m_popups.begin(); it != m_popups.end(); ) {
        // get popup and increment iterator first since closing the popup will change this list by removing the popup
        MapWndPopup* popup = *it++;
        popup->Close();
    }
    // clear list
    m_popups.clear();
}

void MapWnd::HideAllPopups() {
    for (std::list<MapWndPopup*>::iterator it = m_popups.begin(); it != m_popups.end(); ++it) {
        (*it)->Hide();
    }
}

void MapWnd::SetFleetExploring(const int fleet_id){
    std::set<int>::iterator it = std::find(m_fleets_exploring.begin(), m_fleets_exploring.end(), fleet_id);
    if(it == m_fleets_exploring.end()){ //this fleet is not currently exploring
        m_fleets_exploring.insert(fleet_id);
        DispatchFleetsExploring();
    }
}

void MapWnd::StopFleetExploring(const int fleet_id){
    m_fleets_exploring.erase(fleet_id);
    DispatchFleetsExploring();
}

bool MapWnd::IsFleetExploring(const int fleet_id){
    std::set<int>::iterator it;
    it = std::find(m_fleets_exploring.begin(), m_fleets_exploring.end(), fleet_id);
    return it != m_fleets_exploring.end();
}

namespace { //helper function for DispatchFleetsExploring
    //return the set of all systems ID with a starlane connecting them to a system in set
    std::set<int> AddNeighboorsToSet(const Empire *empire, const std::set<int> set){
        std::set<int> retval;
        std::map<int, std::set<int> > starlanes = empire->KnownStarlanes();
        for(std::set<int>::iterator el = set.begin(); el != set.end(); el ++){ //for all elements of the set
            std::map<int, std::set<int> >::iterator new_neighboors_it = starlanes.find(*el);
            if(new_neighboors_it != starlanes.end()){
                std::set<int> new_neighboors = new_neighboors_it->second;
                for(std::set<int>::iterator it = new_neighboors.begin(); it != new_neighboors.end(); it ++){ //for all neighboors of this element
                    retval.insert(*it);
                }
            }
        }

        return retval;
    }

    //return the pair (systemID, dist) of the closest supply point.
    std::pair<int, int> GetNearestSupplyPoint(const Empire *empire, int system_id){
        std::set<int> supplyable_systems = empire->FleetSupplyableSystemIDs();
        std::map<int, std::set<int> > starlanes = empire->KnownStarlanes();
        std::set<int> frontier;
        frontier.insert(system_id);
        int distance = 0;
        bool found = false;
        while(distance < 50 && !found){ //assume 50 is an upperbound an the max fuel limit or the distance to a supply system. TODO : #define it
            for(std::set<int>::iterator sys = frontier.begin(); sys != frontier.end() && !found; sys ++){
                if(supplyable_systems.count(*sys) > 0){
                    //we found a route to a supplyable system
                    return std::pair<int, int>(*sys, distance);
                }
             }
             distance ++;
             frontier = AddNeighboorsToSet(empire, frontier);
        }

        return std::pair<int, int>(INVALID_OBJECT_ID, INT_MAX);
    }
};

void MapWnd::DispatchFleetsExploring() {
    Logger().debugStream() << "MapWnd::DispatchFleetsExploring called";

    int empire_id = HumanClientApp::GetApp()->EmpireID();
    const Empire *empire = HumanClientApp::GetApp()->Empires().Lookup(empire_id);
    if (!empire) return;
    const std::set<int> destroyed_objects = GetUniverse().EmpireKnownDestroyedObjectIDs(empire_id);

    //int nbr_ship_idle = 0;
    std::set<int> fleet_idle;
    std::set<int> systems_being_explored; //all systems ID for which an exploring fleet is in route

    //clean the fleet list by removing non-existing fleet, and extract the fleets waiting for orders
    for (std::set<int>::iterator it = m_fleets_exploring.begin(); it != m_fleets_exploring.end();) {
        Fleet *fleet = GetFleet(*it);
        if (!fleet || destroyed_objects.find(fleet->ID()) != destroyed_objects.end()) {
            m_fleets_exploring.erase(it++); //this fleet can't explore anymore
        } else {
             if (fleet->MovePath().empty())
                fleet_idle.insert(fleet->ID());
            else
                systems_being_explored.insert(fleet->FinalDestinationID());
            it++;
        }
    }

    if (fleet_idle.empty())
        return;

    Logger().debugStream() << "MapWnd::DispatchFleetsExploring There is " << fleet_idle.size() << "ships to dispatch";

    //list all unexplored systems by taking the neighboors of explored systems because ObjectMap does not list them all.
    std::set<int> candidates_unknown_systems;
    std::set<int> explored_systems =  empire->ExploredSystems();
    candidates_unknown_systems = AddNeighboorsToSet(empire, explored_systems);
    std::set<int> neighboors = AddNeighboorsToSet(empire, candidates_unknown_systems);
    candidates_unknown_systems.insert(neighboors.begin(), neighboors.end());

    //list all unknow systems with the distance to the nearest supply available
    std::map<int, int> unknown_systems;
    std::set<int> supplyable_systems = empire->FleetSupplyableSystemIDs();
    for (std::set<int>::iterator it = candidates_unknown_systems.begin(); it != candidates_unknown_systems.end(); it ++) {
        if (System* system = GetSystem(*it)){
            if (!empire->HasExploredSystem(system->ID()) && systems_being_explored.find(*it) == systems_being_explored.end()) {
                //we compute the minimum distance to find a supplyable system
                std::pair<int, int> pair = GetNearestSupplyPoint(empire, system->ID());
                if (pair.first != INVALID_OBJECT_ID) {
                    unknown_systems[system->ID()] = pair.second;
                }
            }
        }
    }

    Logger().debugStream() << "MapWnd::DispatchFleetsExploring There is " << unknown_systems.size() << "unknown systems";

    //for now, send each ship to the nearest unexplored system where no other ship has been ordered so far
    std::set<int> systems_order_sent; //list all systems ID for which a ship was sent this turn
    int nbr_fleet_to_send = fleet_idle.size();
    bool remaining_system_to_explore = true;
    for (int i = 0; i < nbr_fleet_to_send; i++){ //at each iteration, we should send one ship on its way

        double min_dist = DBL_MAX;
        int end_system_id = INVALID_OBJECT_ID;
        int start_system_id = INVALID_OBJECT_ID;
        int last_visibility = NUM_VISIBILITIES; //greater than max visibility
        int better_fleet_id;

        for (std::set<int>::iterator it = fleet_idle.begin(); it != fleet_idle.end(); it ++){
            Fleet *fleet = GetFleet(*it);
            if (fleet && fleet->MovePath().empty()) {

                double far_min_dist = DBL_MAX;
                int far_system_id; //id of the closest unknown system without taking fuel into account

                for (std::map<int, int>::iterator system_it = unknown_systems.begin(); system_it != unknown_systems.end(); system_it ++) {
                    if (systems_order_sent.find(system_it->first) != systems_order_sent.end())
                        continue; //someone already went there this turn

                    std::pair<std::list<int>, double> pair = GetUniverse().ShortestPath(fleet->SystemID(), system_it->first, empire_id);

                    //we check for the fuel.
                    bool is_doable_for_fuel = true;
                    std::list<int> route = pair.first;
                    double current_fuel = fleet->Fuel();
                    for (std::list<int>::iterator route_it = ++(route.begin()); route_it != route.end(); route_it ++) {
                        if (supplyable_systems.count(*route_it) > 0) {
                            if (fleet->Fuel() != fleet->MaxFuel()) {
                                is_doable_for_fuel = false; //if we need to ressupply, do it the first time we enter the empire. If we are full, we can cross it.
                            }
                        } else {
                            current_fuel --;
                        }
                        if (current_fuel < 0) {
                            is_doable_for_fuel = false;
                        }
                    }

                    if (current_fuel < system_it->second)
                        is_doable_for_fuel = false;

                    int vis = GetUniverse().GetObjectVisibilityByEmpire(system_it->first, empire_id);
                    if (vis == VIS_NO_VISIBILITY) vis = VIS_BASIC_VISIBILITY; //those two levels of visibility appears to be identical for a system

                    if (((pair.second < min_dist && vis <= last_visibility) || vis < last_visibility) && is_doable_for_fuel) { //we can explore this system
                        min_dist = pair.second;
                        end_system_id = system_it->first;
                        last_visibility = vis;
                        better_fleet_id = fleet->ID();
                        start_system_id = fleet->SystemID();
                    }

                    if (pair.second < far_min_dist) { //we can explore this system
                        far_min_dist = pair.second;
                        far_system_id = system_it->first;
                    }
                }

                if (!remaining_system_to_explore || min_dist == DBL_MAX) {
                    if (fleet->Fuel() == fleet->MaxFuel() && far_min_dist != DBL_MAX) {
                        //we have full fuel and no unknown planet in range. We can go to a far system, but we will have to wait for resupply
                        Logger().debugStream() << "MapWnd::DispatchFleetsExploring : Next system for fleet " << fleet->ID() << " is " << far_system_id << ". Not enough fuel for the round trip";
                        systems_order_sent.insert(far_system_id);
                        HumanClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new FleetMoveOrder(empire_id, fleet->ID(), fleet->SystemID(), far_system_id)));
                    } else {
                        //no unknown planet in range. Let's try to get home to resupply
                        std::pair<int, int> pair = GetNearestSupplyPoint(empire, fleet->SystemID());
                        Logger().debugStream() << "MapWnd::DispatchFleetsExploring : Fleet " << fleet->ID() << " going to resupply at " << pair.first;
                        HumanClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new FleetMoveOrder(empire_id, fleet->ID(), fleet->SystemID(), pair.first)));
                    }
                    i = nbr_fleet_to_send; //stop the loop since every fleet will have order
                }
            }
        }

        if (min_dist != DBL_MAX) {
            //there is an unexplored system rechable
            Logger().debugStream() << "MapWnd::DispatchFleetsExploring : Next system for fleet " << better_fleet_id << " is " << end_system_id;
            systems_order_sent.insert(end_system_id);
            fleet_idle.erase(better_fleet_id);
            HumanClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new FleetMoveOrder(empire_id, better_fleet_id, start_system_id, end_system_id)));
        } else {
            remaining_system_to_explore = false; //from now on, each ship will be sent to a supply depot or a far system
        }
    }
}

void MapWnd::ShowAllPopups() {
    for (std::list<MapWndPopup*>::iterator it = m_popups.begin(); it != m_popups.end(); ++it) {
        (*it)->Show();
    }
}

void MapWnd::ModeratorNoActionSelected() {
}

void MapWnd::ModeratorCreateSystemSelected(StarType star_type) {
}

void MapWnd::ModeratorCreatePlanetSelected(PlanetType planet_type) {
}

void MapWnd::ModeratorDeleteObjectSelected() {
}

void MapWnd::ModeratorSetOwnerSelected(int owner_id) {
}

void MapWnd::ModeratorCreateStarlaneSelected() {
}

