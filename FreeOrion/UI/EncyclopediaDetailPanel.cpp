#include "EncyclopediaDetailPanel.h"

#include "CUIControls.h"
#include "../universe/Universe.h"
#include "../universe/Tech.h"
#include "../universe/ShipDesign.h"
#include "../universe/Building.h"
#include "../universe/Planet.h"
#include "../universe/System.h"
#include "../universe/Ship.h"
#include "../universe/Fleet.h"
#include "../universe/Special.h"
#include "../universe/Species.h"
#include "../universe/Effect.h"
#include "../Empire/Empire.h"
#include "../Empire/EmpireManager.h"
#include "../util/MultiplayerCommon.h"
#include "../util/OptionsDB.h"
#include "../client/human/HumanClientApp.h"
#include "../UI/DesignWnd.h"

#include <GG/DrawUtil.h>
#include <GG/StaticGraphic.h>
#include <GG/GUI.h>

namespace {
    const GG::X TEXT_MARGIN_X(3);
    const GG::Y TEXT_MARGIN_Y(3);
    void    AddOptions(OptionsDB& db) {
        db.Add("UI.autogenerated-effects-descriptions", "OPTIONS_DB_AUTO_EFFECT_DESC",  false,  Validator<bool>());
    }
    bool temp_bool = RegisterOptions(&AddOptions);

    const std::string EMPTY_STRING;
    const std::string INCOMPLETE_DESIGN = "incomplete design";
    const std::string UNIVERSE_OBJECT = "universe object";
}

namespace {
    std::string LinkTaggedText(const std::string& tag, const std::string& stringtable_entry) {
        return "<" + tag + " " + stringtable_entry + ">" + UserString(stringtable_entry) + "</" + tag + ">" + "\n";
    }

    std::string LinkTaggedIDText(const std::string& tag, int id, const std::string& text) {
        return "<" + tag + " " + boost::lexical_cast<std::string>(id) + ">" + text + "</" + tag + ">" + "\n";
    }

    std::string PediaDirText(const std::string& dir_name) {
        std::string retval;

        if (dir_name == "ENC_INDEX") {
            retval += LinkTaggedText(TextLinker::ENCYCLOPEDIA_TAG, "ENC_SHIP_PART");
            retval += LinkTaggedText(TextLinker::ENCYCLOPEDIA_TAG, "ENC_SHIP_HULL");
            retval += LinkTaggedText(TextLinker::ENCYCLOPEDIA_TAG, "ENC_TECH");
            retval += LinkTaggedText(TextLinker::ENCYCLOPEDIA_TAG, "ENC_BUILDING_TYPE");
            retval += LinkTaggedText(TextLinker::ENCYCLOPEDIA_TAG, "ENC_SPECIAL");
            retval += LinkTaggedText(TextLinker::ENCYCLOPEDIA_TAG, "ENC_SPECIES");

        } else if (dir_name == "ENC_SHIP_PART") {
            const PartTypeManager& part_type_manager = GetPartTypeManager();
            for (PartTypeManager::iterator it = part_type_manager.begin(); it != part_type_manager.end(); ++it)
                retval += LinkTaggedText(VarText::SHIP_PART_TAG, it->first);

        } else if (dir_name == "ENC_SHIP_HULL") {
            const HullTypeManager& hull_type_manager = GetHullTypeManager();
            for (HullTypeManager::iterator it = hull_type_manager.begin(); it != hull_type_manager.end(); ++it)
                retval += LinkTaggedText(VarText::SHIP_HULL_TAG, it->first);

        } else if (dir_name == "ENC_TECH") {
            std::vector<std::string> tech_names = GetTechManager().TechNames();
            for (std::vector<std::string>::const_iterator it = tech_names.begin(); it != tech_names.end(); ++it)
                retval += LinkTaggedText(VarText::TECH_TAG, *it);

        } else if (dir_name == "ENC_BUILDING_TYPE") {
            const BuildingTypeManager& building_type_manager = GetBuildingTypeManager();
            for (BuildingTypeManager::iterator it = building_type_manager.begin(); it != building_type_manager.end(); ++it)
                retval += LinkTaggedText(VarText::BUILDING_TYPE_TAG, it->first);

        } else if (dir_name == "ENC_SPECIAL") {
            const std::vector<std::string> special_names = SpecialNames();
            for (std::vector<std::string>::const_iterator it = special_names.begin(); it != special_names.end(); ++it)
                retval += LinkTaggedText(VarText::SPECIAL_TAG, *it);

        } else if (dir_name == "ENC_SPECIES") {
            const SpeciesManager& species_manager = GetSpeciesManager();
            for (SpeciesManager::iterator it = species_manager.begin(); it != species_manager.end(); ++it)
                retval += LinkTaggedText(VarText::SPECIES_TAG, it->first);

        } else if (dir_name == "ENC_EMPIRE") {
            const EmpireManager& empire_manager = Empires();
            for (EmpireManager::const_iterator it = empire_manager.begin(); it != empire_manager.end(); ++it)
                retval += LinkTaggedIDText(VarText::EMPIRE_ID_TAG, it->first, it->second->Name());

        } else if (dir_name == "ENC_SHIP_DESIGN") {
            const Universe& universe = GetUniverse();
            for (Universe::ship_design_iterator it = universe.beginShipDesigns(); it != universe.endShipDesigns(); ++it)
                retval += LinkTaggedIDText(VarText::DESIGN_ID_TAG, it->first, it->second->Name());
        }

        return retval;
    }
}

std::list <std::pair<std::string, std::string> >            EncyclopediaDetailPanel::m_items = std::list<std::pair<std::string, std::string> >(0);
std::list <std::pair<std::string, std::string> >::iterator  EncyclopediaDetailPanel::m_items_it = m_items.begin();

EncyclopediaDetailPanel::EncyclopediaDetailPanel(GG::X w, GG::Y h) :
    CUIWnd("", GG::X1, GG::Y1, w - 1, h - 1, GG::ONTOP | GG::INTERACTIVE | GG::DRAGABLE | GG::RESIZABLE),
    m_name_text(0),
    m_cost_text(0),
    m_summary_text(0),
    m_description_box(0),
    m_icon(0),
    m_other_icon(0)
{
    const int PTS = ClientUI::Pts();
    const int NAME_PTS = PTS*3/2;
    const int COST_PTS = PTS;
    const int SUMMARY_PTS = PTS*4/3;

    m_name_text =       new GG::TextControl(GG::X0, GG::Y0, GG::X(10), GG::Y(10), "", ClientUI::GetBoldFont(NAME_PTS),  ClientUI::TextColor());
    m_cost_text =       new GG::TextControl(GG::X0, GG::Y0, GG::X(10), GG::Y(10), "", ClientUI::GetFont(COST_PTS),      ClientUI::TextColor());
    m_summary_text =    new GG::TextControl(GG::X0, GG::Y0, GG::X(10), GG::Y(10), "", ClientUI::GetFont(SUMMARY_PTS),   ClientUI::TextColor());
    m_up_button =       new CUIButton(GG::X0, GG::Y0, GG::X(30), UserString("UP"));
    m_back_button =     new CUIButton(GG::X0, GG::Y0, GG::X(30), UserString("BACK"));
    m_next_button =     new CUIButton(GG::X0, GG::Y0, GG::X(30), UserString("NEXT"));

    CUILinkTextMultiEdit* desc_box = new CUILinkTextMultiEdit(GG::X0, GG::Y0, GG::X(10), GG::Y(10), "", GG::MULTI_WORDBREAK | GG::MULTI_READ_ONLY);
    GG::Connect(desc_box->LinkClickedSignal,        &EncyclopediaDetailPanel::HandleLinkClick,          this);
    GG::Connect(desc_box->LinkDoubleClickedSignal,  &EncyclopediaDetailPanel::HandleLinkDoubleClick,    this);
    GG::Connect(desc_box->LinkRightClickedSignal,   &EncyclopediaDetailPanel::HandleLinkDoubleClick,    this);
    GG::Connect(m_up_button->ClickedSignal,         &EncyclopediaDetailPanel::OnUp,                     this);
    GG::Connect(m_back_button->ClickedSignal,       &EncyclopediaDetailPanel::OnBack,                   this);
    GG::Connect(m_next_button->ClickedSignal,       &EncyclopediaDetailPanel::OnNext,                   this);
    m_description_box = desc_box;
    m_description_box->SetColor(GG::CLR_ZERO);
    m_description_box->SetInteriorColor(ClientUI::CtrlColor());

    AttachChild(m_name_text);
    AttachChild(m_cost_text);
    AttachChild(m_summary_text);
    AttachChild(m_description_box);
    AttachChild(m_up_button);
    AttachChild(m_back_button);
    AttachChild(m_next_button);

    SetChildClippingMode(ClipToClientAndWindowSeparately);

    DoLayout();

    AddItem(TextLinker::ENCYCLOPEDIA_TAG, "ENC_INDEX");
}

void EncyclopediaDetailPanel::DoLayout() {
    const int PTS = ClientUI::Pts();
    const int NAME_PTS = PTS*3/2;
    const int COST_PTS = PTS;
    const int SUMMARY_PTS = PTS*4/3;

    const int ICON_SIZE = 12 + NAME_PTS + COST_PTS + SUMMARY_PTS;

    const int BTN_WIDTH = 40;

    // name
    GG::Pt ul = GG::Pt();
    GG::Pt lr = ul + GG::Pt(Width(), GG::Y(NAME_PTS + 4));
    m_name_text->SizeMove(ul, lr);

    // cost / turns
    ul += GG::Pt(GG::X0, m_name_text->Height());
    lr = ul + GG::Pt(Width(), GG::Y(COST_PTS + 4));
    m_cost_text->SizeMove(ul, lr);

    // one line summary
    ul += GG::Pt(GG::X0, m_cost_text->Height());
    lr = ul + GG::Pt(Width(), GG::Y(SUMMARY_PTS + 4));
    m_summary_text->SizeMove(ul, lr);

    // main verbose description (fluff, effects, unlocks, ...)
    ul = GG::Pt(BORDER_LEFT, ICON_SIZE + TEXT_MARGIN_Y + 1);
    lr = GG::Pt(Width() - BORDER_RIGHT, Height() - BORDER_BOTTOM*3 - PTS - 4);
    m_description_box->SizeMove(ul, lr);

    // "back" button
    ul = GG::Pt(Width() - BORDER_RIGHT*3 - BTN_WIDTH * 3 - 8, Height() - BORDER_BOTTOM*2 - PTS);
    lr = GG::Pt(Width() - BORDER_RIGHT*3 - BTN_WIDTH * 2 - 8, Height() - BORDER_BOTTOM*2);
    m_back_button->SizeMove(ul, lr);

    // "up" button
    ul = GG::Pt(Width() - BORDER_RIGHT*3 - BTN_WIDTH * 2 - 4, Height() - BORDER_BOTTOM*3 - PTS);
    lr = GG::Pt(Width() - BORDER_RIGHT*3 - BTN_WIDTH - 4, Height() - BORDER_BOTTOM*3);
    m_up_button->SizeMove(ul, lr);

    // "next" button
    ul = GG::Pt(Width() - BORDER_RIGHT*3 - BTN_WIDTH, Height() - BORDER_BOTTOM*2 - PTS);
    lr = GG::Pt(Width() - BORDER_RIGHT*3, Height() - BORDER_BOTTOM*2);
    m_next_button->SizeMove(ul, lr);

    // icon
    if (m_icon) {
        ul = GG::Pt(GG::X1, GG::Y1);
        lr = ul + GG::Pt(GG::X(ICON_SIZE), GG::Y(ICON_SIZE));
        m_icon->SizeMove(ul, lr);
    }
    // other icon
    if (m_other_icon) {
        lr = GG::Pt(Width() - BORDER_RIGHT, GG::Y(ICON_SIZE + 1));
        ul = lr - GG::Pt(GG::X(ICON_SIZE), GG::Y(ICON_SIZE));
        m_other_icon->SizeMove(ul, lr);
    }
}

void EncyclopediaDetailPanel::SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
    GG::Pt old_size = GG::Wnd::Size();

    // maybe later do something interesting with docking
    GG::Wnd::SizeMove(ul, lr);

    if (Visible() && old_size != GG::Wnd::Size())
        DoLayout();
}

GG::Pt EncyclopediaDetailPanel::ClientUpperLeft() const {
    return GG::Wnd::UpperLeft();
}

void EncyclopediaDetailPanel::Render() {
    GG::Pt ul = UpperLeft();
    GG::Pt lr = LowerRight();
    const GG::Y ICON_SIZE = m_summary_text->LowerRight().y - m_name_text->UpperLeft().y;
    GG::Pt cl_ul = ul + GG::Pt(BORDER_LEFT, ICON_SIZE + BORDER_BOTTOM); // BORDER_BOTTOM is the size of the border at the bottom of a standard CUIWnd
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

    // draw inner border, including extra resize-tab lines
    glBegin(GL_LINE_STRIP);
        glColor(ClientUI::WndInnerBorderColor());
        glVertex(cl_ul.x, cl_ul.y);
        glVertex(cl_lr.x, cl_ul.y);
        glVertex(cl_lr.x, cl_lr.y - INNER_BORDER_ANGLE_OFFSET);
        glVertex(cl_lr.x - INNER_BORDER_ANGLE_OFFSET, cl_lr.y);
        glVertex(cl_ul.x, cl_lr.y);
        glVertex(cl_ul.x, cl_ul.y);
    glEnd();
    glBegin(GL_LINES);
        // draw the extra lines of the resize tab
        glColor(ClientUI::WndInnerBorderColor());
        glVertex(cl_lr.x, cl_lr.y - RESIZE_HASHMARK1_OFFSET);
        glVertex(cl_lr.x - RESIZE_HASHMARK1_OFFSET, cl_lr.y);

        glVertex(cl_lr.x, cl_lr.y - RESIZE_HASHMARK2_OFFSET);
        glVertex(cl_lr.x - RESIZE_HASHMARK2_OFFSET, cl_lr.y);
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

void EncyclopediaDetailPanel::LDrag(const GG::Pt& pt, const GG::Pt& move, GG::Flags<GG::ModKey> mod_keys) {
    if (m_drag_offset != GG::Pt(-GG::X1, -GG::Y1)) {  // resize-dragging
        GG::Pt new_lr = pt - m_drag_offset;

        // constrain to within parent
        if (GG::Wnd* parent = Parent()) {
            GG::Pt max_lr = parent->ClientLowerRight();
            new_lr.x = std::min(new_lr.x, max_lr.x);
            new_lr.y = std::min(new_lr.y, max_lr.y);
        }

        Resize(new_lr - UpperLeft());

    } else {    // normal-dragging
        GG::Pt final_move = move;

        if (GG::Wnd* parent = Parent()) {
            GG::Pt ul = UpperLeft(), lr = LowerRight();
            GG::Pt new_ul = ul + move, new_lr = lr + move;

            GG::Pt min_ul = parent->ClientUpperLeft() + GG::Pt(GG::X1, GG::Y1);
            GG::Pt max_lr = parent->ClientLowerRight();
            GG::Pt max_ul = max_lr - Size();

            new_ul.x = std::max(min_ul.x, std::min(max_ul.x, new_ul.x));
            new_ul.y = std::max(min_ul.y, std::min(max_ul.y, new_ul.y));

            final_move = new_ul - ul;
        }

        GG::Wnd::LDrag(pt, final_move, mod_keys);
    }
}


void EncyclopediaDetailPanel::HandleLinkClick(const std::string& link_type, const std::string& data) {
    using boost::lexical_cast;
    try {

        if (link_type == VarText::PLANET_ID_TAG ||
            link_type == VarText::SYSTEM_ID_TAG ||
            link_type == VarText::FLEET_ID_TAG  ||
            link_type == VarText::SHIP_ID_TAG ||
            link_type == VarText::BUILDING_ID_TAG)
        {
            this->SetObject(data);

        } else if (link_type == VarText::EMPIRE_ID_TAG) {
            this->SetEmpire(lexical_cast<int>(data));
        } else if (link_type == VarText::DESIGN_ID_TAG) {
            this->SetDesign(lexical_cast<int>(data));

        } else if (link_type == VarText::TECH_TAG) {
            this->SetTech(data);
        } else if (link_type == VarText::BUILDING_TYPE_TAG) {
            this->SetBuildingType(data);
        } else if (link_type == VarText::SPECIAL_TAG) {
            this->SetSpecial(data);
        } else if (link_type == VarText::SHIP_HULL_TAG) {
            this->SetHullType(data);
        } else if (link_type == VarText::SHIP_PART_TAG) {
            this->SetPartType(data);
        } else if (link_type == VarText::SPECIES_TAG) {
            this->SetSpecies(data);
        } else if (link_type == TextLinker::ENCYCLOPEDIA_TAG) {
            this->SetText(data, false);
        }
    } catch (const boost::bad_lexical_cast&) {
        Logger().errorStream() << "EncyclopediaDetailPanel::HandleLinkClick caught lexical cast exception for link type: " << link_type << " and data: " << data;
    }
}

void EncyclopediaDetailPanel::HandleLinkDoubleClick(const std::string& link_type, const std::string& data) {
    using boost::lexical_cast;
    try {
        if (link_type == VarText::PLANET_ID_TAG) {
            ClientUI::GetClientUI()->ZoomToPlanet(lexical_cast<int>(data));
        } else if (link_type == VarText::SYSTEM_ID_TAG) {
            ClientUI::GetClientUI()->ZoomToSystem(lexical_cast<int>(data));
        } else if (link_type == VarText::FLEET_ID_TAG) {
            ClientUI::GetClientUI()->ZoomToFleet(lexical_cast<int>(data));
        } else if (link_type == VarText::SHIP_ID_TAG) {
            ClientUI::GetClientUI()->ZoomToShip(lexical_cast<int>(data));
        } else if (link_type == VarText::BUILDING_ID_TAG) {
            ClientUI::GetClientUI()->ZoomToBuilding(lexical_cast<int>(data));

        } else if (link_type == VarText::EMPIRE_ID_TAG) {
            ClientUI::GetClientUI()->ZoomToEmpire(lexical_cast<int>(data));
        } else if (link_type == VarText::DESIGN_ID_TAG) {
            ClientUI::GetClientUI()->ZoomToShipDesign(lexical_cast<int>(data));

        } else if (link_type == VarText::TECH_TAG) {
            ClientUI::GetClientUI()->ZoomToTech(data);
        } else if (link_type == VarText::BUILDING_TYPE_TAG) {
            ClientUI::GetClientUI()->ZoomToBuildingType(data);
        } else if (link_type == VarText::SPECIAL_TAG) {
            ClientUI::GetClientUI()->ZoomToSpecial(data);
        } else if (link_type == VarText::SHIP_HULL_TAG) {
            ClientUI::GetClientUI()->ZoomToShipHull(data);
        } else if (link_type == VarText::SHIP_PART_TAG) {
            ClientUI::GetClientUI()->ZoomToShipPart(data);
        } else if (link_type == VarText::SPECIES_TAG) {
            ClientUI::GetClientUI()->ZoomToSpecies(data);

        } else if (link_type == TextLinker::ENCYCLOPEDIA_TAG) {
            this->SetText(data, false);
        }
    } catch (const boost::bad_lexical_cast&) {
        Logger().errorStream() << "EncyclopediaDetailPanel::HandleLinkDoubleClick caught lexical cast exception for link type: " << link_type << " and data: " << data;
    }
}

void EncyclopediaDetailPanel::Refresh() {
    if (m_icon) {
        DeleteChild(m_icon);
        m_icon = 0;
    }
    if (m_other_icon) {
        DeleteChild(m_other_icon);
        m_other_icon = 0;
    }
    m_name_text->Clear();
    m_summary_text->Clear();
    m_cost_text->Clear();
    m_description_box->Clear();

    // get details of item as applicable in order to set summary, cost, description TextControls
    std::string name = "";
    boost::shared_ptr<GG::Texture> texture;
    boost::shared_ptr<GG::Texture> other_texture;
    int turns = -1;
    double cost = 0.0;
    std::string cost_units;             // "PP" or "RP" or empty string, depending on whether and what something costs
    std::string general_type;           // general type of thing being shown, eg. "Building" or "Ship Part"
    std::string specific_type;          // specific type of thing; thing's purpose.  eg. "Farming" or "Colonization".  May be left blank for things without specific types (eg. specials)
    std::string detailed_description;
    GG::Clr color(GG::CLR_ZERO);

    using boost::io::str;
    if (m_items.empty())
        return;

    if (m_items_it->first == TextLinker::ENCYCLOPEDIA_TAG) {
        // Encyclopedia texts (including directories)
        detailed_description = PediaDirText(m_items_it->second);
        if (detailed_description.empty()) {
            detailed_description = UserString(m_items_it->second);
            name = EMPTY_STRING;
        } else {
            name = UserString(m_items_it->second);
        }

    } else if (m_items_it->first == "ENC_SHIP_PART") {
        const PartType* part = GetPartType(m_items_it->second);
        if (!part) {
            Logger().errorStream() << "EncyclopediaDetailPanel::Refresh couldn't find part with name " << m_items_it->second;
            return;
        }

        // Ship Parts
        name = UserString(m_items_it->second);
        texture = ClientUI::PartTexture(m_items_it->second);
        turns = part->ProductionTime();
        cost = part->ProductionCost();
        cost_units = UserString("ENC_PP");
        general_type = UserString("ENC_SHIP_PART");
        specific_type = UserString(boost::lexical_cast<std::string>(part->Class()));

        detailed_description = UserString(part->Description()) + "\n\n" + part->StatDescription();
        if (GetOptionsDB().Get<bool>("UI.autogenerated-effects-descriptions") && !part->Effects().empty()) {
            detailed_description += str(FlexibleFormat(UserString("ENC_EFFECTS_STR")) % EffectsDescription(part->Effects()));
        }

    } else if (m_items_it->first == "ENC_SHIP_HULL") {
        const HullType* hull = GetHullType(m_items_it->second);
        if (!hull) {
            Logger().errorStream() << "EncyclopediaDetailPanel::Refresh couldn't find hull with name " << m_items_it->second;
            return;
        }

        // Ship Hulls
        name = UserString(m_items_it->second);
        texture = ClientUI::HullTexture(m_items_it->second);
        turns = hull->ProductionTime();
        cost = hull->ProductionCost();
        cost_units = UserString("ENC_PP");
        general_type = UserString("ENC_SHIP_HULL");

        // hulls have no specific types
        detailed_description = UserString(hull->Description()) + "\n\n" + hull->StatDescription();
        if (GetOptionsDB().Get<bool>("UI.autogenerated-effects-descriptions") && !hull->Effects().empty()) {
            detailed_description += str(FlexibleFormat(UserString("ENC_EFFECTS_STR")) % EffectsDescription(hull->Effects()));
        }

    } else if (m_items_it->first == "ENC_TECH") {
        const Tech* tech = GetTech(m_items_it->second);
        if (!tech) {
            Logger().errorStream() << "EncyclopediaDetailPanel::Refresh couldn't find tech with name " << m_items_it->second;
            return;
        }

        // Technologies
        name = UserString(m_items_it->second);
        texture = ClientUI::TechTexture(m_items_it->second);
        other_texture = ClientUI::CategoryIcon(tech->Category()); 
        color = ClientUI::CategoryColor(tech->Category());
        turns = tech->ResearchTime();
        cost = tech->ResearchCost();
        cost_units = UserString("ENC_RP");
        general_type = str(FlexibleFormat(UserString("ENC_TECH_DETAIL_TYPE_STR"))
            % UserString(tech->Category())
            % UserString(boost::lexical_cast<std::string>(tech->Type()))
            % UserString(tech->ShortDescription()));

        detailed_description = UserString(tech->Description());
        if (GetOptionsDB().Get<bool>("UI.autogenerated-effects-descriptions") && !tech->Effects().empty()) {
            detailed_description += str(FlexibleFormat(UserString("ENC_EFFECTS_STR")) % EffectsDescription(tech->Effects()));
        }

        const std::vector<ItemSpec>& unlocked_items = tech->UnlockedItems();
        if (!unlocked_items.empty()) {
            detailed_description += UserString("ENC_TECH_DETAIL_UNLOCKS_SECTION_STR");
            for (unsigned int i = 0; i < unlocked_items.size(); ++i) {
                const ItemSpec& item = unlocked_items[i];

                std::string TAG;
                switch (item.type) {
                case UIT_BUILDING:  TAG = VarText::BUILDING_TYPE_TAG;   break;
                case UIT_SHIP_PART: TAG = VarText::SHIP_PART_TAG;       break;
                case UIT_SHIP_HULL: TAG = VarText::SHIP_HULL_TAG;       break;
                case UIT_TECH:      TAG = VarText::TECH_TAG;            break;
                }

                std::string link_text;
                if (!TAG.empty()) {
                    link_text = "<" + TAG + " " + item.name + ">"
                                + UserString(item.name)
                                + "</" + TAG + ">";
                } else {
                    link_text = UserString(item.name);
                }

                detailed_description += str(FlexibleFormat(UserString("ENC_TECH_DETAIL_UNLOCKED_ITEM_STR"))
                    % UserString(boost::lexical_cast<std::string>(unlocked_items[i].type))
                    % link_text);
            }
        }

    } else if (m_items_it->first == "ENC_BUILDING_TYPE") {
        const BuildingType* building_type = GetBuildingType(m_items_it->second);
        if (!building_type) {
            Logger().errorStream() << "EncyclopediaDetailPanel::Refresh couldn't find building type with name " << m_items_it->second;
            return;
        }

        // Buildings
        name = UserString(m_items_it->second);
        texture = ClientUI::BuildingTexture(m_items_it->second);
        turns = building_type->ProductionTime();
        cost = building_type->ProductionCost();
        cost_units = UserString("ENC_PP");
        general_type = UserString("ENC_BUILDING_TYPE");

        detailed_description = UserString(building_type->Description());
        if (GetOptionsDB().Get<bool>("UI.autogenerated-effects-descriptions") && !building_type->Effects().empty()) {
            detailed_description += str(FlexibleFormat(UserString("ENC_EFFECTS_STR")) % EffectsDescription(building_type->Effects()));
        }

    } else if (m_items_it->first == "ENC_SPECIAL") {
        const Special* special = GetSpecial(m_items_it->second);
        if (!special) {
            Logger().errorStream() << "EncyclopediaDetailPanel::Refresh couldn't find special with name " << m_items_it->second;
            return;
        }

        // Specials
        name = UserString(m_items_it->second);
        texture = ClientUI::SpecialTexture(m_items_it->second);
        detailed_description = UserString(special->Description());
        if (GetOptionsDB().Get<bool>("UI.autogenerated-effects-descriptions") && !special->Effects().empty()) {
            detailed_description += str(FlexibleFormat(UserString("ENC_EFFECTS_STR")) % EffectsDescription(special->Effects()));
        }

    } else if (m_items_it->first == "ENC_SPECIES") {
        const Species* species = GetSpecies(m_items_it->second);
        if (!species) {
            Logger().errorStream() << "EncyclopediaDetailPanel::Refresh couldn't find species with name " << m_items_it->second;
            return;
        }

        // Species
        name = UserString(m_items_it->second);
        texture = ClientUI::SpeciesIcon(m_items_it->second);
        general_type = UserString("ENC_SPECIES");

        detailed_description = UserString(species->Description());
        if (GetOptionsDB().Get<bool>("UI.autogenerated-effects-descriptions") && !species->Effects().empty()) {
            detailed_description += str(FlexibleFormat(UserString("ENC_EFFECTS_STR")) % EffectsDescription(species->Effects()));
        }

    } else if (m_items_it->first == "ENC_SHIP_DESIGN") {
        if (boost::lexical_cast<int>(m_items_it->second) != UniverseObject::INVALID_OBJECT_ID) {
            const ShipDesign* design = GetShipDesign(boost::lexical_cast<int>(m_items_it->second));
            if (!design) {
                Logger().errorStream() << "EncyclopediaDetailPanel::Refresh couldn't find ShipDesign with id " << m_items_it->second;
                return;
            }

            // Ship Designs
            name = design->Name();
            texture = ClientUI::ShipIcon(design->ID());
            turns = design->ProductionTime();
            cost = design->PerTurnCost();
            cost_units = UserString("ENC_PP");
            general_type = UserString("ENC_SHIP_DESIGN");
            detailed_description = str(FlexibleFormat(UserString("ENC_SHIP_DESIGN_DESCRIPTION_STR"))
                % design->Description()
                % static_cast<int>(design->SRWeapons().size())
                % static_cast<int>(design->LRWeapons().size())
                % static_cast<int>(design->FWeapons().size())
                % static_cast<int>(design->PDWeapons().size())
                % design->Structure()
                % design->Shields()
                % design->Detection()
                % design->BattleSpeed()
                % design->StarlaneSpeed()
                % design->Fuel()
                % design->ColonyCapacity()
                % design->Stealth());
        }

    } else if (m_items_it->first == INCOMPLETE_DESIGN) {
        boost::shared_ptr<const ShipDesign> incomplete_design = m_incomplete_design.lock();
        if (incomplete_design) {
            // incomplete design.  not yet in game universe; being created on design screen
            name = incomplete_design->Name();

            texture = ClientUI::GetTexture(ClientUI::ArtDir() / incomplete_design->Graphic(), true);
            if (!texture) {
                if (const HullType* hull_type = incomplete_design->GetHull())
                    texture = ClientUI::HullTexture(hull_type->Name());
            } else {
                texture = ClientUI::HullTexture("");
            }

            turns = incomplete_design->ProductionTime();
            cost = incomplete_design->ProductionCost();
            cost_units = UserString("ENC_PP");

            detailed_description = str(FlexibleFormat(UserString("ENC_SHIP_DESIGN_DESCRIPTION_STR"))
                % incomplete_design->Description()
                % static_cast<int>(incomplete_design->SRWeapons().size())
                % static_cast<int>(incomplete_design->LRWeapons().size())
                % static_cast<int>(incomplete_design->FWeapons().size())
                % static_cast<int>(incomplete_design->PDWeapons().size())
                % incomplete_design->Structure()
                % incomplete_design->Shields()
                % incomplete_design->Detection()
                % incomplete_design->BattleSpeed()
                % incomplete_design->StarlaneSpeed()
                % incomplete_design->Fuel()
                % incomplete_design->ColonyCapacity()
                % incomplete_design->Stealth());
        }

        general_type = UserString("ENC_INCOMPETE_SHIP_DESIGN");

    } else if (m_items_it->first == UNIVERSE_OBJECT) {
        int id = boost::lexical_cast<int>(m_items_it->second);

        if (id != UniverseObject::INVALID_OBJECT_ID) {
            const UniverseObject* obj = GetUniverse().Objects().Object(id);
            if (!obj) {
                int empire_id = HumanClientApp::GetApp()->EmpireID();
                if (empire_id == ALL_EMPIRES) {
                    Logger().errorStream() << "EncyclopediaDetailPanel::Refresh got invalid client's empire id.";
                    return;
                }
                obj = GetUniverse().EmpireKnownObjects(empire_id).Object(id);
            }

            if (!obj) {
                Logger().errorStream() << "EncyclopediaDetailPanel::Refresh couldn't find UniverseObject with id " << m_items_it->second;
                return;
            }

            if (const Ship* ship = universe_object_cast<const Ship*>(obj)) {
                name = ship->Name();
                general_type = UserString("ENC_SHIP");

            } else if (const Fleet* fleet = universe_object_cast<const Fleet*>(obj)) {
                name = fleet->Name();
                general_type = UserString("ENC_FLEET");

            } else if (const Planet* planet = universe_object_cast<const Planet*>(obj)) {
                name = planet->Name();
                general_type = UserString("ENC_PLANET");

            } else if (const Building* building = universe_object_cast<const Building*>(obj)) {
                name = building->Name();
                general_type = UserString("ENC_BUILDING");

            } else if (const System* system = universe_object_cast<const System*>(obj)) {
                name = system->Name();
                general_type = UserString("ENC_SYSTEM");

            } else {
                Logger().errorStream() << "EncyclopediaDetailPanel::Refresh couldn't interpret object: " << obj->Name() << " (" << m_items_it->second << ")";
                return;
            }
        }
    }

    // Create Icons
    if (texture) {
        m_icon =        new GG::StaticGraphic(GG::X0, GG::Y0, GG::X(10), GG::Y(10), texture,        GG::GRAPHIC_FITGRAPHIC | GG::GRAPHIC_PROPSCALE);
        if (color != GG::CLR_ZERO)
            m_icon->SetColor(color);
    }
    if (other_texture) {
        m_other_icon =  new GG::StaticGraphic(GG::X0, GG::Y0, GG::X(10), GG::Y(10), other_texture,  GG::GRAPHIC_FITGRAPHIC | GG::GRAPHIC_PROPSCALE);
        if (color != GG::CLR_ZERO)
            m_other_icon->SetColor(color);
    }

    if (m_icon) {
        m_icon->Show();
        AttachChild(m_icon);
    }
    if (m_other_icon) {
        m_other_icon->Show();
        AttachChild(m_other_icon);
    }

    // Set Text
    if (!name.empty())
        m_name_text->SetText(name);

    if (!specific_type.empty())
        m_summary_text->SetText(str(FlexibleFormat(UserString("ENC_DETAIL_TYPE_STR"))
            % specific_type
            % general_type));
    if (color != GG::CLR_ZERO)
        m_summary_text->SetColor(color);

    if (cost != 0.0 && turns != -1) {
        m_cost_text->SetText(str(FlexibleFormat(UserString("ENC_COST_AND_TURNS_STR"))
            % DoubleToString(cost, 3, false)
            % cost_units
            % turns));
    }

    if (!detailed_description.empty())
        m_description_box->SetText(detailed_description);

    DoLayout();
}

void EncyclopediaDetailPanel::AddItem(const std::string& type, const std::string& name) {
    // if the actual item is not the last one, all aubsequented items are deleted
    if (!m_items.empty()) {
        std::list<std::pair <std::string, std::string> >::iterator end = m_items.end();
        end--;
        if (m_items_it != end) {
            std::list<std::pair <std::string, std::string> >::iterator i = m_items_it;
            ++i;
            m_items.erase(i, m_items.end());
        }
    }

    m_items.push_back(std::pair<std::string, std::string>(type, name));
    if (m_items.size() == 1)
        m_items_it = m_items.begin();
    else
        ++m_items_it;
    Refresh();
}

void EncyclopediaDetailPanel::PopItem() {
    if (!m_items.empty()) {
        m_items.pop_back();
        if (m_items_it == m_items.end() && m_items_it != m_items.begin())
            m_items_it--;
        Refresh();
    }
}

void EncyclopediaDetailPanel::ClearItems() {
    m_items.clear();
    m_items_it = m_items.end();
}

void EncyclopediaDetailPanel::SetText(const std::string& text, bool lookup_in_stringtable) {
    if (m_items_it != m_items.end() && text == m_items_it->second)
        return;
    if (text == "ENC_INDEX")
        SetIndex();
    else
        AddItem(TextLinker::ENCYCLOPEDIA_TAG, (text.empty() || !lookup_in_stringtable) ? text : UserString(text));
}

void EncyclopediaDetailPanel::SetTech(const std::string& tech_name) {
    if (m_items_it != m_items.end() && tech_name == m_items_it->second)
        return;
    AddItem("ENC_TECH", tech_name);
}

void EncyclopediaDetailPanel::SetPartType(const std::string& part_name) {
    if (m_items_it != m_items.end() && part_name == m_items_it->second)
        return;
    AddItem("ENC_SHIP_PART", part_name);
}

void EncyclopediaDetailPanel::SetHullType(const std::string& hull_name) {
    if (m_items_it != m_items.end() && hull_name == m_items_it->second)
        return;
    AddItem("ENC_SHIP_HULL", hull_name);
}

void EncyclopediaDetailPanel::SetBuildingType(const std::string& building_name) {
    if (m_items_it != m_items.end() && building_name == m_items_it->second)
        return;
    AddItem("ENC_BUILDING_TYPE", building_name);
}

void EncyclopediaDetailPanel::SetSpecial(const std::string& special_name) {
    if (m_items_it != m_items.end() && special_name == m_items_it->second)
        return;
    AddItem("ENC_SPECIAL", special_name);
}

void EncyclopediaDetailPanel::SetSpecies(const std::string& species_name) {
    if (m_items_it != m_items.end() && species_name == m_items_it->second)
        return;
    AddItem("ENC_SPECIES", species_name);
}

void EncyclopediaDetailPanel::SetObject(int object_id) {
    int id = UniverseObject::INVALID_OBJECT_ID;
    if (m_items_it != m_items.end())
        id = boost::lexical_cast<int>(m_items_it->second);
    if (object_id == id)
        return;
    AddItem(UNIVERSE_OBJECT, boost::lexical_cast<std::string>(object_id));
}

void EncyclopediaDetailPanel::SetObject(const std::string& object_id) {
    if (m_items_it != m_items.end() && object_id == m_items_it->second)
        return;
    AddItem(UNIVERSE_OBJECT, object_id);
}

void EncyclopediaDetailPanel::SetEmpire(int empire_id) {
    int id = ALL_EMPIRES;
    if (m_items_it != m_items.end())
        id = boost::lexical_cast<int>(m_items_it->second);
    if (empire_id == id)
        return;
    AddItem("ENC_EMPIRE", boost::lexical_cast<std::string>(empire_id));
}

void EncyclopediaDetailPanel::SetEmpire(const std::string& empire_id) {
    if (m_items_it != m_items.end() && empire_id == m_items_it->second)
        return;
    AddItem("ENC_EMPIRE", empire_id);
}

void EncyclopediaDetailPanel::SetDesign(int design_id) {
    int id = ShipDesign::INVALID_DESIGN_ID;
    if (m_items_it != m_items.end())
        id = boost::lexical_cast<int>(m_items_it->second);
    if (design_id == id)
        return;
    AddItem("ENC_SHIP_DESIGN", boost::lexical_cast<std::string>(design_id));
}

void EncyclopediaDetailPanel::SetDesign(const std::string& design_id) {
    if (m_items_it != m_items.end() && design_id == m_items_it->second)
        return;
    AddItem("ENC_SHIP_DESIGN", design_id);
}

void EncyclopediaDetailPanel::SetIncompleteDesign(boost::weak_ptr<const ShipDesign> incomplete_design) {
    m_incomplete_design = incomplete_design;

    if (m_items_it == m_items.end() ||
        m_items_it->first != INCOMPLETE_DESIGN) {
        AddItem(INCOMPLETE_DESIGN, EMPTY_STRING);
    } else {
        Refresh();
    }
}

void EncyclopediaDetailPanel::SetIndex() {
    AddItem(TextLinker::ENCYCLOPEDIA_TAG, "ENC_INDEX");
}

void EncyclopediaDetailPanel::SetItem(const Tech* tech) {
    SetTech(tech ? tech->Name() : EMPTY_STRING);
}

void EncyclopediaDetailPanel::SetItem(const PartType* part) {
    SetPartType(part ? part->Name() : EMPTY_STRING);
}

void EncyclopediaDetailPanel::SetItem(const HullType* hull_type) {
    SetHullType(hull_type ? hull_type->Name() : EMPTY_STRING);
}

void EncyclopediaDetailPanel::SetItem(const BuildingType* building_type) {
    SetBuildingType(building_type ? building_type->Name() : EMPTY_STRING);
}

void EncyclopediaDetailPanel::SetItem(const Special* special) {
    SetSpecial(special ? special->Name() : EMPTY_STRING);
}

void EncyclopediaDetailPanel::SetItem(const Species* species) {
    SetSpecies(species ? species->Name() : EMPTY_STRING);
}

void EncyclopediaDetailPanel::SetItem(const UniverseObject* obj) {
    SetObject(obj ? obj->ID() : UniverseObject::INVALID_OBJECT_ID);
}

void EncyclopediaDetailPanel::SetItem(const Empire* empire) {
    SetEmpire(empire ? empire->EmpireID() : ALL_EMPIRES);
}

void EncyclopediaDetailPanel::SetItem(const ShipDesign* design) {
    SetDesign(design ? design->ID() : ShipDesign::INVALID_DESIGN_ID);
}

void EncyclopediaDetailPanel::OnUp() {
    if (!m_items.empty()) {
        if (m_items_it->first == TextLinker::ENCYCLOPEDIA_TAG ||
            m_items_it->first == INCOMPLETE_DESIGN            ||
            m_items_it->first == UNIVERSE_OBJECT)
        {
            AddItem(TextLinker::ENCYCLOPEDIA_TAG, "ENC_INDEX");
        } else {
            AddItem(TextLinker::ENCYCLOPEDIA_TAG, m_items_it->first);
        }
    } else {
        AddItem(TextLinker::ENCYCLOPEDIA_TAG, "ENC_INDEX");
    }
}

void EncyclopediaDetailPanel::OnBack() {
    if (m_items_it != m_items.begin())
        m_items_it--;
    Refresh();
}

void EncyclopediaDetailPanel::OnNext() {
    std::list<std::pair <std::string, std::string> >::iterator end = m_items.end();
    end--;
    if (m_items_it != end && !m_items.empty())
        m_items_it++;
    Refresh();
}
