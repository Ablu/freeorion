#ifdef FREEORION_WIN32
#include <GL/glew.h>
#endif

#include "SidePanel.h"

#include "CUIWnd.h"
#include "CUIControls.h"
#include "SystemIcon.h"
#include "Sound.h"
#include "FleetWnd.h"
#include "InfoPanels.h"
#include "MapWnd.h"
#include "ShaderProgram.h"
#include "../universe/Predicates.h"
#include "../universe/ShipDesign.h"
#include "../universe/Fleet.h"
#include "../universe/Ship.h"
#include "../universe/Building.h"
#include "../universe/Species.h"
#include "../Empire/Empire.h"
#include "../util/Directories.h"
#include "../util/MultiplayerCommon.h"
#include "../util/Random.h"
#include "../util/XMLDoc.h"
#include "../util/OptionsDB.h"
#include "../client/human/HumanClientApp.h"

#include <GG/DrawUtil.h>
#include <GG/StaticGraphic.h>
#include <GG/DynamicGraphic.h>
#include <GG/Scroll.h>
#include <GG/dialogs/ThreeButtonDlg.h>

#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <boost/filesystem/fstream.hpp>

namespace {
    const int       EDGE_PAD(3);
    const double    TWO_PI(2.0*3.1415926536);

    void        PlaySidePanelOpenSound()       {Sound::GetSound().PlaySound(GetOptionsDB().Get<std::string>("UI.sound.sidepanel-open"), true);}
    void        PlayFarmingFocusClickSound()   {Sound::GetSound().PlaySound(GetOptionsDB().Get<std::string>("UI.sound.farming-focus"), true);}
    void        PlayIndustryFocusClickSound()  {Sound::GetSound().PlaySound(GetOptionsDB().Get<std::string>("UI.sound.industry-focus"), true);}
    void        PlayResearchFocusClickSound()  {Sound::GetSound().PlaySound(GetOptionsDB().Get<std::string>("UI.sound.research-focus"), true);}
    void        PlayMiningFocusClickSound()    {Sound::GetSound().PlaySound(GetOptionsDB().Get<std::string>("UI.sound.mining-focus"), true);}
    void        PlayTradeFocusClickSound()     {Sound::GetSound().PlaySound(GetOptionsDB().Get<std::string>("UI.sound.trade-focus"), true);}
    void        PlayBalancedFocusClickSound()  {Sound::GetSound().PlaySound(GetOptionsDB().Get<std::string>("UI.sound.balanced-focus"), true);}

    struct RotatingPlanetData {
        RotatingPlanetData(const XMLElement& elem) {
            if (elem.Tag() != "RotatingPlanetData")
                throw std::invalid_argument("Attempted to construct a RotatingPlanetData from an XMLElement that had a tag other than \"RotatingPlanetData\"");

            planet_type = boost::lexical_cast<PlanetType>(elem.Child("planet_type").Text());
            filename = elem.Child("filename").Text();
            shininess = boost::lexical_cast<double>(elem.Child("shininess").Text());

            // ensure proper bounds
            shininess = std::max(0.0, std::min(shininess, 128.0));
        }

        XMLElement  XMLEncode() const {
            XMLElement retval("RotatingPlanetData");
            retval.AppendChild(XMLElement("planet_type", boost::lexical_cast<std::string>(planet_type)));
            retval.AppendChild(XMLElement("filename", filename));
            retval.AppendChild(XMLElement("shininess", boost::lexical_cast<std::string>(shininess)));
            return retval;
        }

        PlanetType  planet_type;    ///< the type of planet for which this data may be used
        std::string filename;       ///< the filename of the image used to texture a rotating image
        double      shininess;      ///< the exponent of specular (shiny) reflection off of the planet; must be in [0.0, 128.0]
    };

    struct PlanetAtmosphereData {
        struct Atmosphere {
            Atmosphere() {}
            Atmosphere(const XMLElement& elem) {
                if (elem.Tag() != "Atmosphere")
                    throw std::invalid_argument("Attempted to construct an Atmosphere from an XMLElement that had a tag other than \"Atmosphere\"");
                filename = elem.Child("filename").Text();
                alpha = boost::lexical_cast<int>(elem.Child("alpha").Text());
                alpha = std::max(0, std::min(alpha, 255));
            }

            std::string filename;
            int         alpha;
        };

        PlanetAtmosphereData() {}
        PlanetAtmosphereData(const XMLElement& elem) {
            if (elem.Tag() != "PlanetAtmosphereData")
                throw std::invalid_argument("Attempted to construct a PlanetAtmosphereData from an XMLElement that had a tag other than \"PlanetAtmosphereData\"");
            planet_filename = elem.Child("planet_filename").Text();
            const XMLElement& atmospheres_elem = elem.Child("atmospheres");
            for (XMLElement::const_child_iterator it = atmospheres_elem.child_begin(); it != atmospheres_elem.child_end(); ++it) {
                atmospheres.push_back(Atmosphere(*it));
            }
        }

        std::string             planet_filename; ///< the filename of the planet image that this atmosphere image data goes with
        std::vector<Atmosphere> atmospheres;     ///< the filenames of the atmosphere images suitable for use with this planet image
    };

    const std::map<PlanetType, std::vector<RotatingPlanetData> >&   GetRotatingPlanetData() {
        ScopedTimer timer("GetRotatingPlanetData", true);
        static std::map<PlanetType, std::vector<RotatingPlanetData> > data;
        if (data.empty()) {
            XMLDoc doc;
            try {
                boost::filesystem::ifstream ifs(ClientUI::ArtDir() / "planets" / "planets.xml");
                doc.ReadDoc(ifs);
                ifs.close();
            } catch (const std::exception& e) {
                Logger().errorStream() << "GetRotatingPlanetData: error reading artdir/planets/planets.xml: " << e.what();
            }

            if (doc.root_node.ContainsChild("GLPlanets")) {
                const XMLElement& elem = doc.root_node.Child("GLPlanets");
                for (XMLElement::const_child_iterator it = elem.child_begin(); it != elem.child_end(); ++it) {
                    if (it->Tag() == "RotatingPlanetData") {
                        RotatingPlanetData current_data(*it);
                        data[current_data.planet_type].push_back(current_data);
                    }
                }
            }
        }
        return data;
    }

    const std::map<std::string, PlanetAtmosphereData>&              GetPlanetAtmosphereData() {
        static std::map<std::string, PlanetAtmosphereData> data;
        if (data.empty()) {
            XMLDoc doc;
            boost::filesystem::ifstream ifs(ClientUI::ArtDir() / "planets" / "atmospheres.xml");
            doc.ReadDoc(ifs);
            ifs.close();

            for (XMLElement::const_child_iterator it = doc.root_node.child_begin(); it != doc.root_node.child_end(); ++it) {
                if (it->Tag() == "PlanetAtmosphereData") {
                    PlanetAtmosphereData current_data(*it);
                    data[current_data.planet_filename] = current_data;
                }
            }
        }
        return data;
    }

    double      GetAsteroidsFPS() {
        static double retval = -1.0;
        if (retval == -1.0) {
            XMLDoc doc;
            boost::filesystem::ifstream ifs(ClientUI::ArtDir() / "planets" / "planets.xml");
            doc.ReadDoc(ifs);
            ifs.close();

            if (doc.root_node.ContainsChild("asteroids_fps"))
                retval = boost::lexical_cast<double>(doc.root_node.Child("asteroids_fps").Text());
            else
                retval = 15.0;

            retval = std::max(0.0, std::min(retval, 60.0));
        }
        return retval;
    }

    double      GetRotatingPlanetAmbientIntensity() {
        static double retval = -1.0;

        if (retval == -1.0) {
            XMLDoc doc;
            boost::filesystem::ifstream ifs(ClientUI::ArtDir() / "planets" / "planets.xml");
            doc.ReadDoc(ifs);
            ifs.close();

            if (doc.root_node.ContainsChild("GLPlanets") && doc.root_node.Child("GLPlanets").ContainsChild("ambient_intensity"))
                retval = boost::lexical_cast<double>(doc.root_node.Child("GLPlanets").Child("ambient_intensity").Text());
            else
                retval = 0.5;

            retval = std::max(0.0, std::min(retval, 1.0));
        }

        return retval;
    }

    double      GetRotatingPlanetDiffuseIntensity() {
        static double retval = -1.0;

        if (retval == -1.0) {
            XMLDoc doc;
            boost::filesystem::ifstream ifs(ClientUI::ArtDir() / "planets" / "planets.xml");
            doc.ReadDoc(ifs);
            ifs.close();

            if (doc.root_node.ContainsChild("GLPlanets") && doc.root_node.Child("GLPlanets").ContainsChild("diffuse_intensity"))
                retval = boost::lexical_cast<double>(doc.root_node.Child("GLPlanets").Child("diffuse_intensity").Text());
            else
                retval = 0.5;

            retval = std::max(0.0, std::min(retval, 1.0));
        }

        return retval;
    }

    void        RenderSphere(double r, const GG::Clr& ambient, const GG::Clr& diffuse,
                             const GG::Clr& spec, double shine,
                             boost::shared_ptr<GG::Texture> texture)
    {
        static GLUquadric* quad = gluNewQuadric();
        if (!quad)
            return;

        if (texture) {
            glBindTexture(GL_TEXTURE_2D, texture->OpenGLId());
        }

        // commented out shininess rendering because it wasn't working properly.
        // it just appeared as a white blob, seemingly at the poles of the planet (but possibly not?)
        // regardless, IMO it didn't look good. -Geoff
        //if (shine) {
        //    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, static_cast<float>(shine));
        //    GLfloat spec_v[] = {spec.r / 255.0f, spec.g / 255.0f, spec.b / 255.0f, spec.a / 255.0f};
        //    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec_v);
        //}

        GLfloat ambient_v[] = {ambient.r / 255.0f, ambient.g / 255.0f, ambient.b / 255.0f, ambient.a / 255.0f};
        glMaterialfv(GL_FRONT, GL_AMBIENT, ambient_v);
        GLfloat diffuse_v[] = {diffuse.r / 255.0f, diffuse.g / 255.0f, diffuse.b / 255.0f, diffuse.a / 255.0f};
        glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse_v);

        gluQuadricTexture(quad,     texture ? GL_TRUE : GL_FALSE);
        gluQuadricNormals(quad,     GLU_SMOOTH);
        gluQuadricOrientation(quad, GLU_OUTSIDE);
        gluQuadricDrawStyle(quad,   GLU_FILL);

        glColor(GG::CLR_WHITE);

        gluSphere(quad, r, 30, 30);
    }

    GLfloat*    GetLightPosition() {
        static GLfloat retval[] = {0.0, 0.0, 0.0, 0.0};

        if (retval[0] == 0.0 && retval[1] == 0.0 && retval[2] == 0.0) {
            XMLDoc doc;
            boost::filesystem::ifstream ifs(ClientUI::ArtDir() / "planets" / "planets.xml");
            doc.ReadDoc(ifs);
            ifs.close();

            retval[0] = boost::lexical_cast<GLfloat>(doc.root_node.Child("GLPlanets").Child("light_pos").Child("x").Text());
            retval[1] = boost::lexical_cast<GLfloat>(doc.root_node.Child("GLPlanets").Child("light_pos").Child("y").Text());
            retval[2] = boost::lexical_cast<GLfloat>(doc.root_node.Child("GLPlanets").Child("light_pos").Child("z").Text());
        }

        return retval;
    }

    const std::map<StarType, std::vector<float> >& GetStarLightColors() {
        static std::map<StarType, std::vector<float> > light_colors;

        if (light_colors.empty()) {
            XMLDoc doc;
            boost::filesystem::ifstream ifs(ClientUI::ArtDir() / "planets" / "planets.xml");
            doc.ReadDoc(ifs);
            ifs.close();

            if (doc.root_node.ContainsChild("GLStars") && 0 < doc.root_node.Child("GLStars").NumChildren()) {
                for (XMLElement::child_iterator it = doc.root_node.Child("GLStars").child_begin(); it != doc.root_node.Child("GLStars").child_end(); ++it) {
                    std::vector<float>& color_vec = light_colors[boost::lexical_cast<StarType>(it->Child("star_type").Text())];
                    GG::Clr color(XMLToClr(it->Child("GG::Clr")));
                    color_vec.push_back(color.r / 255.0f);
                    color_vec.push_back(color.g / 255.0f);
                    color_vec.push_back(color.b / 255.0f);
                    color_vec.push_back(color.a / 255.0f);
                }
            } else {
                for (int i = STAR_BLUE; i < NUM_STAR_TYPES; ++i) {
                    light_colors[StarType(i)].resize(4, 1.0);
                }
            }
        }

        return light_colors;
    }

    const std::vector<float>& StarLightColour(StarType star_type) {
        static std::vector<float> white(4, 0.0f);
        const std::map<StarType, std::vector<float> >& colour_map = GetStarLightColors();
        std::map<StarType, std::vector<float> >::const_iterator it = colour_map.find(star_type);
        if (it != colour_map.end())
            return it->second;
        return white;
    }

    void        RenderPlanet(const GG::Pt& center, int diameter, boost::shared_ptr<GG::Texture> texture,
                             boost::shared_ptr<GG::Texture> overlay_texture,
                             double initial_rotation, double RPM, double axial_tilt, double shininess,
                             StarType star_type)
    {
        glPushAttrib(GL_ENABLE_BIT | GL_PIXEL_MODE_BIT | GL_TEXTURE_BIT | GL_SCISSOR_BIT);
        HumanClientApp::GetApp()->Exit2DMode();

        // slide the texture coords to simulate a rotating axis
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glTranslated(initial_rotation - GG::GUI::GetGUI()->Ticks() / 1000.0 * RPM / 60.0, 0.0, 0.0);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, Value(HumanClientApp::GetApp()->AppWidth()),
                Value(HumanClientApp::GetApp()->AppHeight()), 0.0,
                0.0, Value(HumanClientApp::GetApp()->AppWidth()));

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glPushAttrib(GL_LIGHTING_BIT | GL_ENABLE_BIT);
        GLfloat* light_position = GetLightPosition();
        glLightfv(GL_LIGHT0, GL_POSITION, light_position);
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);

        const std::vector<float>& colour = StarLightColour(star_type);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, &colour[0]);
        glLightfv(GL_LIGHT0, GL_SPECULAR, &colour[0]);
        glEnable(GL_TEXTURE_2D);

        glTranslated(Value(center.x), Value(center.y), -(diameter / 2 + 1));
        glRotated(100.0, -1.0, 0.0, 0.0); // make the poles upright, instead of head-on (we go a bit more than 90 degrees, to avoid some artifacting caused by the GLU-supplied texture coords)
        glRotated(axial_tilt, 0.0, 1.0, 0.0);  // axial tilt
        float intensity = static_cast<float>(GetRotatingPlanetAmbientIntensity());
        GG::Clr ambient = GG::FloatClr(intensity, intensity, intensity, 1.0f);
        intensity = static_cast<float>(GetRotatingPlanetDiffuseIntensity());
        GG::Clr diffuse = GG::FloatClr(intensity, intensity, intensity, 1.0f);

        RenderSphere(diameter / 2, ambient, diffuse, GG::CLR_WHITE, shininess, texture);
        if (overlay_texture) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glCullFace(GL_FRONT);
            glEnable(GL_CULL_FACE);
            RenderSphere(diameter / 2 + 0.1, ambient, diffuse, GG::CLR_WHITE, 0.0, overlay_texture);
            glDisable(GL_CULL_FACE);
            glDisable(GL_BLEND);
        }

        glPopAttrib();

        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);

        HumanClientApp::GetApp()->Enter2DMode();
        glPopAttrib();
    }

    int         MaxPlanetDiameter()
    { return GetOptionsDB().Get<int>("UI.sidepanel-planet-max-diameter"); }

    int         PlanetDiameter(PlanetSize size) {
        double scale = 0.0;
        switch (size) {
        case SZ_TINY      : scale = 1.0/7.0; break;
        case SZ_SMALL     : scale = 2.0/7.0; break;
        case SZ_MEDIUM    : scale = 3.0/7.0; break;
        case SZ_LARGE     : scale = 4.0/7.0; break;
        case SZ_HUGE      : scale = 5.0/7.0; break;
        case SZ_GASGIANT  : scale = 7.0/7.0; break;
        case SZ_ASTEROIDS : scale = 7.0/7.0; break;
        default           : scale = 3.0/7.0; break;
        }

        int MIN_PLANET_DIAMETER = GetOptionsDB().Get<int>("UI.sidepanel-planet-min-diameter");
        // sanity check
        if (MIN_PLANET_DIAMETER > MaxPlanetDiameter())
            MIN_PLANET_DIAMETER = MaxPlanetDiameter();

        return static_cast<int>(MIN_PLANET_DIAMETER + (MaxPlanetDiameter() - MIN_PLANET_DIAMETER) * scale) - 2 * EDGE_PAD;
    }

    /** Adds options related to sidepanel to Options DB. */
    void        AddOptions(OptionsDB& db) {
        db.Add("UI.sidepanel-width",                "OPTIONS_DB_UI_SIDEPANEL_WIDTH",                384,    RangedValidator<int>(64, 512));
        db.Add("UI.sidepanel-planet-max-diameter",  "OPTIONS_DB_UI_SIDEPANEL_PLANET_MAX_DIAMETER",  128,    RangedValidator<int>(16, 512));
        db.Add("UI.sidepanel-planet-min-diameter",  "OPTIONS_DB_UI_SIDEPANEL_PLANET_MIN_DIAMETER",  24,     RangedValidator<int>(8,  128));
        db.Add("UI.sidepanel-planet-shown",         "OPTIONS_DB_UI_SIDEPANEL_PLANET_SHOWN",         true,   Validator<bool>());
    }
    bool temp_bool = RegisterOptions(&AddOptions);

    /** Returns map from planet ID to issued colonize orders affecting it. There
      * should be only one ship colonzing each planet for this client. */
    std::map<int, int> PendingColonizationOrders() {
        std::map<int, int> retval;
        const ClientApp* app = ClientApp::GetApp();
        if (!app)
            return retval;
        const OrderSet& orders = app->Orders();
        for (OrderSet::const_iterator it = orders.begin(); it != orders.end(); ++it) {
            if (boost::shared_ptr<ColonizeOrder> order = boost::dynamic_pointer_cast<ColonizeOrder>(it->second)) {
                retval[order->PlanetID()] = it->first;
            }
        }
        return retval;
    }

    /** Returns map from planet ID to issued invasion orders affecting it. There
      * may be multiple ships invading a single planet. */
    std::map<int, std::set<int> > PendingInvadeOrders() {
        std::map<int, std::set<int> > retval;
        const ClientApp* app = ClientApp::GetApp();
        if (!app)
            return retval;
        const OrderSet& orders = app->Orders();
        for (OrderSet::const_iterator it = orders.begin(); it != orders.end(); ++it) {
            if (boost::shared_ptr<InvadeOrder> order = boost::dynamic_pointer_cast<InvadeOrder>(it->second)) {
                retval[order->PlanetID()].insert(it->first);
            }
        }
        return retval;
    }

    /** Returns map from object ID to issued colonize orders affecting it. */
    std::map<int, int> PendingScrapOrders() {
        std::map<int, int> retval;
        const ClientApp* app = ClientApp::GetApp();
        if (!app)
            return retval;
        const OrderSet& orders = app->Orders();
        for (OrderSet::const_iterator it = orders.begin(); it != orders.end(); ++it) {
            if (boost::shared_ptr<ScrapOrder> order = boost::dynamic_pointer_cast<ScrapOrder>(it->second)) {
                retval[order->ObjectID()] = it->first;
            }
        }
        return retval;
    }
}


/** A single planet's info and controls; several of these may appear at any
  * one time in a SidePanel */
class SidePanel::PlanetPanel : public GG::Control {
public:
    /** \name Structors */ //@{
    PlanetPanel(GG::X w, int planet_id, StarType star_type); ///< basic ctor
    ~PlanetPanel();
    //@}

    /** \name Accessors */ //@{
    virtual bool            InWindow(const GG::Pt& pt) const;
    int                     PlanetID() const { return m_planet_id; }
    //@}

    /** \name Mutators */ //@{
    void                    Select(bool selected);

    virtual void            Render();
    virtual void            LClick(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
    virtual void            RClick(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
    virtual void            MouseWheel(const GG::Pt& pt, int move, GG::Flags<GG::ModKey> mod_keys);

    virtual void            SizeMove(const GG::Pt& ul, const GG::Pt& lr);

    void                    Refresh();                      ///< updates panels, shows / hides colonize button, redoes layout of infopanels

    /** Enables, or disables if \a enable is false, issuing orders via this PlanetPanel. */
    void            EnableOrderIssuing(bool enable = true);
    //@}

    mutable boost::signal<void (int)>               LClickedSignal;     ///< emitted when the planet panel is left clicked by the user.  returns the id of the clicked planet
    mutable boost::signal<void ()>                  ResizedSignal;      ///< emitted when resized, so external container can redo layout
    mutable boost::signal<void (const std::string&)>FocusChangedSignal; ///< emitted when focus is changed

private:
    void                    DoLayout();
    void                    CheckDisplayPlanets();
    void                    SetFocus(const std::string& focus); ///< set the focus of the planet to \a focus
    void                    ClickColonize();                    ///< called if colonize button is pressed
    void                    ClickInvade();                      ///< called if invade button is pressed

    void                    FocusDropListSelectionChanged(GG::DropDownList::iterator selected); ///< called when droplist selection changes, emits FocusChangedSignal

    int                     m_planet_id;                ///< id for the planet with is represented by this planet panel
    GG::TextControl*        m_planet_name;              ///< planet name
    GG::TextControl*        m_env_size;                 ///< indicates size and planet environment rating uncolonized planets
    CUIButton*              m_colonize_button;          ///< btn which can be pressed to colonize this planet
    CUIButton*              m_invade_button;            ///< btn which can be pressed to invade this planet
    GG::DynamicGraphic*     m_planet_graphic;           ///< image of the planet (can be a frameset); this is now used only for asteroids
    RotatingPlanetControl*  m_rotating_planet_graphic;  ///< a realtime-rendered planet that rotates, with a textured surface mapped onto it
    bool                    m_selected;                 ///< is this planet panel selected
    bool                    m_order_issuing_enabled;    ///< can orders be issues via this planet panel?
    GG::Clr                 m_empire_colour;            ///< colour to use for empire-specific highlighting.  set based on ownership of planet.
    CUIDropDownList*        m_focus_drop;               ///< displays and allows selection of planetary focus
    PopulationPanel*        m_population_panel;         ///< contains info about population and health
    ResourcePanel*          m_resource_panel;           ///< contains info about resources production and focus selection UI
    MilitaryPanel*          m_military_panel;           ///< contains icons representing military-related meters
    BuildingsPanel*         m_buildings_panel;          ///< contains icons representing buildings
    SpecialsPanel*          m_specials_panel;           ///< contains icons representing specials
    StarType                m_star_type;

    boost::signals::connection  m_planet_connection;
};

/** Container class that holds PlanetPanels.  Creates and destroys PlanetPanel
  * as necessary, and does layout of them after creation and in response to
  * scrolling through them by the user. */
class SidePanel::PlanetPanelContainer : public GG::Wnd {
public:
    /** \name Structors */ //@{
    PlanetPanelContainer(GG::X x, GG::Y y, GG::X w, GG::Y h);
    ~PlanetPanelContainer();
    //@}

    /** \name Accessors */ //@{
    virtual bool    InWindow(const GG::Pt& pt) const;
    virtual void    MouseWheel(const GG::Pt& pt, int move, GG::Flags<GG::ModKey> mod_keys);  ///< respond to movement of the mouse wheel (move > 0 indicates the wheel is rolled up, < 0 indicates down)

    int                     SelectedPlanetID() const    {return m_selected_planet_id;}
    const std::set<int>&    SelectionCandidates() const {return m_candidate_ids;}
    int                     ScrollPosition() const;
    //@}

    /** \name Mutators */ //@{
    virtual void    LDrag(const GG::Pt& pt, const GG::Pt& move, GG::Flags<GG::ModKey> mod_keys);

    void            Clear();
    void            SetPlanets(const std::vector<int>& planet_ids, StarType star_type);
    void            SelectPlanet(int planet_id);        //!< programatically selects a planet with id \a planet_id
    void            SetValidSelectionPredicate(const boost::shared_ptr<UniverseObjectVisitor> &visitor);
    void            ScrollTo(int pos);

    void            RefreshAllPlanetPanels();           //!< updates data displayed in info panels and redoes layout

    virtual void    SizeMove(const GG::Pt& ul, const GG::Pt& lr);

    /** Enables, or disables if \a enable is false, issuing orders via the
      * PlanetPanels in this PlanetPanelContainer. */
    void            EnableOrderIssuing(bool enable = true);
    //@}

    mutable boost::signal<void (int)> PlanetSelectedSignal; ///< emitted when an enabled planet panel is clicked by the user

private:
    void            DisableNonSelectionCandidates();    //!< disables planet panels that aren't selection candidates

    void            PlanetPanelClicked(int planet_id);  //!< responds to user clicking a planet panel.  emits PlanetSelectedSignal
    void            DoPanelsLayout(GG::Y top);          //!< repositions PlanetPanels, positioning the top panel at y position \a top relative to the to of the container.
    void            DoPanelsLayout();                   //!< repositions PlanetPanels, without moving top panel.  Panels below may shift if ones above them have resized.
    void            DoLayout();

    void            VScroll(int pos_top, int pos_bottom, int range_min, int range_max); //!< responds to user scrolling of planet panels list.  all but first parameter ignored

    std::vector<PlanetPanel*>   m_planet_panels;
    GG::Y                       m_planet_panels_top;

    int                         m_selected_planet_id;
    std::set<int>               m_candidate_ids;

    boost::shared_ptr<UniverseObjectVisitor>
                                m_valid_selection_predicate;

    CUIScroll*                  m_vscroll; ///< the vertical scroll (for viewing all the planet panes)
};

class RotatingPlanetControl : public GG::Control {
public:
    RotatingPlanetControl(GG::X x, GG::Y y, int planet_id, StarType star_type) :
        GG::Control(x, y, GG::X1, GG::Y1, GG::Flags<GG::WndFlag>()),
        m_planet_id(planet_id),
        m_initial_rotation(fmod(planet_id / 7.352535, 1.0)),    // arbitrary scale number applied to id to give consistent by varied angles
        m_star_type(star_type)
    {
        if (!s_scanline_shader && GetOptionsDB().Get<bool>("UI.system-fog-of-war")) {
            boost::filesystem::path shader_path = GetRootDataDir() / "default" / "shaders" / "scanlines.frag";
            std::string shader_text;
            ReadFile(shader_path, shader_text);
            s_scanline_shader = boost::shared_ptr<ShaderProgram>(
                ShaderProgram::shaderProgramFactory("", shader_text));
        }
        Refresh();
    }

    virtual void Render() {
        GG::Pt ul = UpperLeft(), lr = LowerRight();
        // render rotating base planet texture
        RenderPlanet(ul + GG::Pt(Width() / 2, Height() / 2), Value(Width()), m_surface_texture, m_overlay_texture,
                     m_initial_rotation, m_rpm, m_axial_tilt, m_shininess, m_star_type);

        // overlay atmosphere texture (non-animated)
        if (m_atmosphere_texture) {
            double texture_w = Value(m_atmosphere_texture->DefaultWidth());
            double texture_h = Value(m_atmosphere_texture->DefaultHeight());
            double x_scale = m_diameter / texture_w;
            double y_scale = m_diameter / texture_h;
            glColor4ub(255, 255, 255, m_atmosphere_alpha);
            m_atmosphere_texture->OrthoBlit(GG::Pt(static_cast<GG::X>(ul.x - m_atmosphere_planet_rect.ul.x * x_scale),
                                                   static_cast<GG::Y>(ul.y - m_atmosphere_planet_rect.ul.y * y_scale)),
                                            GG::Pt(static_cast<GG::X>(lr.x + (texture_w - m_atmosphere_planet_rect.lr.x) * x_scale),
                                                   static_cast<GG::Y>(lr.y + (texture_h - m_atmosphere_planet_rect.lr.y) * y_scale)));
        }

        // render fog of war over planet if it's not visible to this client's player
        if (s_scanline_shader &&
            m_visibility <= VIS_BASIC_VISIBILITY &&
            GetOptionsDB().Get<bool>("UI.system-fog-of-war"))
        {
            float fog_scanline_spacing = static_cast<float>(GetOptionsDB().Get<double>("UI.system-fog-of-war-spacing"));
            s_scanline_shader->Use();
            s_scanline_shader->Bind("scanline_spacing", fog_scanline_spacing);
            CircleArc(ul, lr, 0.0, TWO_PI, true);
            s_scanline_shader->stopUse();
        }
    }

    void Refresh() {
        ScopedTimer timer("RotatingPlanetControl::Refresh", true);
        const Planet* planet = GetPlanet(m_planet_id);
        if (!planet) return;

        // these values ensure that wierd GLUT-sphere artifacts do not show themselves
        double period = static_cast<double>(planet->RotationalPeriod());// gives about one rpm for a 1 "Day" rotational period
        if (std::abs(period) <  0.1)    // prevent divide by zero or extremely fast rotations
            period = 0.1;
        m_rpm = 1.0 / period;
        m_diameter = PlanetDiameter(planet->Size());
        m_axial_tilt = std::max(-30.0, std::min(static_cast<double>(planet->AxialTilt()), 60.0));
        m_visibility = GetUniverse().GetObjectVisibilityByEmpire(m_planet_id, HumanClientApp::GetApp()->EmpireID());

        const std::string texture_filename;
        const std::map<PlanetType, std::vector<RotatingPlanetData> >& planet_data = GetRotatingPlanetData();

        std::map<PlanetType, std::vector<RotatingPlanetData> >::const_iterator it = planet_data.find(planet->Type());
        int num_planets_of_type;
        if (it != planet_data.end() && (num_planets_of_type = planet_data.find(planet->Type())->second.size())) {
            unsigned int hash_value = static_cast<int>(m_planet_id);
            const RotatingPlanetData& rpd = it->second[hash_value % num_planets_of_type];
            m_surface_texture = ClientUI::GetTexture(ClientUI::ArtDir() / rpd.filename, true);
            m_shininess = rpd.shininess;

            const std::map<std::string, PlanetAtmosphereData>& atmosphere_data = GetPlanetAtmosphereData();
            std::map<std::string, PlanetAtmosphereData>::const_iterator it = atmosphere_data.find(rpd.filename);
            if (it != atmosphere_data.end()) {
                const PlanetAtmosphereData::Atmosphere& atmosphere = it->second.atmospheres[RandSmallInt(0, it->second.atmospheres.size() - 1)];
                m_atmosphere_texture = ClientUI::GetTexture(ClientUI::ArtDir() / atmosphere.filename, true);
                m_atmosphere_alpha = atmosphere.alpha;
                m_atmosphere_planet_rect = GG::Rect(GG::X1, GG::Y1, m_atmosphere_texture->DefaultWidth() - 4, m_atmosphere_texture->DefaultHeight() - 4);
            }
        }

        if (!planet->SurfaceTexture().empty())
            m_overlay_texture = ClientUI::GetTexture(ClientUI::ArtDir() / planet->SurfaceTexture(), true);

        Resize(GG::Pt(GG::X(PlanetDiameter(planet->Size())), GG::Y(PlanetDiameter(planet->Size()))));
    }

private:
    int                             m_planet_id;
    double                          m_rpm;
    int                             m_diameter;
    double                          m_axial_tilt;
    Visibility                      m_visibility;
    boost::shared_ptr<GG::Texture>  m_surface_texture;
    double                          m_shininess;
    boost::shared_ptr<GG::Texture>  m_overlay_texture;
    boost::shared_ptr<GG::Texture>  m_atmosphere_texture;
    int                             m_atmosphere_alpha;
    GG::Rect                        m_atmosphere_planet_rect;
    double                          m_initial_rotation;
    StarType                        m_star_type;

    static boost::shared_ptr<ShaderProgram> s_scanline_shader;
};
boost::shared_ptr<ShaderProgram> RotatingPlanetControl::s_scanline_shader = boost::shared_ptr<ShaderProgram>();


namespace {
    int SystemNameFontSize() {
        return ClientUI::Pts()*1.5;
    }

    GG::Y SystemNameTextControlHeight() {
        return GG::Y(SystemNameFontSize()*4/3);
    }

    class SystemRow : public GG::ListBox::Row {
    public:
        SystemRow(int system_id) :
            GG::ListBox::Row(),
            m_system_id(system_id)
        {
            SetDragDropDataType("SystemRow");
            push_back(new OwnerColoredSystemName(m_system_id, SystemNameFontSize(), false));
        }

        int SystemID() const { return m_system_id; }
    private:
        int m_system_id;
    };

    XMLElement  GetXMLChild(XMLElement &node,const std::string &child_path) {
        int index;

        if (-1 == (index=child_path.find_first_of('.')))
            return node.ContainsChild(child_path)?node.Child(child_path):XMLElement();
        else
            return node.ContainsChild(child_path.substr(0,index)) ?
                GetXMLChild(node.Child(child_path.substr(0, index)), child_path.substr(index + 1, child_path.length() - index - 1))
              : XMLElement();
    }

    void        GetAsteroidTextures(int planet_id, std::vector<boost::shared_ptr<GG::Texture> > &textures) {
        const int NUM_ASTEROID_SETS = 3;
        const int NUM_IMAGES_PER_SET = 256;
        const int SET = (planet_id % NUM_ASTEROID_SETS) + 1;

        for (int i = 0; i < NUM_IMAGES_PER_SET; ++i)
            textures.push_back(ClientUI::GetTexture(ClientUI::ArtDir() / "planets" / "asteroids" / boost::io::str(boost::format("asteroids%d_%03d.png") % SET % i)));
    }

    const std::string EMPTY_STRING;

    const std::string& GetPlanetSizeName(const Planet& planet) {
        if (planet.Size() == SZ_ASTEROIDS || planet.Size() == SZ_GASGIANT)
            return EMPTY_STRING;
        return UserString(boost::lexical_cast<std::string>(planet.Size()));
    }

    const std::string& GetPlanetTypeName(const Planet& planet)
    { return UserString(boost::lexical_cast<std::string>(planet.Type())); }

    const std::string& GetPlanetEnvironmentName(const Planet &planet, const std::string& species_name)
    { return UserString(boost::lexical_cast<std::string>(planet.EnvironmentForSpecies(species_name))); }

    const std::string& GetStarTypeName(const System& system) {
        if (system.GetStarType() == INVALID_STAR_TYPE)
            return EMPTY_STRING;
        return UserString(boost::lexical_cast<std::string>(system.GetStarType()));
    }

    const GG::Y PLANET_PANEL_TOP = GG::Y(140);

    GG::X ButtonWidth() {
        return GG::X(Value(SystemNameTextControlHeight()));
    }

    GG::Y ButtonHeight() {
        return SystemNameTextControlHeight();
    }
}

////////////////////////////////////////////////
// SidePanel::PlanetPanel
////////////////////////////////////////////////
namespace {
    static const bool SHOW_ALL_PLANET_PANELS = false;   //!< toggles whether to show population, resource, military and building info panels on planet panels that this player doesn't control

    /** How big we want meter icons with respect to the current UI font size.
      * Meters should scale along font size, but not below the size for the
      * default 12 points font. */
    GG::Pt MeterIconSize() {
        const int icon_size = std::max(ClientUI::Pts(), 12) * 4/3;
        return GG::Pt(GG::X(icon_size), GG::Y(icon_size));
    }
}

SidePanel::PlanetPanel::PlanetPanel(GG::X w, int planet_id, StarType star_type) :
    GG::Control(GG::X0, GG::Y0, w, GG::Y1, GG::INTERACTIVE),
    m_planet_id(planet_id),
    m_planet_name(0),
    m_env_size(0),
    m_colonize_button(0),
    m_invade_button(0),
    m_planet_graphic(0),
    m_rotating_planet_graphic(0),
    m_selected(false),
    m_order_issuing_enabled(true),
    m_empire_colour(GG::CLR_ZERO),
    m_focus_drop(0),
    m_population_panel(0),
    m_resource_panel(0),
    m_military_panel(0),
    m_buildings_panel(0),
    m_specials_panel(0),
    m_star_type(star_type)
{
    SetName(UserString("PLANET_PANEL"));

    const Planet* planet = GetPlanet(m_planet_id);
    if (!planet) {
        Logger().errorStream() << "SidePanel::PlanetPanel::PlanetPanel couldn't get latest known planet with ID " << m_planet_id;
        return;
    }

    // create planet name text

    // apply formatting tags around planet name to indicate:
    //    Italic for homeworlds
    //    Bold for capital(s)
    //    Underline for shipyard(s), and
    bool capital = false, homeworld = false, has_shipyard = false;

    // need to check all empires for capitals
    const EmpireManager& empire_manager = Empires();
    for (EmpireManager::const_iterator empire_it = empire_manager.begin(); empire_it != empire_manager.end(); ++empire_it) {
        const Empire* empire = empire_it->second;
        if (!empire) {
            Logger().errorStream() << "PlanetPanel::PlanetPanel got null empire pointer for id " << empire_it->first;
            continue;
        }
        if (empire->CapitalID() == m_planet_id) {
            capital = true;
            break;
        }
    }

    // need to check all species for homeworlds
    const SpeciesManager& species_manager = GetSpeciesManager();
    for (SpeciesManager::iterator species_it = species_manager.begin(); species_it != species_manager.end(); ++species_it) {
        if (const Species* species = species_it->second) {
            const std::set<int>& homeworld_ids = species->Homeworlds();
            if (homeworld_ids.find(m_planet_id) != homeworld_ids.end()) {
                homeworld = true;
                break;
            }
        }
    }

    // check for shipyard
    const std::set<int>& buildings = planet->Buildings();
    for (std::set<int>::const_iterator building_it = buildings.begin(); building_it != buildings.end(); ++building_it) {
        const Building* building = GetBuilding(*building_it);
        if (!building)
            continue;
        // annoying hard-coded building name here... not sure how better to deal with it
        if (building->BuildingTypeName() == "BLD_SHIPYARD_BASE") {
            has_shipyard = true;
            break;
        }
    }

    // wrap with formatting tags
    std::string wrapped_planet_name = planet->Name();
    if (homeworld)
        wrapped_planet_name = "<i>" + wrapped_planet_name + "</i>";
    if (has_shipyard)
        wrapped_planet_name = "<u>" + wrapped_planet_name + "</u>";
    boost::shared_ptr<GG::Font> font;
    if (capital)
        font = ClientUI::GetBoldFont(ClientUI::Pts()*4/3);
    else
        font = ClientUI::GetFont(ClientUI::Pts()*4/3);


    GG::X panel_width = w - MaxPlanetDiameter() - 2*EDGE_PAD;


    // create planet name control
    m_planet_name = new ShadowedTextControl(GG::X(MaxPlanetDiameter() + EDGE_PAD), GG::Y0, wrapped_planet_name,
                                            font, ClientUI::TextColor());
    AttachChild(m_planet_name);


    // focus-selection droplist
    m_focus_drop = new CUIDropDownList(GG::X0, GG::Y0, MeterIconSize().x*4,
                                       MeterIconSize().y*3/2, MeterIconSize().y*7/2);
    AttachChild(m_focus_drop);
    GG::Connect(m_focus_drop->SelChangedSignal,     &SidePanel::PlanetPanel::FocusDropListSelectionChanged,  this);
    GG::Connect(this->FocusChangedSignal,           &SidePanel::PlanetPanel::SetFocus, this);
    m_focus_drop->MoveTo(GG::Pt(GG::X1, GG::Y1));   // force auto-resize so height is correct for subsequent layout stuff
    m_focus_drop->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));


    // meter panels
    m_population_panel = new PopulationPanel(panel_width, m_planet_id);
    AttachChild(m_population_panel);
    GG::Connect(m_population_panel->ExpandCollapseSignal,       &SidePanel::PlanetPanel::DoLayout, this);

    m_resource_panel = new ResourcePanel(panel_width, m_planet_id);
    AttachChild(m_resource_panel);
    GG::Connect(m_resource_panel->ExpandCollapseSignal,         &SidePanel::PlanetPanel::DoLayout, this);

    m_military_panel = new MilitaryPanel(panel_width, m_planet_id);
    AttachChild(m_military_panel);
    GG::Connect(m_military_panel->ExpandCollapseSignal,         &SidePanel::PlanetPanel::DoLayout, this);

    m_buildings_panel = new BuildingsPanel(panel_width, 4, m_planet_id);
    AttachChild(m_buildings_panel);
    GG::Connect(m_buildings_panel->ExpandCollapseSignal,        &SidePanel::PlanetPanel::DoLayout, this);

    m_specials_panel = new SpecialsPanel(panel_width, m_planet_id);
    AttachChild(m_specials_panel);

    m_env_size = new GG::TextControl(GG::X(MaxPlanetDiameter()), GG::Y0, "", ClientUI::GetFont(), ClientUI::TextColor());
    AttachChild(m_env_size);


    m_colonize_button = new CUIButton(GG::X(MaxPlanetDiameter()), GG::Y0, GG::X(ClientUI::Pts()*15),
                                      UserString("PL_COLONIZE"), ClientUI::GetFont(),
                                      ClientUI::CtrlColor(), ClientUI::CtrlBorderColor(), 1,
                                      ClientUI::TextColor(), GG::INTERACTIVE);
    GG::Connect(m_colonize_button->ClickedSignal, &SidePanel::PlanetPanel::ClickColonize, this);

    m_invade_button =  new CUIButton(GG::X(MaxPlanetDiameter()), GG::Y0, GG::X(ClientUI::Pts()*15),
                                      UserString("PL_INVADE"), ClientUI::GetFont(),
                                      ClientUI::CtrlColor(), ClientUI::CtrlBorderColor(), 1,
                                      ClientUI::TextColor(), GG::INTERACTIVE);
    GG::Connect(m_invade_button->ClickedSignal, &SidePanel::PlanetPanel::ClickInvade, this);

    if (m_planet_graphic)
        MoveChildDown(m_planet_graphic);

    SetChildClippingMode(ClipToWindow);

    Refresh();
}

SidePanel::PlanetPanel::~PlanetPanel() {
    delete m_colonize_button;
    delete m_invade_button;
    delete m_env_size;
    delete m_focus_drop;
    delete m_population_panel;
    delete m_resource_panel;
    delete m_military_panel;
    delete m_buildings_panel;
    delete m_specials_panel;
}

void SidePanel::PlanetPanel::DoLayout() {
    GG::X left = GG::X0 + MaxPlanetDiameter() + EDGE_PAD;
    GG::X right = left + Width() - MaxPlanetDiameter() - 2*EDGE_PAD;
    GG::Y y = GG::Y0;

    if (m_planet_name) {
        m_planet_name->MoveTo(GG::Pt(left, y));
        y += m_planet_name->Height();                           // no interpanel space needed here, I declare arbitrarily
    }

    if (m_specials_panel) {
        m_specials_panel->SizeMove(GG::Pt(left, y), GG::Pt(right, y + m_specials_panel->Height())); // assumed to always be this Wnd's child
        y += m_specials_panel->Height() + EDGE_PAD;
    }

    if (m_env_size && m_env_size->Parent() == this) {
        m_env_size->MoveTo(GG::Pt(left, y));
        y += m_env_size->Height() + EDGE_PAD;
    }

    if (m_colonize_button && m_colonize_button->Parent() == this) {
        m_colonize_button->MoveTo(GG::Pt(left, y));
        y += m_colonize_button->Height() + EDGE_PAD;
    } else if (m_invade_button && m_invade_button->Parent() == this) {
        m_invade_button->MoveTo(GG::Pt(left, y));
        y += m_invade_button->Height() + EDGE_PAD;
    }

    if (m_focus_drop && m_focus_drop->Parent() == this) {
        m_focus_drop->MoveTo(GG::Pt(left, y));
        y += m_focus_drop->Height() + EDGE_PAD;
    }

    if (m_population_panel && m_population_panel->Parent() == this) {
        m_population_panel->SizeMove(GG::Pt(left, y), GG::Pt(right, y + m_population_panel->Height()));
        y += m_population_panel->Height() + EDGE_PAD;
    }

    if (m_resource_panel && m_resource_panel->Parent() == this) {
        m_resource_panel->SizeMove(GG::Pt(left, y), GG::Pt(right, y + m_resource_panel->Height()));
        y += m_resource_panel->Height() + EDGE_PAD;
    }

    if (m_military_panel && m_military_panel->Parent() == this) {
        m_military_panel->SizeMove(GG::Pt(left, y), GG::Pt(right, y + m_military_panel->Height()));
        y += m_military_panel->Height() + EDGE_PAD;
    }

    if (m_buildings_panel && m_buildings_panel->Parent() == this) {
        m_buildings_panel->SizeMove(GG::Pt(left, y), GG::Pt(right, y + m_buildings_panel->Height()));
        y += m_buildings_panel->Height() + EDGE_PAD;
    }

    GG::Y min_height(MaxPlanetDiameter());

    CheckDisplayPlanets();
    if (m_planet_graphic)
        min_height = m_planet_graphic->Height();
    // TODO: get following to resize panel properly...
    //else if (m_rotating_planet_graphic)
    //    min_height = m_rotating_planet_graphic->Height();

    Resize(GG::Pt(Width(), std::max(y, min_height)));

    ResizedSignal();
}

void SidePanel::PlanetPanel::CheckDisplayPlanets() {
    const Planet* planet = GetPlanet(m_planet_id);
    if (!planet || !GetOptionsDB().Get<bool>("UI.sidepanel-planet-shown"))
        return;

    delete m_planet_graphic;
    m_planet_graphic = 0;
    delete m_rotating_planet_graphic;
    m_rotating_planet_graphic = 0;

    if (planet->Type() == PT_ASTEROIDS) {
        std::vector<boost::shared_ptr<GG::Texture> > textures;
        GetAsteroidTextures(m_planet_id, textures);
        GG::X texture_width = textures[0]->DefaultWidth();
        GG::Y texture_height = textures[0]->DefaultHeight();
        GG::Pt planet_image_pos(GG::X(MaxPlanetDiameter() / 2 - texture_width / 2 + 3), GG::Y0);

        m_planet_graphic = new GG::DynamicGraphic(planet_image_pos.x, planet_image_pos.y,
                                                    texture_width, texture_height, true,
                                                    texture_width, texture_height, 0, textures,
                                                    GG::GRAPHIC_FITGRAPHIC | GG::GRAPHIC_PROPSCALE);
        m_planet_graphic->SetFPS(GetAsteroidsFPS());
        m_planet_graphic->SetFrameIndex(RandSmallInt(0, textures.size() - 1));
        AttachChild(m_planet_graphic);
        m_planet_graphic->Play();
    } else if (planet->Type() < NUM_PLANET_TYPES) {
        int planet_image_sz = PlanetDiameter(planet->Size());
        GG::Pt planet_image_pos(GG::X(MaxPlanetDiameter() / 2 - planet_image_sz / 2 + 3),
                                GG::Y(MaxPlanetDiameter() / 2 - planet_image_sz / 2));
        m_rotating_planet_graphic = new RotatingPlanetControl(planet_image_pos.x, planet_image_pos.y,
                                                              m_planet_id, m_star_type);
        AttachChild(m_rotating_planet_graphic);
    }
}

namespace {
    bool IsAvailable(const Ship* ship, int system_id, int empire_id) {
        if (!ship)
            return false;
        Fleet* fleet = GetFleet(ship->FleetID());
        if (!fleet)
            return false;
        if (ship->SystemID() == system_id &&
            ship->OwnedBy(empire_id) &&
            ship->GetVisibility(empire_id) >= VIS_PARTIAL_VISIBILITY &&
            ship->OrderedScrapped() == false &&
            fleet->FinalDestinationID() == INVALID_OBJECT_ID )
        { return true; }
        return false;
    }

    bool AvailableToColonize(const Ship* ship, int system_id, int empire_id) {
        if (!ship)
            return false;
        Fleet* fleet = GetFleet(ship->FleetID());
        if (!fleet)
            return false;
        if ( IsAvailable(ship, system_id, empire_id) &&
            ship->CanColonize() &&
            ship->OrderedColonizePlanet() == INVALID_OBJECT_ID )
        { return true; }
        return false;
    };

    bool AvailableToInvade(const Ship* ship, int system_id, int empire_id) {
        if (!ship)
            return false;
        Fleet* fleet = GetFleet(ship->FleetID());
        if (!fleet)
            return false;
        if (IsAvailable(ship, system_id, empire_id) &&
            ship->HasTroops() &&
            ship->OrderedInvadePlanet() == INVALID_OBJECT_ID )
        { return true; }
        return false;
    };

    bool CanColonizePlanetType(const Ship* ship, PlanetType planet_type) {
        if (!ship || planet_type == INVALID_PLANET_TYPE)
            return false;

        const ShipDesign* design = 0;
        double colony_ship_capacity = 0.0;

        design = ship->Design();
        if (design)
            colony_ship_capacity = design->ColonyCapacity();

        if (const Species* colony_ship_species = GetSpecies(ship->SpeciesName())) {
            PlanetEnvironment planet_env_for_colony_species = colony_ship_species->GetPlanetEnvironment(planet_type);

            // One-Click Colonize planets that are colonizable (even if they are
            // not hospitable), and One-Click Outpost planets that are not
            // colonizable.
            if (colony_ship_capacity > 0.0) {
                return planet_env_for_colony_species >= PE_HOSTILE && planet_env_for_colony_species <= PE_GOOD;
            } else {
                return planet_env_for_colony_species < PE_HOSTILE || planet_env_for_colony_species > PE_GOOD;
            }
        }
        return false;
    }

    std::set<const Ship*> ValidSelectedInvasionShips(int system_id) {
        std::set<const Ship*> retval;

        // if not looking in a valid system, no valid colony ship can be available
        if (system_id == INVALID_OBJECT_ID)
            return retval;

        // is there a valid single selected ship in the active FleetWnd?
        std::set<int> selected_ship_ids = FleetUIManager::GetFleetUIManager().SelectedShipIDs();
        for (std::set<int>::const_iterator ss_it = selected_ship_ids.begin(); ss_it != selected_ship_ids.end(); ++ss_it)
            if (Ship* ship = GetUniverse().Objects().Object<Ship>(*ss_it))
                if (ship->SystemID() == system_id && ship->HasTroops() && ship->OwnedBy(HumanClientApp::GetApp()->EmpireID()))
                    retval.insert(ship);

        return retval;
    }

    bool OwnedColonyShipsInSystem(int empire_id, int system_id) {
        if (!GetSystem(system_id))
            return false;

        const ObjectMap& objects = Objects();
        std::vector<const Ship*> ships = objects.FindObjects<Ship>();
        for (std::vector<const Ship*>::const_iterator it = ships.begin(); it != ships.end(); ++it) {
            const Ship* ship = *it;
            if (ship->SystemID() == system_id &&
                ship->OwnedBy(empire_id) &&
                ship->CanColonize())
            {
                return true;
                break;
            }
        }
        return false;
    }
}

const Ship* ValidSelectedColonyShip(int system_id) {
    // if not looking in a valid system, no valid colony ship can be available
    if (system_id == INVALID_OBJECT_ID)
        return 0;

    // is there a valid selected ship in the active FleetWnd?
    std::set<int> selected_ship_ids = FleetUIManager::GetFleetUIManager().SelectedShipIDs();
    for (std::set<int>::const_iterator ss_it = selected_ship_ids.begin(); ss_it != selected_ship_ids.end(); ++ss_it)
        if (const Ship* ship = GetShip(*ss_it))
            if (ship->SystemID() == system_id && ship->CanColonize() && ship->OwnedBy(HumanClientApp::GetApp()->EmpireID()))
                return ship;
    return 0;
}

int AutomaticallyChosenColonyShip(int target_planet_id) {
    int empire_id = HumanClientApp::GetApp()->EmpireID();
    if (empire_id == ALL_EMPIRES)
        return INVALID_OBJECT_ID;
    const Planet* target_planet = GetPlanet(target_planet_id);
    if (!target_planet)
        return INVALID_OBJECT_ID;
    int system_id = target_planet->SystemID();
    const System* system = GetSystem(system_id);
    if (!system)
        return INVALID_OBJECT_ID;
    // is planet a valid colonization target?
    if (target_planet->CurrentMeterValue(METER_POPULATION) > 0.0 ||
        (!target_planet->Unowned() && !target_planet->OwnedBy(empire_id)))
    { return INVALID_OBJECT_ID; }

    PlanetType target_planet_type = target_planet->Type();

    const ObjectMap& objects = Objects();
    std::vector<const Ship*> ships = objects.FindObjects<Ship>();
    std::vector<const Ship*> capable_and_available_colony_ships;
    capable_and_available_colony_ships.reserve(ships.size());

    // get all ships that can colonize and that are free to do so in the
    // specified planet'ssystem and that can colonize the requested planet
    for (std::vector<const Ship*>::const_iterator it = ships.begin(); it != ships.end(); ++it) {
        const Ship* ship = *it;
        if (!AvailableToColonize(ship, system_id, empire_id))
            continue;
        if (!CanColonizePlanetType(ship, target_planet_type))
            continue;
        capable_and_available_colony_ships.push_back(ship);
    }

    // simple case early exits: no ships, or just one capable ship
    if (capable_and_available_colony_ships.empty())
        return INVALID_OBJECT_ID;
    if (capable_and_available_colony_ships.size() == 1)
        return (*capable_and_available_colony_ships.begin())->ID();

    // TODO: have more than one ship capable and available to colonize.
    // pick the "best" one.

    Logger().debugStream() << "Autoselected colony ship " << (*capable_and_available_colony_ships.begin())->ID() << " for planet " << target_planet->Name();

    return (*capable_and_available_colony_ships.begin())->ID();
}

std::set<const Ship*> AutomaticallyChosenInvasionShips(int target_planet_id) {
    std::set<const Ship*> retval;

    int empire_id = HumanClientApp::GetApp()->EmpireID();
    if (empire_id == ALL_EMPIRES)
        return retval;

    const Planet* target_planet = GetPlanet(target_planet_id);
    if (!target_planet)
        return retval;
    int system_id = target_planet->SystemID();
    const System* system = GetSystem(system_id);
    if (!system)
        return retval;

    //Can't invade owned-by-self planets; early exit
    if (target_planet->OwnedBy(empire_id))
        return retval; 


    // get "just enough" ships that can invade and that are free to do so
    double defending_troops = target_planet->NextTurnCurrentMeterValue(METER_TROOPS);

    const ObjectMap& objects = Objects();
    std::vector<const Ship*> ships = objects.FindObjects<Ship>();

    double invasion_troops = 0;
    for (std::vector<const Ship*>::const_iterator it = ships.begin(); it != ships.end(); ++it) {
        const Ship* ship = *it;
        if (!AvailableToInvade(ship, system_id, empire_id))
            continue;

        if (const ShipDesign* design = ship->Design())
            invasion_troops += design->TroopCapacity();

        retval.insert(ship);

        if (invasion_troops > defending_troops)
            break;
    }

    return retval;
}

void SidePanel::PlanetPanel::Refresh() {
    int client_empire_id = HumanClientApp::GetApp()->EmpireID();
    m_planet_connection.disconnect();

    const Planet* planet = GetPlanet(m_planet_id);
    if (!planet) {
        Logger().debugStream() << "PlanetPanel::Refresh couldn't get planet!";
        // clear / hide everything...
        DetachChild(m_planet_name);
        delete m_planet_name;           m_planet_name = 0;

        DetachChild(m_env_size);
        delete m_env_size;              m_env_size = 0;

        DetachChild(m_focus_drop);      m_focus_drop = 0;
        delete m_focus_drop;

        DetachChild(m_population_panel);
        delete m_population_panel;      m_population_panel = 0;

        DetachChild(m_resource_panel);
        delete m_resource_panel;        m_resource_panel = 0;

        DetachChild(m_military_panel);
        delete m_military_panel;        m_military_panel = 0;

        DetachChild(m_buildings_panel);
        delete m_buildings_panel;       m_buildings_panel = 0;

        DetachChild(m_colonize_button);
        delete m_colonize_button;       m_colonize_button = 0;

        DetachChild(m_invade_button);
        delete m_invade_button;         m_invade_button = 0;

        DetachChild(m_specials_panel);
        delete m_specials_panel;        m_specials_panel = 0;

        DoLayout();
        return;
    }

    // colour planet name with owner's empire colour
    m_empire_colour = GG::CLR_ZERO;
    if (!planet->Unowned() && m_planet_name) {
        if (Empire* planet_empire = Empires().Lookup(planet->Owner())) {
            m_empire_colour = planet_empire->Color();
            m_planet_name->SetTextColor(planet_empire->Color());
        } else {
            m_planet_name->SetTextColor(ClientUI::TextColor());
        }
    }

    const Ship* selected_colony_ship = ValidSelectedColonyShip(SidePanel::SystemID());
    if (!selected_colony_ship && FleetUIManager::GetFleetUIManager().SelectedShipIDs().empty())
        selected_colony_ship = GetShip(AutomaticallyChosenColonyShip(m_planet_id));

    std::set<const Ship*> invasion_ships = ValidSelectedInvasionShips(SidePanel::SystemID());
    if (invasion_ships.empty()) {
        std::set<const Ship*> autoselected_invasion_ships = AutomaticallyChosenInvasionShips(m_planet_id);
        invasion_ships.insert(autoselected_invasion_ships.begin(), autoselected_invasion_ships.end());
    }


    std::string colony_ship_species_name;
    const ShipDesign* design = 0;
    double colony_ship_capacity = 0.0;
    if (selected_colony_ship) {
        colony_ship_species_name = selected_colony_ship->SpeciesName();
        design = selected_colony_ship->Design();
        if (design)
            colony_ship_capacity = design->ColonyCapacity();
    }
    const Species* colony_ship_species = GetSpecies(colony_ship_species_name);
    PlanetEnvironment planet_env_for_colony_species = PE_UNINHABITABLE;
    if (colony_ship_species)
        planet_env_for_colony_species = colony_ship_species->GetPlanetEnvironment(planet->Type());
    double planet_capacity = planet->CurrentMeterValue(METER_TARGET_POPULATION);


    // calculate truth tables for planet colonization and invasion
    bool has_owner =        !planet->Unowned();
    bool mine =             planet->OwnedBy(client_empire_id);
    bool populated =        planet->CurrentMeterValue(METER_POPULATION) > 0.0;
    bool habitable =        planet_env_for_colony_species >= PE_HOSTILE && planet_env_for_colony_species <= PE_GOOD;
    bool visible =          GetUniverse().GetObjectVisibilityByEmpire(m_planet_id, client_empire_id) >= VIS_BASIC_VISIBILITY;
    bool shielded =         planet->CurrentMeterValue(METER_SHIELD) > 0.0;
    bool being_colonized =  planet->IsAboutToBeColonized();
    bool outpostable =                   !populated && (  !has_owner /*&& !shielded*/         ) && visible && !being_colonized;
    bool colonizable =      habitable && !populated && ( (!has_owner /*&& !shielded*/) || mine) && visible && !being_colonized;
    bool can_colonize =     selected_colony_ship && (colonizable || (outpostable && colony_ship_capacity == 0.0));

    bool being_invaded =    planet->IsAboutToBeInvaded();
    bool at_war_with_me =   !mine && (populated || (has_owner && Empires().GetDiplomaticStatus(client_empire_id, planet->Owner()) == DIPLO_WAR));
    bool invadable =        at_war_with_me && !shielded && visible && !being_invaded && !invasion_ships.empty();

    if (populated || SHOW_ALL_PLANET_PANELS) {
        AttachChild(m_population_panel);
        if (m_population_panel)
            m_population_panel->Refresh();
    } else {
        DetachChild(m_population_panel);
    }

    if (populated || has_owner || SHOW_ALL_PLANET_PANELS) {
        AttachChild(m_resource_panel);
        if (m_resource_panel)
            m_resource_panel->Refresh();
        AttachChild(m_military_panel);
        if (m_military_panel)
            m_military_panel->Refresh();
    } else {
        DetachChild(m_resource_panel);
        DetachChild(m_military_panel);
    }


    DetachChild(m_invade_button);
    DetachChild(m_colonize_button);
    DetachChild(m_focus_drop);

    std::string species_name;
    if (!planet->SpeciesName().empty())
        species_name = planet->SpeciesName();
    else if (!colony_ship_species_name.empty())
        species_name = colony_ship_species_name;

    std::string env_size_text;

    if (species_name.empty())
        env_size_text = boost::io::str(FlexibleFormat(UserString("PL_TYPE_SIZE"))
                                       % GetPlanetSizeName(*planet)
                                       % GetPlanetTypeName(*planet));
    else
        env_size_text = boost::io::str(FlexibleFormat(UserString("PL_TYPE_SIZE_ENV"))
                                       % GetPlanetSizeName(*planet)
                                       % GetPlanetTypeName(*planet)
                                       % GetPlanetEnvironmentName(*planet, species_name));


    if (Disabled() || !(can_colonize || being_colonized || invadable || being_invaded)) {
        // hide everything

    } else if (can_colonize) {
        // show colonize button
        AttachChild(m_colonize_button);
        std::string colonize_text;
        if (colony_ship_capacity > 0.0) {
            std::string initial_pop = DoubleToString(colony_ship_capacity, 2, false);

            std::string clr_tag;
            if (planet_capacity < colony_ship_capacity && colony_ship_capacity > 0.0)
                clr_tag = GG::RgbaTag(ClientUI::StatDecrColor());
            else if (planet_capacity > colony_ship_capacity && colony_ship_capacity > 0.0)
                clr_tag = GG::RgbaTag(ClientUI::StatIncrColor());
            std::string clr_tag_close = (clr_tag.empty() ? "" : "</rgba>");
            std::string target_pop = clr_tag + DoubleToString(planet_capacity, 2, false) + clr_tag_close;

            colonize_text = boost::io::str(FlexibleFormat(UserString("PL_COLONIZE")) % initial_pop % target_pop);
        } else {
            colonize_text = UserString("PL_OUTPOST");
        }
        if (m_colonize_button)
            m_colonize_button->SetText(colonize_text);

    } else if (being_colonized) {
        // shown colonize cancel button
        AttachChild(m_colonize_button);
        if (m_colonize_button)
            m_colonize_button->SetText(UserString("PL_CANCEL_COLONIZE"));

    } else if (invadable) {
        // show invade button
        AttachChild(m_invade_button);
        double invasion_troops = 0.0;
        for (std::set<const Ship*>::const_iterator ship_it = invasion_ships.begin();
             ship_it != invasion_ships.end(); ++ship_it)
        {
            const Ship* invasion_ship = *ship_it;
            if (const ShipDesign* design = invasion_ship->Design())
                invasion_troops += design->TroopCapacity();
        }
        std::string invasion_troops_text = DoubleToString(invasion_troops, 2, false);
        std::string invasion_text = boost::io::str(FlexibleFormat(UserString("PL_INVADE")) % invasion_troops_text);
        if (m_invade_button)
            m_invade_button->SetText(invasion_text);

    } else if (being_invaded) {
        // show invade cancel button
        AttachChild(m_invade_button);
        if (m_invade_button)
            m_invade_button->SetText(UserString("PL_CANCEL_INVADE"));
    }

    m_env_size->SetText(env_size_text);

    if (!planet->SpeciesName().empty()) {
        AttachChild(m_focus_drop);

        const std::vector<std::string>& available_foci = planet->AvailableFoci();

        // refresh items in list
        m_focus_drop->Clear();
        for (std::vector<std::string>::const_iterator it = available_foci.begin();
             it != available_foci.end(); ++it)
        {
            boost::shared_ptr<GG::Texture> texture = ClientUI::GetTexture(
                ClientUI::ArtDir() / planet->FocusIcon(*it), true);
            GG::StaticGraphic* graphic = new GG::StaticGraphic(GG::X0, GG::Y0, MeterIconSize().x*3/2,
                                                               MeterIconSize().y*3/2, texture,
                                                               GG::GRAPHIC_FITGRAPHIC | GG::GRAPHIC_PROPSCALE);
            GG::DropDownList::Row* row = new GG::DropDownList::Row(graphic->Width(), graphic->Height(), "FOCUS");
            row->push_back(dynamic_cast<GG::Control*>(graphic));
            m_focus_drop->Insert(row);
        }

        int drop_items = std::min(5, static_cast<int>(available_foci.size()));
        m_focus_drop->SetDropHeight(drop_items * MeterIconSize().y*3/2 + GG::Y(5));

        // set browse text and select appropriate focus in droplist
        std::string focus_text;
        if (!planet->Focus().empty()) {
            for (unsigned int i = 0; i < available_foci.size(); ++i) {
                if (available_foci[i] == planet->Focus()) {
                    m_focus_drop->Select(i);
                    focus_text = boost::io::str(FlexibleFormat(UserString("RP_FOCUS_TOOLTIP"))
                                                % UserString(planet->Focus()));
                }
            }
        } else {
            m_focus_drop->Select(m_focus_drop->end());
        }
        m_focus_drop->SetBrowseText(focus_text);

        if (!planet->OwnedBy(client_empire_id))
            m_focus_drop->Disable();
    }


    // other panels...
    if (m_buildings_panel)
        m_buildings_panel->Refresh();
    if (m_specials_panel)
        m_specials_panel->Update();


    // BuildingsPanel::Refresh (and other panels) emit ExpandCollapseSignal,
    // which should be connected to SidePanel::PlanetPanel::DoLayout

    m_planet_connection = GG::Connect(planet->StateChangedSignal, &SidePanel::PlanetPanel::Refresh, this, boost::signals::at_front);
}

void SidePanel::PlanetPanel::SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
    GG::Pt old_size = GG::Wnd::Size();

    GG::Wnd::SizeMove(ul, lr);

    if (old_size != GG::Wnd::Size())
        DoLayout();
}

void SidePanel::PlanetPanel::SetFocus(const std::string& focus) {
    const Planet* planet = GetPlanet(m_planet_id);
    if (!planet || !planet->OwnedBy(HumanClientApp::GetApp()->EmpireID()))
        return;
    HumanClientApp::GetApp()->Orders().IssueOrder(OrderPtr(
        new ChangeFocusOrder(HumanClientApp::GetApp()->EmpireID(), planet->ID(), focus)));
}

bool SidePanel::PlanetPanel::InWindow(const GG::Pt& pt) const {
    GG::Pt ul = UpperLeft(), lr = LowerRight();
    if (!(ul <= pt && pt < lr))
        return false;

    GG::Pt planet_box_lr = ul + GG::Pt(GG::X(MaxPlanetDiameter()), GG::Y(MaxPlanetDiameter()));

    // if pt is to the right of the space where the planet render could be,
    // it doesn't matter whether the render is being shown
    if (pt.x >= planet_box_lr.x)
        return true;

    // if pt is in the horizontal space of the planet render, it matters
    // whether the render is being shown
    if (m_rotating_planet_graphic) {
        // showing full sized graphic.  size defaulted to above is accurate
    } else if (m_planet_graphic) {
        planet_box_lr = m_planet_graphic->LowerRight(); // smaller sized image being shown.  use its size.
    } else {
        return false;   // not showing a render, and pt is outside the non-render space, so is not over panel
    }

    // if pt is below planet render space, it can't be over the panel
    if (pt.y > planet_box_lr.y)
        return false;

    // TODO: consider corners

    // otherwise, pt is over render or graphic
    return true;
}

void SidePanel::PlanetPanel::LClick(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys) {
    //std::cout << "SidePanel::PlanetPanel::LClick m_planet_id: " << m_planet_id << std::endl;
    if (!Disabled())
        LClickedSignal(m_planet_id);
}

void SidePanel::PlanetPanel::RClick(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys) {
    const Planet* planet = GetPlanet(m_planet_id);
    if (!planet)
        return;


    GG::MenuItem menu_contents;
    if (planet->OwnedBy(HumanClientApp::GetApp()->EmpireID()) && m_order_issuing_enabled )
        menu_contents.next_level.push_back(GG::MenuItem(UserString("SP_RENAME_PLANET"), 1, false, false));

    menu_contents.next_level.push_back(GG::MenuItem(UserString("SP_PLANET_SUITABILITY"), 2, false, false));
    GG::PopupMenu popup(pt.x, pt.y, ClientUI::GetFont(), menu_contents, ClientUI::TextColor(),
                        ClientUI::WndOuterBorderColor(), ClientUI::WndColor());

    if (popup.Run())
        switch (popup.MenuID())
        {
        case 1:
        { // rename planet
            std::string plt_name = planet->Name();
            CUIEditWnd edit_wnd(GG::X(350), UserString("SP_ENTER_NEW_PLANET_NAME"), plt_name);
            edit_wnd.Run();
            if (edit_wnd.Result() != "" && edit_wnd.Result() != planet->Name() && m_order_issuing_enabled)
            {
                HumanClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new RenameOrder(HumanClientApp::GetApp()->EmpireID(), planet->ID(), edit_wnd.Result())));
                m_planet_name->SetText(planet->Name());
            }
            break;
        }
        case 2:
        {   // colonizable/suitability report
            ClientUI::GetClientUI()->ZoomToPlanetPedia(m_planet_id);
        }
        default:
        break;
    }
}

void SidePanel::PlanetPanel::MouseWheel(const GG::Pt& pt, int move, GG::Flags<GG::ModKey> mod_keys)
{ ForwardEventToParent(); }

void SidePanel::PlanetPanel::Render() {
    GG::Pt ul = UpperLeft(), lr = LowerRight();
    GG::Pt name_ul = m_planet_name->UpperLeft() - GG::Pt(GG::X(EDGE_PAD), GG::Y0);
    GG::Pt name_lr = GG::Pt(lr.x, m_planet_name->LowerRight().y);
    GG::Pt planet_box_lr = ul + GG::Pt(GG::X(MaxPlanetDiameter()), GG::Y(MaxPlanetDiameter()));
    bool show_planet_box = true;

    if (m_rotating_planet_graphic) {
        // default OK
    } else if (m_planet_graphic) {
        planet_box_lr = m_planet_graphic->LowerRight();
    } else {
        show_planet_box = false;    // no planet render to put box behind
    }


    GG::Clr background_colour = ClientUI::CtrlColor();
    GG::Clr title_background_colour = ClientUI::WndOuterBorderColor();
    GG::Clr border_colour = (m_selected ? m_empire_colour : ClientUI::WndOuterBorderColor());


    const int OFFSET = 15;          // size of corners cut off sticky-out bit of background around planet render
    glDisable(GL_TEXTURE_2D);

    // standard WndColor background for whole panel
    glColor(background_colour);
    glBegin(GL_TRIANGLE_FAN);
        glVertex(lr.x,                  ul.y);                  // top right corner
        if (show_planet_box) {
            glVertex(ul.x + OFFSET,     ul.y);                      // top left, offset right to cut off corner
            glVertex(ul.x,              ul.y + OFFSET);             // top left, offset down to cut off corner
            glVertex(ul.x,              planet_box_lr.y - OFFSET);  // bottom left, offset up to cut off corner
            glVertex(ul.x + OFFSET,     planet_box_lr.y);           // bottom left, offset right to cut off corner
            glVertex(planet_box_lr.x,   planet_box_lr.y);           // inner corner between planet box and rest of panel
        } else {
            glVertex(planet_box_lr.x,   ul.y);                      // top left of main panel, excluding planet box
        }
        glVertex(planet_box_lr.x,   lr.y);                      // bottom left of main panel
        glVertex(lr.x,              lr.y);                      // bottom right
    glEnd();

    // title background box
    glColor(title_background_colour);
    glBegin(GL_QUAD_STRIP);
        glVertex(name_lr.x,             name_ul.y);
        glVertex(name_ul.x,             name_ul.y);
        glVertex(name_lr.x,             name_lr.y);
        glVertex(name_ul.x,             name_lr.y);
    glEnd();

    // border
    glColor(border_colour);
    glLineWidth(1.5);
    glBegin(GL_LINE_LOOP);
        glVertex(lr.x,                  ul.y);                  // top right corner
        if (show_planet_box) {
            glVertex(ul.x + OFFSET,     ul.y);                      // top left, offset right to cut off corner
            glVertex(ul.x,              ul.y + OFFSET);             // top left, offset down to cut off corner
            glVertex(ul.x,              planet_box_lr.y - OFFSET);  // bottom left, offset up to cut off corner
            glVertex(ul.x + OFFSET,     planet_box_lr.y);           // bottom left, offset right to cut off corner
            glVertex(planet_box_lr.x,   planet_box_lr.y);           // inner corner between planet box and rest of panel
        } else {
            glVertex(planet_box_lr.x,   ul.y);                      // top left of main panel, excluding planet box
        }
        glVertex(planet_box_lr.x,   lr.y);                      // bottom left of main panel
        glVertex(lr.x,              lr.y);                      // bottom right
    glEnd();
    glLineWidth(1.0);

    // disable greyover
    const GG::Clr HALF_GREY(128, 128, 128, 128);
    if (Disabled()) {
        glColor(HALF_GREY);
        glBegin(GL_TRIANGLE_FAN);
            glVertex(lr.x,                  ul.y);                  // top right corner
            if (show_planet_box) {
                glVertex(ul.x + OFFSET,     ul.y);                      // top left, offset right to cut off corner
                glVertex(ul.x,              ul.y + OFFSET);             // top left, offset down to cut off corner
                glVertex(ul.x,              planet_box_lr.y - OFFSET);  // bottom left, offset up to cut off corner
                glVertex(ul.x + OFFSET,     planet_box_lr.y);           // bottom left, offset right to cut off corner
                glVertex(planet_box_lr.x,   planet_box_lr.y);           // inner corner between planet box and rest of panel
            } else {
                glVertex(planet_box_lr.x,   ul.y);                      // top left of main panel, excluding planet box
            }
            glVertex(planet_box_lr.x,   lr.y);                      // bottom left of main panel
            glVertex(lr.x,              lr.y);                      // bottom right
        glEnd();
    }

    glEnable(GL_TEXTURE_2D);
}

void SidePanel::PlanetPanel::Select(bool selected) {
    if (m_selected != selected) {
        m_selected = selected;
        // TODO: consider setting text box colours?
    }
}

namespace {
    void CancelColonizeInvadeScrapShipOrders(const Ship* ship) {
        if (!ship)
            return;

        const ClientApp* app = ClientApp::GetApp();
        if (!app)
            return;
        const OrderSet orders = app->Orders();

        // is selected ship already ordered to colonize?  If so, recind that order.
        if (ship->OrderedColonizePlanet() != INVALID_OBJECT_ID) {
            for (OrderSet::const_iterator it = orders.begin(); it != orders.end(); ++it) {
                if (boost::shared_ptr<ColonizeOrder> order = boost::dynamic_pointer_cast<ColonizeOrder>(it->second)) {
                    if (order->ShipID() == ship->ID()) {
                        HumanClientApp::GetApp()->Orders().RecindOrder(it->first);
                        // could break here, but won't to ensure there are no problems with doubled orders
                    }
                }
            }
        }

        // is selected ship ordered to invade?  If so, recind that order
        if (ship->OrderedInvadePlanet() != INVALID_OBJECT_ID) {
            for (OrderSet::const_iterator it = orders.begin(); it != orders.end(); ++it) {
               if (boost::shared_ptr<InvadeOrder> order = boost::dynamic_pointer_cast<InvadeOrder>(it->second)) {
                    if (order->ShipID() == ship->ID()) {
                        HumanClientApp::GetApp()->Orders().RecindOrder(it->first);
                        // could break here, but won't to ensure there are no problems with doubled orders
                    }
                }
            }
        }

        // is selected ship ordered scrapped?  If so, recind that order
        if (ship->OrderedScrapped()) {
            for (OrderSet::const_iterator it = orders.begin(); it != orders.end(); ++it) {
                if (boost::shared_ptr<ScrapOrder> order = boost::dynamic_pointer_cast<ScrapOrder>(it->second)) {
                    if (order->ObjectID() == ship->ID()) {
                        HumanClientApp::GetApp()->Orders().RecindOrder(it->first);
                        // could break here, but won't to ensure there are no problems with doubled orders
                    }
                }
            }
        }
    }
}

void SidePanel::PlanetPanel::ClickColonize() {
    // order or cancel colonization, depending on whether it has previosuly
    // been ordered

    const Planet* planet = GetPlanet(m_planet_id);
    if (!planet || !planet->CurrentMeterValue(METER_POPULATION) == 0.0 || !m_order_issuing_enabled)
        return;

    int empire_id = HumanClientApp::GetApp()->EmpireID();
    if (empire_id == ALL_EMPIRES)
        return;

    std::map<int, int> pending_colonization_orders = PendingColonizationOrders();
    std::map<int, int>::const_iterator it = pending_colonization_orders.find(m_planet_id);

    if (it != pending_colonization_orders.end()) {
        // cancel previous colonization order for planet
        HumanClientApp::GetApp()->Orders().RecindOrder(it->second);

    } else {
        // find colony ship and order it to colonize
        const Ship* ship = ValidSelectedColonyShip(SidePanel::SystemID());
        if (!ship)
            ship = GetShip(AutomaticallyChosenColonyShip(m_planet_id));

        if (!ship) {
            Logger().errorStream() << "SidePanel::PlanetPanel::ClickColonize valid colony not found!";
            return;
        }
        if (!ship->OwnedBy(empire_id)) {
            Logger().errorStream() << "SidePanel::PlanetPanel::ClickColonize selected colony ship not owned by this client's empire.";
            return;
        }

        CancelColonizeInvadeScrapShipOrders(ship);
        HumanClientApp::GetApp()->Orders().IssueOrder(OrderPtr(
            new ColonizeOrder(empire_id, ship->ID(), m_planet_id)));
    }
}

void SidePanel::PlanetPanel::ClickInvade() {
    // order or cancel invasion, depending on whether it has previosuly
    // been ordered

    const Planet* planet = GetPlanet(m_planet_id);
    if (!planet ||
        !m_order_issuing_enabled ||
        (planet->CurrentMeterValue(METER_POPULATION) <= 0.0 && planet->Unowned()))
    { return; }

    int empire_id = HumanClientApp::GetApp()->EmpireID();
    if (empire_id == ALL_EMPIRES)
        return;

    std::map<int, std::set<int> > pending_invade_orders = PendingInvadeOrders();
    std::map<int, std::set<int> >::const_iterator it = pending_invade_orders.find(m_planet_id);

    if (it != pending_invade_orders.end()) {
        const std::set<int>& planet_invade_orders = it->second;
        // cancel previous invasion orders for this planet
        for (std::set<int>::const_iterator o_it = planet_invade_orders.begin();
             o_it != planet_invade_orders.end(); ++o_it)
        { HumanClientApp::GetApp()->Orders().RecindOrder(*o_it); }

    } else {
        // order selected invasion ships to invade planet
        
        std::set<const Ship*> invasion_ships = ValidSelectedInvasionShips(planet->SystemID());

        if( invasion_ships.empty() ){
            std::set<const Ship*> autoselected_invasion_ships = AutomaticallyChosenInvasionShips(m_planet_id);
            invasion_ships.insert(autoselected_invasion_ships.begin(), autoselected_invasion_ships.end());
        }

        for (std::set<const Ship*>::const_iterator ship_it = invasion_ships.begin();
             ship_it != invasion_ships.end(); ++ship_it)
        {
            const Ship* ship = *ship_it;
            if (!ship)
                continue;

            CancelColonizeInvadeScrapShipOrders(ship);

            HumanClientApp::GetApp()->Orders().IssueOrder(OrderPtr(
                new InvadeOrder(empire_id, ship->ID(), m_planet_id)));
        }
    }
}

void SidePanel::PlanetPanel::FocusDropListSelectionChanged(GG::DropDownList::iterator selected) {
    // all this funciton needs to do is emit FocusChangedSignal.  The code
    // preceeding that determines which focus was selected from the iterator 
    // parameter, does some safety checks, and disables UI sounds

    if (m_focus_drop->CurrentItem() == m_focus_drop->end()) {
        Logger().errorStream() << "PlanetPanel::FocusDropListSelectionChanged passed end / invalid interator";
        return;
    }

    const UniverseObject* obj = GetUniverseObject(m_planet_id);
    if (!obj) {
        Logger().errorStream() << "PlanetPanel::FocusDropListSelectionChanged couldn't get object with id " << m_planet_id;
        return;
    }
    const ResourceCenter* res = dynamic_cast<const ResourceCenter*>(obj);
    if (!res) {
        Logger().errorStream() << "PlanetPanel::FocusDropListSelectionChanged couldn't convert object with id " << m_planet_id << " to a ResourceCenter";
        return;
    }

    std::size_t i = m_focus_drop->IteratorToIndex(selected);
    if (i >= res->AvailableFoci().size()) {
        Logger().errorStream() << "PlanetPanel::FocusDropListSelectionChanged got invalid focus selected index: " << i;
        return;
    }

    Sound::TempUISoundDisabler sound_disabler;
    FocusChangedSignal(res->AvailableFoci().at(i));
}

void SidePanel::PlanetPanel::EnableOrderIssuing(bool enable/* = true*/) {
    m_order_issuing_enabled = enable;

    m_colonize_button->Disable(!enable);
    m_invade_button->Disable(!enable);

    m_population_panel->EnableOrderIssuing(enable);
    m_resource_panel->EnableOrderIssuing(enable);
    m_military_panel->EnableOrderIssuing(enable);
    m_buildings_panel->EnableOrderIssuing(enable);
    m_specials_panel->EnableOrderIssuing(enable);

    const UniverseObject* obj = GetUniverseObject(m_planet_id);
    if (!enable || !obj || !obj->OwnedBy(HumanClientApp::GetApp()->EmpireID()))
        m_focus_drop->Disable();
    else
        m_focus_drop->Disable(false);
}

////////////////////////////////////////////////
// SidePanel::PlanetPanelContainer
////////////////////////////////////////////////
SidePanel::PlanetPanelContainer::PlanetPanelContainer(GG::X x, GG::Y y, GG::X w, GG::Y h) :
    Wnd(x, y, w, h, GG::INTERACTIVE),
    m_planet_panels(),
    m_planet_panels_top(GG::Y0),
    m_selected_planet_id(INVALID_OBJECT_ID),
    m_vscroll(new CUIScroll(Width() - GG::X(ClientUI::ScrollWidth()),
                            GG::Y0,
                            GG::X(ClientUI::ScrollWidth()),
                            Height(),
                            GG::VERTICAL))
{
    SetName("PlanetPanelContainer");
    SetChildClippingMode(ClipToClient);
    GG::Connect(m_vscroll->ScrolledSignal, &SidePanel::PlanetPanelContainer::VScroll, this);
}

SidePanel::PlanetPanelContainer::~PlanetPanelContainer()
{ delete m_vscroll; }

bool SidePanel::PlanetPanelContainer::InWindow(const GG::Pt& pt) const {
    // ensure pt is below top of container
    if (pt.y < UpperLeft().y)
        return false;

    // allow point to be within any planet panel that is below top of container
    for (std::vector<PlanetPanel*>::const_iterator it = m_planet_panels.begin(); it != m_planet_panels.end(); ++it) {
        if ((*it)->InWindow(pt))
            return true;
    }

    // disallow point between containers that is left of solid portion of sidepanel.
    // this done by restricting InWindow to discard space between left of container
    // and left of solid portion that wasn't already caught above as being part
    // of a planet panel.

    return UpperLeft() + GG::Pt(GG::X(MaxPlanetDiameter()), GG::Y0) <= pt && pt < LowerRight();
}

void SidePanel::PlanetPanelContainer::MouseWheel(const GG::Pt& pt, int move, GG::Flags<GG::ModKey> mod_keys) {
    if (m_vscroll && m_vscroll->Parent() == this) {
        const std::pair<int, int> initial_pos = m_vscroll->PosnRange();
        if (move < 0)
            m_vscroll->ScrollLineIncr();
        else
            m_vscroll->ScrollLineDecr();
        if (initial_pos != m_vscroll->PosnRange())
            GG::SignalScroll(*m_vscroll, true);
    }
}

void SidePanel::PlanetPanelContainer::LDrag(const GG::Pt& pt, const GG::Pt& move, GG::Flags<GG::ModKey> mod_keys)
{ ForwardEventToParent(); }

int SidePanel::PlanetPanelContainer::ScrollPosition() const {
    if (m_vscroll && m_vscroll->Parent() == this)
        return m_vscroll->PosnRange().first;
    else
        return 0;
}

void SidePanel::PlanetPanelContainer::ScrollTo(int pos) {
    if (m_vscroll && m_vscroll->Parent() == this) {
        const std::pair<int, int> initial_pos = m_vscroll->PosnRange();
        m_vscroll->ScrollTo(pos);
        if (initial_pos != m_vscroll->PosnRange())
            GG::SignalScroll(*m_vscroll, true);
    }
}

void SidePanel::PlanetPanelContainer::Clear() {
    m_planet_panels.clear();
    m_selected_planet_id = INVALID_OBJECT_ID;
    PlanetSelectedSignal(m_selected_planet_id);
    DetachChild(m_vscroll);
    DeleteChildren();
    AttachChild(m_vscroll);
}

void SidePanel::PlanetPanelContainer::SetPlanets(const std::vector<int>& planet_ids, StarType star_type) {
    //std::cout << "SidePanel::PlanetPanelContainer::SetPlanets( size: " << planet_ids.size() << " )" << std::endl;

    int initial_selected_planet_panel = m_selected_planet_id;

    // remove old panels
    Clear();

    std::multimap<int, int> orbits_planets;
    for (std::vector<int>::const_iterator planet_it = planet_ids.begin();
         planet_it != planet_ids.end(); ++planet_it)
    {
        int planet_id = *planet_it;
        const Planet* planet = GetPlanet(planet_id);
        if (!planet) {
            Logger().errorStream() << "PlanetPanelContainer::SetPlanets couldn't find planet with id " << planet_id;
            continue;
        }
        int system_id = planet->SystemID();
        const System* system = GetSystem(system_id);
        if (!system) {
            Logger().errorStream() << "PlanetPanelContainer::SetPlanets couldn't find system of planet" << planet->Name();
            continue;
        }
        orbits_planets.insert(std::make_pair(system->OrbitOfObjectID(planet_id), planet_id));
    }

    // create new panels and connect their signals
    for (std::multimap<int, int>::const_iterator it = orbits_planets.begin(); it != orbits_planets.end(); ++it) {
        PlanetPanel* planet_panel = new PlanetPanel(Width() - m_vscroll->Width(), it->second, star_type);
        AttachChild(planet_panel);
        m_planet_panels.push_back(planet_panel);
        GG::Connect(m_planet_panels.back()->LClickedSignal, &SidePanel::PlanetPanelContainer::PlanetPanelClicked,   this);
        GG::Connect(m_planet_panels.back()->ResizedSignal,  &SidePanel::PlanetPanelContainer::DoPanelsLayout,       this);
    }

    // reset scroll when resetting planets to ensure new set of planets won't be stuck scrolled up out of view
    VScroll(0, 0, 0, 0);

    // disable non-selectable planet panels
    DisableNonSelectionCandidates();

    // redo contents and layout of panels, after enabling or disabling, so
    // they take this into account when doing contents
    RefreshAllPlanetPanels();

    SelectPlanet(initial_selected_planet_panel);
}

void SidePanel::PlanetPanelContainer::DoPanelsLayout()
{ DoPanelsLayout(m_planet_panels_top); }

void SidePanel::PlanetPanelContainer::DoPanelsLayout(GG::Y top) {
    if (top > 0)
        Logger().errorStream() << "SidePanel::PlanetPanelContainer::DoPanelsLaout passed positive top.  It is expected to be 0 or negative only.";
    m_planet_panels_top = top;
    GG::Y y = m_planet_panels_top;
    GG::X x = GG::X0;

    for (std::vector<PlanetPanel*>::iterator it = m_planet_panels.begin(); it != m_planet_panels.end(); ++it) {
        PlanetPanel* panel = *it;
        const GG::Y PANEL_HEIGHT = panel->Height(); // panel height may be different for each panel depending whether that panel has been previously left expanded or collapsed
        GG::Pt panel_ul(x, y);
        GG::Pt panel_lr(Width() - m_vscroll->Width(), y + PANEL_HEIGHT);
        panel->SizeMove(panel_ul, panel_lr);
        y += PANEL_HEIGHT + EDGE_PAD;
    }

    GG::Y available_height = Height();
    if (GG::Wnd* parent = Parent()) {
        GG::Y containing_height = parent->Height();
        const GG::Y BIG_PAD_TO_BE_SAFE = GG::Y(185);
        available_height = containing_height - BIG_PAD_TO_BE_SAFE;  // height of visible "page" of panels
    }

    // adjust size of scrollbar to account for panel resizing
    // hide scrollbar if all panels are visible and fit into the available height
    if (Value(m_planet_panels_top) >= 0 && Value(y - m_planet_panels_top) < available_height + 1) {
        DetachChild(m_vscroll);
    } else {
        // need to show scrollbar.

        // if only need scrollbar due to being scrolled down (but would
        // otherwise fit in available space), make scroll range larger
        // to allow scrolling back up
        int scroll_max = std::max(Value(y - m_planet_panels_top), Value(available_height - m_planet_panels_top));

        m_vscroll->SizeScroll(0, scroll_max, MaxPlanetDiameter(), Value(available_height));
        AttachChild(m_vscroll);
        m_vscroll->Show();
    }
}

void SidePanel::PlanetPanelContainer::DoLayout() {
    GG::Pt scroll_ul(Width() - ClientUI::ScrollWidth(), GG::Y0);
    //GG::Pt scroll_ul(ClientLowerRight().x - GG::X(ClientUI::ScrollWidth()), ClientUpperLeft().y);
    GG::Pt scroll_lr = scroll_ul + GG::Pt(GG::X(ClientUI::ScrollWidth()), Height());
    m_vscroll->SizeMove(scroll_ul, scroll_lr);
    DoPanelsLayout();
}

void SidePanel::PlanetPanelContainer::SelectPlanet(int planet_id) {
    //std::cout << "SidePanel::PlanetPanelContainer::SelectPlanet(" << planet_id << ")" << std::endl;
    if (planet_id != m_selected_planet_id && m_candidate_ids.find(planet_id) != m_candidate_ids.end()) {
        m_selected_planet_id = planet_id;
        bool planet_id_match_found = false;

        // scan through panels in container, marking the selected one and
        // unmarking the rest, and remembering if any are marked
        for (std::vector<PlanetPanel*>::iterator it = m_planet_panels.begin(); it != m_planet_panels.end(); ++it) {
            PlanetPanel* panel = *it;
            if (panel->PlanetID() == m_selected_planet_id) {
                panel->Select(true);
                planet_id_match_found = true;
                //std::cout << " ... selecting planet with id " << panel->PlanetID() << std::endl;
            } else {
                panel->Select(false);
            }
        }

        // if a panel was marked, signal this fact
        if (!planet_id_match_found) {
            m_selected_planet_id = INVALID_OBJECT_ID;
            //std::cout << " ... no planet with requested ID found" << std::endl;
        }
    }
}

void SidePanel::PlanetPanelContainer::SetValidSelectionPredicate(const boost::shared_ptr<UniverseObjectVisitor>& visitor)
{ m_valid_selection_predicate = visitor; }

void SidePanel::PlanetPanelContainer::DisableNonSelectionCandidates() {
    //std::cout << "SidePanel::PlanetPanelContainer::DisableNonSelectionCandidates" << std::endl;
    m_candidate_ids.clear();
    std::set<PlanetPanel*>   disabled_panels;

    if (m_valid_selection_predicate) {
        // if there is a selection predicate, which determines which planet panels
        // can be selected, refresh the candidiates and disable the non-selectables

        // find selectables
        for (std::vector<PlanetPanel*>::iterator it = m_planet_panels.begin(); it != m_planet_panels.end(); ++it) {
            PlanetPanel*    panel =     *it;
            int             planet_id = panel->PlanetID();
            const Planet*   planet =    GetPlanet(planet_id);

            if (planet && planet->Accept(*m_valid_selection_predicate)) {
                m_candidate_ids.insert(planet_id);
                //std::cout << " ... planet " << planet->ID() << " is a selection candidate" << std::endl;
            } else {
                disabled_panels.insert(panel);
            }
        }
    }

    // disable and enabled appropriate panels
    for (std::vector<PlanetPanel*>::iterator it = m_planet_panels.begin(); it != m_planet_panels.end(); ++it) {
        PlanetPanel* panel = *it;
        if (disabled_panels.find(panel) != disabled_panels.end()) {
            panel->Disable(true);
            //std::cout << " ... DISABLING PlanetPanel for planet " << panel->PlanetID() << std::endl;
        } else {
            panel->Disable(false);
        }
    }
}

void SidePanel::PlanetPanelContainer::PlanetPanelClicked(int planet_id) {
    //Logger().debugStream() << "SidePanel::PlanetPanelContainer::PlanetPanelClicked(" << planet_id << ")";
    PlanetSelectedSignal(planet_id);
}

void SidePanel::PlanetPanelContainer::VScroll(int pos_top, int pos_bottom, int range_min, int range_max) {
    if (pos_bottom > range_max) {
        // prevent scrolling beyond allowed max
        int extra = pos_bottom - range_max;
        pos_top -= extra;
    }
    DoPanelsLayout(GG::Y(-pos_top));    // scrolling bar down pos_top pixels causes the panels to move up that many pixels
}

void SidePanel::PlanetPanelContainer::RefreshAllPlanetPanels() {
    //std::cout << "SidePanel::PlanetPanelContainer::RefreshAllPlanetPanels.  selected planet id: " << m_selected_planet_id << std::endl;
    for (std::vector<PlanetPanel*>::iterator it = m_planet_panels.begin(); it != m_planet_panels.end(); ++it)
        (*it)->Refresh();
    //std::cout << "  ...  after refreshing all planet panels: selected planet id: " << m_selected_planet_id << std::endl<< std::endl<< std::endl;
}

void SidePanel::PlanetPanelContainer::SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
    GG::Pt old_size = GG::Wnd::Size();

    GG::Wnd::SizeMove(ul, lr);

    if (old_size != GG::Wnd::Size())
        DoLayout();
}

void SidePanel::PlanetPanelContainer::EnableOrderIssuing(bool enable/* = true*/) {
    for (std::vector<PlanetPanel*>::iterator it = m_planet_panels.begin(); it != m_planet_panels.end(); ++it) {
        PlanetPanel* panel = *it;
        panel->EnableOrderIssuing(enable);
    }
}

////////////////////////////////////////////////
// SidePanel
////////////////////////////////////////////////
// static(s)
int                                         SidePanel::s_system_id = INVALID_OBJECT_ID;
std::set<SidePanel*>                        SidePanel::s_side_panels;
std::set<boost::signals::connection>        SidePanel::s_system_connections;
std::map<int, boost::signals::connection>   SidePanel::s_fleet_state_change_signals;
boost::signal<void ()>                      SidePanel::ResourceCenterChangedSignal;
boost::signal<void (int)>                   SidePanel::PlanetSelectedSignal;
boost::signal<void (int)>                   SidePanel::SystemSelectedSignal;


SidePanel::SidePanel(GG::X x, GG::Y y, GG::Y h) :
    CUIWnd("SidePanel", x, y, GG::X(GetOptionsDB().Get<int>("UI.sidepanel-width")), h,
           GG::INTERACTIVE | GG::RESIZABLE | GG::DRAGABLE | GG::ONTOP),
    m_system_name(0),
    m_star_type_text(0),
    m_button_prev(0),
    m_button_next(0),
    m_star_graphic(0),
    m_planet_panel_container(0),
    m_system_resource_summary(0),
    m_selection_enabled(false)
{
    const boost::shared_ptr<GG::Font>& font = ClientUI::GetFont(SystemNameFontSize());
    const GG::Y DROP_DISPLAYED_LIST_HEIGHT = GG::Y(10*SystemNameFontSize());

    m_planet_panel_container = new PlanetPanelContainer(GG::X0, GG::Y0, GG::X1, GG::Y1);
    AttachChild(m_planet_panel_container);

    m_button_prev = new GG::Button(             GG::X0, GG::Y0, ButtonWidth(),  SystemNameTextControlHeight(),  "", font, GG::CLR_WHITE);
    m_button_next = new GG::Button(             GG::X0, GG::Y0, ButtonWidth(),  SystemNameTextControlHeight(),  "", font, GG::CLR_WHITE);
    m_system_name = new CUIDropDownList(        GG::X0, GG::Y0, GG::X(40),      SystemNameTextControlHeight(),  DROP_DISPLAYED_LIST_HEIGHT, GG::CLR_ZERO, GG::FloatClr(0.0, 0.0, 0.0, 0.5));
    m_star_type_text = new ShadowedTextControl( GG::X0, GG::Y0, "", ClientUI::GetFont(), ClientUI::TextColor(), GG::FORMAT_CENTER);

    Sound::TempUISoundDisabler sound_disabler;

    SetName(UserString("SIDE_PANEL"));

    m_system_name->DisableDropArrow();
    m_system_name->SetStyle(GG::LIST_CENTER);
    m_system_name->SetInteriorColor(GG::Clr(0, 0, 0, 200));
    m_system_name->SetNumCols(1);
    m_system_name->SetColWidth(0, GG::X0);
    m_system_name->LockColWidths();
    AttachChild(m_system_name);

    AttachChild(m_star_type_text);

    m_button_prev->SetUnpressedGraphic(GG::SubTexture(ClientUI::GetTexture( ClientUI::ArtDir() / "icons" / "leftarrownormal.png"   ), GG::X0, GG::Y0, GG::X(32), GG::Y(32)));
    m_button_prev->SetPressedGraphic  (GG::SubTexture(ClientUI::GetTexture( ClientUI::ArtDir() / "icons" / "leftarrowclicked.png"  ), GG::X0, GG::Y0, GG::X(32), GG::Y(32)));
    m_button_prev->SetRolloverGraphic (GG::SubTexture(ClientUI::GetTexture( ClientUI::ArtDir() / "icons" / "leftarrowmouseover.png"), GG::X0, GG::Y0, GG::X(32), GG::Y(32)));
    AttachChild(m_button_prev);

    m_button_next->SetUnpressedGraphic(GG::SubTexture(ClientUI::GetTexture( ClientUI::ArtDir() / "icons" / "rightarrownormal.png"  ), GG::X0, GG::Y0, GG::X(32), GG::Y(32)));
    m_button_next->SetPressedGraphic  (GG::SubTexture(ClientUI::GetTexture( ClientUI::ArtDir() / "icons" / "rightarrowclicked.png"   ), GG::X0, GG::Y0, GG::X(32), GG::Y(32)));
    m_button_next->SetRolloverGraphic (GG::SubTexture(ClientUI::GetTexture( ClientUI::ArtDir() / "icons" / "rightarrowmouseover.png"), GG::X0, GG::Y0, GG::X(32), GG::Y(32)));
    AttachChild(m_button_next);

    m_system_resource_summary = new MultiIconValueIndicator(Width() - EDGE_PAD*2);
    AttachChild(m_system_resource_summary);


    GG::Connect(m_system_name->SelChangedSignal,                &SidePanel::SystemSelectionChanged, this);
    GG::Connect(m_button_prev->ClickedSignal,                   &SidePanel::PrevButtonClicked,      this);
    GG::Connect(m_button_next->ClickedSignal,                   &SidePanel::NextButtonClicked,      this);
    GG::Connect(m_planet_panel_container->PlanetSelectedSignal, &SidePanel::PlanetSelected,         this);

    SetMinSize(GG::Pt(GG::X(MaxPlanetDiameter() + BORDER_LEFT + BORDER_RIGHT + 120),
                      PLANET_PANEL_TOP + GG::Y(MaxPlanetDiameter())));

    DoLayout();
    Hide();

    s_side_panels.insert(this);
}

SidePanel::~SidePanel() {
    // disconnect any existing stored signals
    while (!s_system_connections.empty()) {
        s_system_connections.begin()->disconnect();
        s_system_connections.erase(s_system_connections.begin());
    }
    while (!s_fleet_state_change_signals.empty()) {
        s_fleet_state_change_signals.begin()->second.disconnect();
        s_fleet_state_change_signals.erase(s_fleet_state_change_signals.begin());
    }
    s_side_panels.erase(this);

    delete m_star_graphic;
    delete m_system_resource_summary;
}

bool SidePanel::InWindow(const GG::Pt& pt) const {
    return (UpperLeft() + GG::Pt(GG::X(MaxPlanetDiameter()), GG::Y0) <= pt && pt < LowerRight())
           || (m_planet_panel_container && m_planet_panel_container->InWindow(pt))
           || (m_system_resource_summary && m_system_resource_summary->InWindow(pt));
}

GG::Pt SidePanel::ClientUpperLeft() const
{ return GG::Wnd::UpperLeft() + GG::Pt(BORDER_LEFT, BORDER_BOTTOM); }

void SidePanel::Render() {
    GG::Pt ul = UpperLeft() + GG::Pt(GG::X(MaxPlanetDiameter() + 2), GG::Y0);
    GG::Pt lr = LowerRight();

    GG::Pt cl_ul = ClientUpperLeft() + GG::Pt(GG::X(MaxPlanetDiameter() + 2), PLANET_PANEL_TOP);
    GG::Pt cl_lr = lr - GG::Pt(BORDER_RIGHT, BORDER_BOTTOM);

   // use GL to draw the lines
    glDisable(GL_TEXTURE_2D);
    GLint initial_modes[2];
    glGetIntegerv(GL_POLYGON_MODE, initial_modes);

    // draw background
    glPolygonMode(GL_BACK, GL_FILL);
    glBegin(GL_POLYGON);
        glColor(ClientUI::WndColor());
        glVertex(ul.x, ul.y);
        glVertex(lr.x, ul.y);
        glVertex(lr.x, lr.y - OUTER_EDGE_ANGLE_OFFSET);
        glVertex(lr.x - OUTER_EDGE_ANGLE_OFFSET, lr.y);
        glVertex(ul.x, lr.y);
        glVertex(ul.x, ul.y);
    glEnd();

    // draw outer border on pixel inside of the outer edge of the window
    glPolygonMode(GL_BACK, GL_LINE);
    glBegin(GL_POLYGON);
        glColor(ClientUI::WndOuterBorderColor());
        glVertex(ul.x, ul.y);
        glVertex(lr.x, ul.y);
        glVertex(lr.x, lr.y - OUTER_EDGE_ANGLE_OFFSET);
        glVertex(lr.x - OUTER_EDGE_ANGLE_OFFSET, lr.y);
        glVertex(ul.x, lr.y);
        glVertex(ul.x, ul.y);
    glEnd();

    // reset this to whatever it was initially
    glPolygonMode(GL_BACK, initial_modes[1]);

    // draw inner border
    if (cl_ul.y < cl_lr.y) {
        glBegin(GL_LINE_STRIP);
            glColor(ClientUI::WndInnerBorderColor());
            glVertex(cl_ul.x, cl_ul.y);
            glVertex(cl_lr.x, cl_ul.y);
            glVertex(cl_lr.x, cl_lr.y - INNER_BORDER_ANGLE_OFFSET);
            glVertex(cl_lr.x - INNER_BORDER_ANGLE_OFFSET, cl_lr.y);
            glVertex(cl_ul.x, cl_lr.y);
            glVertex(cl_ul.x, cl_ul.y);
        glEnd();
    }
    // resize hash lines
    glBegin(GL_LINES);
        glColor(ClientUI::WndInnerBorderColor());
        glVertex(cl_lr.x, cl_lr.y - RESIZE_HASHMARK1_OFFSET);
        glVertex(cl_lr.x - RESIZE_HASHMARK1_OFFSET, cl_lr.y);

        glVertex(cl_lr.x, cl_lr.y - RESIZE_HASHMARK2_OFFSET);
        glVertex(cl_lr.x - RESIZE_HASHMARK2_OFFSET, cl_lr.y);
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

void SidePanel::Update() {
    //std::cout << "SidePanel::Update" << std::endl;
    for (std::set<SidePanel*>::iterator it = s_side_panels.begin(); it != s_side_panels.end(); ++it)
        (*it)->UpdateImpl();
}

void SidePanel::UpdateImpl() {
    //std::cout << "SidePanel::UpdateImpl" << std::endl;
    if (m_system_resource_summary)
        m_system_resource_summary->Update();
    // update individual PlanetPanels in PlanetPanelContainer, then redo layout of panel container
    m_planet_panel_container->RefreshAllPlanetPanels();
}

void SidePanel::Refresh() {
    // disconnect any existing system and fleet signals
    for (std::set<boost::signals::connection>::iterator it = s_system_connections.begin(); it != s_system_connections.end(); ++it)
        it->disconnect();
    s_system_connections.clear();

    for (std::map<int, boost::signals::connection>::iterator it = s_fleet_state_change_signals.begin(); it != s_fleet_state_change_signals.end(); ++it)
        it->second.disconnect();
    s_fleet_state_change_signals.clear();


    // refresh individual panels' contents
    for (std::set<SidePanel*>::iterator it = s_side_panels.begin(); it != s_side_panels.end(); ++it)
        (*it)->RefreshImpl();


    // early exit if no valid system object to get or connect signals to
    if (s_system_id == INVALID_OBJECT_ID)
        return;


    // connect state changed and insertion signals for planets and fleets in system
    const System* system = GetSystem(s_system_id);
    if (!system) {
        Logger().errorStream() << "SidePanel::Refresh couldn't get system with id " << s_system_id;
        return;
    }

    std::vector<int> planet_ids = system->FindObjectIDs<Planet>();
    for (std::vector<int>::const_iterator it = planet_ids.begin(); it != planet_ids.end(); ++it)
        if (Planet* planet = GetPlanet(*it))
            s_system_connections.insert(GG::Connect(planet->ResourceCenterChangedSignal, SidePanel::ResourceCenterChangedSignal));

    std::vector<int> fleet_ids = system->FindObjectIDs<Fleet>();
    for (std::vector<int>::const_iterator it = fleet_ids.begin(); it != fleet_ids.end(); ++it)
        if (Fleet* fleet = GetFleet(*it))
            s_fleet_state_change_signals[*it] = GG::Connect(fleet->StateChangedSignal, &SidePanel::FleetStateChanged);

    //s_system_connections.insert(GG::Connect(s_system->StateChangedSignal,   &SidePanel::Update));
    s_system_connections.insert(GG::Connect(system->FleetInsertedSignal,    &SidePanel::FleetInserted));
    s_system_connections.insert(GG::Connect(system->FleetRemovedSignal,     &SidePanel::FleetRemoved));
}

void SidePanel::RefreshImpl() {
    //std::cout << "SidePanel::RefreshImpl" << std::endl;
    Sound::TempUISoundDisabler sound_disabler;


    // save initial scroll position so it can be restored after repopulating the planet panel container
    const int initial_scroll_pos = m_planet_panel_container->ScrollPosition();

    // save initial selected planet so it can be restored
    const int initial_selected_planet_id = m_planet_panel_container->SelectedPlanetID();


    // clear out current contents
    m_planet_panel_container->Clear();
    m_system_name->Clear();
    delete m_star_graphic;              m_star_graphic = 0;
    delete m_system_resource_summary;   m_system_resource_summary = 0;


    // get info with which to repopulate
    int app_empire_id = HumanClientApp::GetApp()->EmpireID();
    const System* system = GetSystem(s_system_id);
    // if no system object, there is nothing to populate with.  early abort.
    if (!system)
        return;


    // populate droplist of system names
    int system_names_in_droplist = 0;
    for (ObjectMap::const_iterator<System> sys_it = Objects().const_begin<System>();
         sys_it != Objects().const_end<System>(); ++sys_it)
    {
        int sys_id = sys_it->ID();

        if (sys_it->Name().empty() && sys_id != s_system_id)
            continue;   // skip rows for systems that aren't known to this client, except the selected system

        GG::DropDownList::iterator latest_it = m_system_name->Insert(new SystemRow(sys_id));
        ++system_names_in_droplist;

        if (sys_id == s_system_id)
            m_system_name->Select(latest_it);
    }

    // set dropheight.  shrink to fit a small number, but cap at a reasonable max
    const GG::Y TEXT_ROW_HEIGHT = CUISimpleDropDownListRow::DEFAULT_ROW_HEIGHT;
    const GG::Y MAX_DROPLIST_DROP_HEIGHT = TEXT_ROW_HEIGHT * 10;
    const int TOTAL_LISTBOX_MARGIN = 4;
    GG::Y drop_height = std::min(TEXT_ROW_HEIGHT * system_names_in_droplist, MAX_DROPLIST_DROP_HEIGHT) + TOTAL_LISTBOX_MARGIN;
    m_system_name->SetDropHeight(drop_height);


    // (re)create top right star graphic
    boost::shared_ptr<GG::Texture> graphic =
        ClientUI::GetClientUI()->GetModuloTexture(ClientUI::ArtDir() / "stars_sidepanel",
                                                  ClientUI::StarTypeFilePrefixes()[system->GetStarType()],
                                                  s_system_id);
    std::vector<boost::shared_ptr<GG::Texture> > textures;
    textures.push_back(graphic);

    int graphic_width = Value(Width()) - MaxPlanetDiameter();
    m_star_graphic = new GG::DynamicGraphic(GG::X(MaxPlanetDiameter()), GG::Y0,
                                            GG::X(graphic_width), GG::Y(graphic_width), true,
                                            textures[0]->DefaultWidth(), textures[0]->DefaultHeight(),
                                            0, textures, GG::GRAPHIC_FITGRAPHIC | GG::GRAPHIC_PROPSCALE);

    AttachChild(m_star_graphic);
    MoveChildDown(m_star_graphic);


    // star type
    m_star_type_text->SetText(GetStarTypeName(*system));


    // configure selection of planet panels in panel container
    boost::shared_ptr<UniverseObjectVisitor> vistor;
    if (m_selection_enabled) {
        int empire_id = HumanClientApp::GetApp()->EmpireID();
        if (empire_id != ALL_EMPIRES)
            vistor = boost::shared_ptr<UniverseObjectVisitor>(new OwnedVisitor<Planet>(empire_id));
    }
    m_planet_panel_container->SetValidSelectionPredicate(vistor);


    // update planet panel container contents (applying just-set selection predicate)
    //std::cout << " ... setting planet panel container planets" << std::endl;

    // find all planets in this system.  need to check all known planets, since
    // the objects this system object reports to contain don't include objects
    // that aren't visible this turn to this client's player.  Also need to
    // make sure that a known object isn't also known to be destroyed, in which
    // case it shouldn't be shown.
    const std::set<int>& destroyed_object_ids = GetUniverse().EmpireKnownDestroyedObjectIDs(app_empire_id);
    std::vector<int> known_system_planet_ids;
    const ObjectMap& objects = Objects();
    std::vector<const Planet*> all_planets = objects.FindObjects<Planet>();
    for (std::vector<const Planet*>::const_iterator it = all_planets.begin(); it != all_planets.end(); ++it) {
        const Planet* planet = *it;
        int planet_id = planet->ID();
        if (planet->SystemID() == s_system_id) {
            if (destroyed_object_ids.find(planet_id) == destroyed_object_ids.end()) // to be displayed, planet should not be in set of known destroyed objects
                known_system_planet_ids.push_back(planet_id);
        }
    }
    m_planet_panel_container->SetPlanets(known_system_planet_ids, system->GetStarType());


    // restore planet panel container scroll position from before clearing
    m_planet_panel_container->ScrollTo(initial_scroll_pos);

    // restore planet selection
    m_planet_panel_container->SelectPlanet(initial_selected_planet_id);


    // populate system resource summary

    // get planets owned by player's empire
    int empire_id = HumanClientApp::GetApp()->EmpireID();
    std::vector<int> owned_planets;
    for (std::vector<int>::const_iterator it = known_system_planet_ids.begin(); it != known_system_planet_ids.end(); ++it) {
        const Planet* planet = GetPlanet(*it);
        if (planet && planet->OwnedBy(empire_id))
            owned_planets.push_back(*it);
    }

    // specify which meter types to include in resource summary.  Oddly enough, these are the resource meters.
    std::vector<std::pair<MeterType, MeterType> > meter_types;
    meter_types.push_back(std::make_pair(METER_INDUSTRY,    METER_TARGET_INDUSTRY));
    meter_types.push_back(std::make_pair(METER_RESEARCH,    METER_TARGET_RESEARCH));
    meter_types.push_back(std::make_pair(METER_TRADE,       METER_TARGET_TRADE));

    // refresh the system resource summary.
    m_system_resource_summary = new MultiIconValueIndicator(Width() - MaxPlanetDiameter() - 8, owned_planets, meter_types);
    m_system_resource_summary->MoveTo(GG::Pt(GG::X(MaxPlanetDiameter() + 4), 140 - m_system_resource_summary->Height()));
    AttachChild(m_system_resource_summary);


    // add tooltips and show system resource summary if it is not empty
    if (m_system_resource_summary->Empty()) {
        DetachChild(m_system_resource_summary);
    } else {
        // add tooltips to the system resource summary
        for (std::vector<std::pair<MeterType, MeterType> >::const_iterator it = meter_types.begin(); it != meter_types.end(); ++it) {
            MeterType type = it->first;
            // add tooltip for each meter type
            boost::shared_ptr<GG::BrowseInfoWnd> browse_wnd = boost::shared_ptr<GG::BrowseInfoWnd>(
                new SystemResourceSummaryBrowseWnd(MeterToResource(type), s_system_id, HumanClientApp::GetApp()->EmpireID()));
            m_system_resource_summary->SetToolTip(type, browse_wnd);
        }

        AttachChild(m_system_resource_summary);
        m_system_resource_summary->Update();
    }

    DoLayout();
}

void SidePanel::DoLayout() {
    // left button
    GG::Pt ul(GG::X(MaxPlanetDiameter()) + 2*EDGE_PAD, GG::Y(EDGE_PAD));
    GG::Pt lr(ul + GG::Pt(ButtonWidth(), ButtonHeight()));
    m_button_prev->SizeMove(ul, lr);

    // right button
    ul = GG::Pt(ClientWidth() - ButtonWidth() - 2*EDGE_PAD, GG::Y(EDGE_PAD));
    lr = ul + GG::Pt(ButtonWidth(), ButtonHeight());
    m_button_next->SizeMove(ul, lr);

    // system name / droplist
    ul = GG::Pt(GG::X(MaxPlanetDiameter()), GG::Y0);
    lr = ul + GG::Pt(ClientWidth() - GG::X(MaxPlanetDiameter()), m_system_name->Height());
    m_system_name->SizeMove(ul, lr);

    // system name droplist rows
    GG::Pt row_size(ListRowSize());
    for (GG::ListBox::iterator it = m_system_name->begin(); it != m_system_name->end(); ++it)
        (*it)->Resize(row_size);

    // star type text
    ul = GG::Pt(GG::X(MaxPlanetDiameter()) + 2*EDGE_PAD, m_system_name->Height() + EDGE_PAD*4);
    lr = GG::Pt(ClientWidth() - 1, ul.y + m_star_type_text->Height());
    m_star_type_text->SizeMove(ul, lr);

    // resize planet panel container
    ul = GG::Pt(BORDER_LEFT, PLANET_PANEL_TOP);
    lr = GG::Pt(ClientWidth() - 1, ClientHeight() - GG::Y(INNER_BORDER_ANGLE_OFFSET));
    m_planet_panel_container->SizeMove(ul, lr);

    // resize system resource summary
    if (m_system_resource_summary) {
        ul = GG::Pt(GG::X(EDGE_PAD + 1), PLANET_PANEL_TOP - m_system_resource_summary->Height());
        lr = ul + GG::Pt(ClientWidth() - EDGE_PAD - 1, m_system_resource_summary->Height());
        m_system_resource_summary->SizeMove(ul, lr);
    }
}

GG::Pt SidePanel::ListRowSize() const
{ return GG::Pt(m_system_name->Width() - ClientUI::ScrollWidth() - 5, m_system_name->Height()); }

void SidePanel::SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
    GG::Pt old_size = GG::Wnd::Size();

    GG::Wnd::SizeMove(ul, lr);

    if (old_size != GG::Wnd::Size())
        DoLayout();
}

void SidePanel::SystemSelectionChanged(GG::DropDownList::iterator it) {
    int system_id = INVALID_OBJECT_ID;
    if (it != m_system_name->end())
        system_id = boost::polymorphic_downcast<const SystemRow*>(*it)->SystemID();
    if (SystemID() != system_id)
        SystemSelectedSignal(system_id);
}

void SidePanel::PrevButtonClicked() {
    assert(!m_system_name->Empty());
    GG::DropDownList::iterator selected = m_system_name->CurrentItem();
    if (selected == m_system_name->begin())
        selected = m_system_name->end();
    m_system_name->Select(--selected);
    SystemSelectionChanged(m_system_name->CurrentItem());
}

void SidePanel::NextButtonClicked() {
    assert(!m_system_name->Empty());
    GG::DropDownList::iterator selected = m_system_name->CurrentItem();
    if (++selected == m_system_name->end())
        selected = m_system_name->begin();
    m_system_name->Select(selected);
    SystemSelectionChanged(m_system_name->CurrentItem());
}

void SidePanel::PlanetSelected(int planet_id) {
    //std::cout << "SidePanel::PlanetSelected(" << planet_id << ")" << std::endl;
    if (SelectedPlanetID() != planet_id)
        PlanetSelectedSignal(planet_id);
}

void SidePanel::FleetInserted(Fleet& fleet) {
    //std::cout << "SidePanel::FleetInserted" << std::endl;
    s_fleet_state_change_signals[fleet.ID()].disconnect();  // in case already present
    s_fleet_state_change_signals[fleet.ID()] = GG::Connect(fleet.StateChangedSignal, &SidePanel::FleetStateChanged);
    SidePanel::Update();
}

void SidePanel::FleetRemoved(Fleet& fleet) {
    //std::cout << "SidePanel::FleetRemoved" << std::endl;
    std::map<int, boost::signals::connection>::iterator it = s_fleet_state_change_signals.find(fleet.ID());
    if (it != s_fleet_state_change_signals.end()) {
        it->second.disconnect();
        s_fleet_state_change_signals.erase(it);
    }
    SidePanel::Update();
}

void SidePanel::FleetStateChanged()
{ SidePanel::Update(); }

int SidePanel::SystemID()
{ return s_system_id; }

int SidePanel::SelectedPlanetID() const {
    if (m_planet_panel_container)
        return m_planet_panel_container->SelectedPlanetID();
    else
        return INVALID_OBJECT_ID;
}

bool SidePanel::PlanetSelectable(int id) const {
    if (!m_planet_panel_container)
        return false;
    const std::set<int>& candidate_ids = m_planet_panel_container->SelectionCandidates();
    return (candidate_ids.find(id) != candidate_ids.end());
}

void SidePanel::SelectPlanet(int planet_id) {
    for (std::set<SidePanel*>::iterator it = s_side_panels.begin(); it != s_side_panels.end(); ++it)
        (*it)->SelectPlanetImpl(planet_id);
}

void SidePanel::SelectPlanetImpl(int planet_id) {
    //std::cout << "SidePanel::SelectPlanetImpl(" << planet_id << ")" << std::endl;
    m_planet_panel_container->SelectPlanet(planet_id);
}

void SidePanel::SetSystem(int system_id) {
    if (s_system_id == system_id)
        return;
    s_system_id = system_id;

    if (GetSystem(s_system_id))
        PlaySidePanelOpenSound();

    // refresh sidepanels
    Refresh();
}

void SidePanel::EnableSelection(bool enable/* = true*/)
{ m_selection_enabled = enable; }

void SidePanel::EnableOrderIssuing(bool enable/* = true*/)
{ m_planet_panel_container->EnableOrderIssuing(enable); }
