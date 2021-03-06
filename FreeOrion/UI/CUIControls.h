// -*- C++ -*-
//CUIControls.h
#ifndef _CUIControls_h_
#define _CUIControls_h_

#include "ClientUI.h"
#include "CUIDrawUtil.h"
#include <GG/Button.h>
#include <GG/DropDownList.h>
#include <GG/Edit.h>
#include <GG/Menu.h>
#include <GG/MultiEdit.h>
#include <GG/Scroll.h>
#include <GG/Slider.h>
#include <GG/StaticGraphic.h>
#include <GG/TabWnd.h>
#include <GG/dialogs/FileDlg.h>

#include <boost/function.hpp>

#include "LinkText.h"


//! \file All CUI* classes are FreeOrion-style controls incorporating 
//! the visual theme the project requires.  Implementation may
//! depend on graphics and design team specifications.  They extend
//! GG controls.

/** a FreeOrion Button control */
class CUIButton : public GG::Button {
public:
    /** \name Structors */ //@{
    CUIButton(GG::X x, GG::Y y, GG::X w, const std::string& str, const boost::shared_ptr<GG::Font>& font = boost::shared_ptr<GG::Font>(), 
              GG::Clr color = ClientUI::CtrlColor(), GG::Clr border = ClientUI::CtrlBorderColor(), int thick = 1, 
              GG::Clr text_color = ClientUI::TextColor(), GG::Flags<GG::WndFlag> flags = GG::INTERACTIVE); ///< basic ctor
    //@}

    /** \name Accessors */ //@{
    GG::Clr      BorderColor() const {return m_border_color;} ///< returns the color used to render the border of the button
    int          BorderThickness() const {return m_border_thick;} ///< returns the width used to render the border of the button

    virtual bool InWindow(const GG::Pt& pt) const;
    //@}

    /** \name Mutators */ //@{
    virtual void MouseHere(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);

    void SetBorderColor(GG::Clr clr);   ///< sets the color used to render the border of the button
    void SetBorderThick(int thick);     ///< sets the thickness of the rendered the border of the button

    void MarkNotSelected();             ///< sets button colours to standard UI colours
    void MarkSelectedGray();            ///< sets button colours to lighter grey background and border to indicate selection or activation
    void MarkSelectedTechCategoryColor(std::string category);   ///< sets button background and border colours to variants of the colour of the tech category specified
    //@}

protected:
    /** \name Mutators control */ //@{
    virtual void RenderPressed();
    virtual void RenderRollover();
    virtual void RenderUnpressed();
    //@}

private:
    GG::Clr m_border_color;
    int     m_border_thick;
};

class SettableInWindowCUIButton : public CUIButton {
public:
    /** \name Structors */ //@{
    SettableInWindowCUIButton(GG::X x, GG::Y y, GG::X w, const std::string& str,
                              const boost::shared_ptr<GG::Font>& font = boost::shared_ptr<GG::Font>(),
                              GG::Clr color = ClientUI::CtrlColor(), GG::Clr border = ClientUI::CtrlBorderColor(),
                              int thick = 1,  GG::Clr text_color = ClientUI::TextColor(),
                              GG::Flags<GG::WndFlag> flags = GG::INTERACTIVE); ///< basic ctor
    //@}

    /** \name Accessors */ //@{
    virtual bool    InWindow(const GG::Pt& pt) const;
    //@}

    /** \name Mutators */ //@{
    void            SetInWindow(boost::function<bool(const GG::Pt&)> in_window_function);
    //@}

private:
    boost::function<bool(const GG::Pt&)>    m_in_window_func;
};

/** a FreeOrion next-turn button control */
class CUITurnButton : public CUIButton {
public:
    /** \name Structors */ //@{
    CUITurnButton(GG::X x, GG::Y y, GG::X w, const std::string& str,
                  const boost::shared_ptr<GG::Font>& font = boost::shared_ptr<GG::Font>(), 
                  GG::Clr color = ClientUI::CtrlColor(), GG::Clr border = ClientUI::CtrlBorderColor(),
                  int thick = 1,  GG::Clr text_color = ClientUI::TextColor(),
                  GG::Flags<GG::WndFlag> flags = GG::INTERACTIVE); ///< basic ctor
    //@}
};

/** a FreeOrion triangular arrow button */
class CUIArrowButton : public GG::Button {
public:
    /** \name Structors */ //@{
    CUIArrowButton(GG::X x, GG::Y y, GG::X w, GG::Y h, ShapeOrientation orientation,
                   GG::Clr color, GG::Flags<GG::WndFlag> flags = GG::INTERACTIVE); ///< basic ctor
    //@}

    /** \name Accessors */ //@{
    virtual bool    InWindow(const GG::Pt& pt) const;
    bool            FillBackgroundWithWndColor() const;
    //@}

    /** \name Mutators */ //@{
    virtual void    MouseHere(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
    void            FillBackgroundWithWndColor(bool fill);
    //@}

protected:
    /** \name Mutators control */ //@{
    virtual void RenderPressed();
    virtual void RenderRollover();
    virtual void RenderUnpressed();
    //@}

private:
    ShapeOrientation m_orientation;
    bool             m_fill_background_with_wnd_color;
};

/** a FreeOrion StateButton control */
class CUIStateButton : public GG::StateButton {
public:
    /** \name Structors */ //@{
    CUIStateButton(GG::X x, GG::Y y, GG::X w, GG::Y h, const std::string& str, GG::Flags<GG::TextFormat> format, GG::StateButtonStyle style = GG::SBSTYLE_3D_CHECKBOX,
                   GG::Clr color = ClientUI::StateButtonColor(), const boost::shared_ptr<GG::Font>& font = boost::shared_ptr<GG::Font>(),
                   GG::Clr text_color = ClientUI::TextColor(), GG::Clr interior = GG::CLR_ZERO,
                   GG::Clr border = ClientUI::CtrlBorderColor(), GG::Flags<GG::WndFlag> flags = GG::INTERACTIVE); ///< ctor
    //@}

    /** \name Accessors */ //@{
    virtual GG::Pt MinUsableSize() const;

    GG::Clr        BorderColor() const {return m_border_color;} ///< returns the color used to render the border of the button
    //@}

    /** \name Mutators */ //@{
    virtual void   Render();
    virtual void   MouseEnter(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
    virtual void   MouseLeave();
    //@}

private:
    GG::Clr m_border_color;
    bool    m_mouse_here;
};

/** Tab bar with buttons for selecting tabbed windows. */
class CUITabBar : public GG::TabBar {
public:
    /** \name Structors */ ///@{
    /** Basic ctor. */
    CUITabBar(GG::X x, GG::Y y, GG::X w, const boost::shared_ptr<GG::Font>& font, GG::Clr color,
              GG::Clr text_color, GG::TabBarStyle style, GG::Flags<GG::WndFlag> flags);
    //@}

private:
    virtual void DistinguishCurrentTab(const std::vector<GG::StateButton*>& tab_buttons);
};

/** a FreeOrion Scroll control */
class CUIScroll : public GG::Scroll {
public:
    /** represents the tab button for a CUIScroll */
    class ScrollTab : public GG::Button {
    public:
        ScrollTab(GG::Orientation orientation, int scroll_width, GG::Clr color, GG::Clr border_color); ///< basic ctor
        virtual void SetColor(GG::Clr c);
        virtual void Render();
        virtual void LButtonDown(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
        virtual void LButtonUp(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
        virtual void LClick(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
        virtual void MouseEnter(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
        virtual void MouseLeave();
    private:
        GG::Clr m_border_color;
        GG::Orientation m_orientation;
        bool m_mouse_here;
        bool m_being_dragged;
    };

    /** \name Structors */ //@{
    CUIScroll(GG::X x, GG::Y y, GG::X w, GG::Y h, GG::Orientation orientation,
              GG::Clr border_color = ClientUI::CtrlBorderColor(), GG::Clr interior_color = ClientUI::CtrlColor(),
              GG::Flags<GG::WndFlag> flags = GG::INTERACTIVE | GG::REPEAT_BUTTON_DOWN); ///< basic ctor
    //@}

    /** \name Mutators */ //@{
    virtual void Render();
    virtual void SizeMove(const GG::Pt& ul, const GG::Pt& lr);
    //@}

protected:
    GG::Clr m_border_color;
};

/** a FreeOrion ListBox control */
class CUIListBox : public GG::ListBox {
public:
    /** \name Structors */ //@{
    CUIListBox(GG::X x, GG::Y y, GG::X w, GG::Y h, GG::Clr border_color = ClientUI::CtrlBorderColor(), GG::Clr interior_color = ClientUI::CtrlColor(),
               GG::Flags<GG::WndFlag> flags = GG::INTERACTIVE); ///< basic ctor
    //@}

    /** \name Mutators */ //@{
    virtual void Render();
    //@}
};

/** a ListBox with user-sortable columns */
class CUISortListBox : public GG::ListBox {
public:
    /** \name Structors */ //@{
    CUISortListBox(GG::X x, GG::Y y, GG::X w, GG::Y h, GG::Clr color = ClientUI::CtrlBorderColor(), GG::Clr interior = GG::CLR_ZERO,
                   GG::Flags<GG::WndFlag> flags = GG::INTERACTIVE);
    //@}

    /** \name Mutators */ //@{
    virtual void Render();

    void SetSortCol(int n);                 ///< sets column used to sort rows, updates sort-button appearance
    void SetSortDirecion(bool ascending);   ///< sets whether to sort ascending (true) or descending (false), updates sort-button appearance
    void SetColHeaders(std::map<int, boost::shared_ptr<GG::Texture> > textures_map);    ///< indexed by column number, provides texture with which to label column
    void SetColHeaders(std::map<int, std::string> label_text_map);    ///< indexed by column number, provides text with which to label column
    //@}

private:
    Row* m_header_row;  ///< need to keep own pointer to header row, since ListBox doesn't provide non-const access to its header row pointer
};

/** a FreeOrion DropDownList control */
class CUIDropDownList : public GG::DropDownList {
public:
    /** \name Structors */ //@{
    CUIDropDownList(GG::X x, GG::Y y, GG::X w, GG::Y h, GG::Y drop_ht, GG::Clr border_color = ClientUI::CtrlBorderColor(),
                    GG::Clr interior = ClientUI::CtrlColor(), GG::Flags<GG::WndFlag> flags = GG::INTERACTIVE); ///< basic ctor
    //@}

    /** \name Mutators */ //@{
    virtual void Render();
    virtual void LClick(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
    virtual void MouseEnter(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
    virtual void MouseLeave();

    void         DisableDropArrow();  ///< disables rendering of the small downward-facing arrow on the right of the control
    void         EnableDropArrow();   ///< enables rendering of the small downward-facing arrow on the right of the control
    //@}

private:
    bool m_render_drop_arrow;
    bool m_mouse_here;
};

/** a FreeOrion Edit control */
class CUIEdit : public GG::Edit {
public:
    /** \name Structors */ //@{
    CUIEdit(GG::X x, GG::Y y, GG::X w, const std::string& str, const boost::shared_ptr<GG::Font>& font = boost::shared_ptr<GG::Font>(),
            GG::Clr border_color = ClientUI::CtrlBorderColor(), GG::Clr text_color = ClientUI::TextColor(),
            GG::Clr interior = ClientUI::CtrlColor(), GG::Flags<GG::WndFlag> flags = GG::INTERACTIVE); ///< basic ctor
    //@}

    /** \name Mutators */ //@{
    virtual void Render();
    //@}
};

/** a FreeOrion MultiEdit control */
class CUIMultiEdit : public GG::MultiEdit {
public:
    /** \name Structors */ //@{
    CUIMultiEdit(GG::X x, GG::Y y, GG::X w, GG::Y h, const std::string& str, GG::Flags<GG::MultiEditStyle> style = GG::MULTI_LINEWRAP,
                 const boost::shared_ptr<GG::Font>& font = boost::shared_ptr<GG::Font>(),
                 GG::Clr border_color = ClientUI::CtrlBorderColor(), GG::Clr text_color = ClientUI::TextColor(),
                 GG::Clr interior = ClientUI::CtrlColor(), GG::Flags<GG::WndFlag> flags = GG::INTERACTIVE); ///< basic ctor
    //@}

    /** \name Mutators */ //@{
    virtual void Render();
    //@}
};

/** a FreeOrion MultiEdit control that parses its text and makes links within clickable */
class CUILinkTextMultiEdit : public CUIMultiEdit, public TextLinker {
public:
    /** \name Structors */ //@{
    CUILinkTextMultiEdit(GG::X x, GG::Y y, GG::X w, GG::Y h, const std::string& str, GG::Flags<GG::MultiEditStyle> style = GG::MULTI_LINEWRAP,
                         const boost::shared_ptr<GG::Font>& font = boost::shared_ptr<GG::Font>(),
                         GG::Clr border_color = ClientUI::CtrlBorderColor(), GG::Clr text_color = ClientUI::TextColor(),
                         GG::Clr interior = ClientUI::CtrlColor(), GG::Flags<GG::WndFlag> flags = GG::INTERACTIVE); ///< basic ctor
    //@}
    /** \name Accessors */ //@{
    virtual const std::vector<GG::Font::LineData>&  GetLineData() const;
    virtual const boost::shared_ptr<GG::Font>&      GetFont() const;
    virtual GG::Pt                                  TextUpperLeft() const;
    virtual GG::Pt                                  TextLowerRight() const;
    virtual const std::string&                      RawText() const;
    //@}

    /** \name Mutators */ //@{
    virtual void    Render();
    virtual void    LClick(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
    virtual void    LDoubleClick(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
    virtual void    RClick(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
    virtual void    MouseHere(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
    virtual void    MouseLeave();
    virtual void    SizeMove(const GG::Pt& ul, const GG::Pt& lr);

    /** sets the text to \a str; may resize the window.  If the window was
        constructed to fit the size of the text (i.e. if the second ctor type
        was used), calls to this function cause the window to be resized to
        whatever space the newly rendered text occupies. */
    virtual void    SetText(const std::string& str);
    //@}

private:
    virtual void    SetLinkedText(const std::string& str);

    bool            m_already_setting_text_so_dont_link;
    std::string     m_raw_text;
};

/** A simple GG::ListBox::Row subclass designed for use in text-only drop-down
  * lists, such as the ones used in the game setup dialogs. */
struct CUISimpleDropDownListRow : public GG::ListBox::Row {
    CUISimpleDropDownListRow(const std::string& row_text, GG::Y row_height = DEFAULT_ROW_HEIGHT);
    static const GG::Y DEFAULT_ROW_HEIGHT;
};

/** Encapsulates an icon and text that goes with it in a single control.  For
  * example, "[trade icon] +1" or "[population icon] 66 (+5)", where [... icon]
  * is an icon image, not text.
  * The icon may have one or two numerical values.  If one, just that number is
  * displayed.  If two, the first number is displayed followed by the second in
  * brackets "()" */
class StatisticIcon : public GG::Control {
public:
    /** \name Structors */ //@{
    StatisticIcon(GG::X x, GG::Y y, GG::X w, GG::Y h, const boost::shared_ptr<GG::Texture> texture,
                  double value, int digits, bool showsign,
                  GG::Flags<GG::WndFlag> flags = GG::INTERACTIVE); ///< initializes with one value

    StatisticIcon(GG::X x, GG::Y y, GG::X w, GG::Y h, const boost::shared_ptr<GG::Texture> texture,
                  double value0, double value1, int digits0, int digits1,
                  bool showsign0, bool showsign1,
                  GG::Flags<GG::WndFlag> flags = GG::INTERACTIVE);  ///< initializes with two values
    //@}

    /** \name Accessors */ //@{
    double          GetValue(int index = 0) const;
    //@}

    /** \name Mutators */ //@{
    virtual void    Render() {}

    virtual void    LButtonDown(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
    virtual void    RButtonDown(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
    virtual void    MouseWheel(const GG::Pt& pt, int move, GG::Flags<GG::ModKey> mod_keys);

    void            SetValue(double value, int index = 0);  ///< sets displayed \a value with \a index
    //@}

private:
    void            Refresh();
    GG::Clr         ValueColor(int index) const;        ///< returns colour in which to draw value

    int                 m_num_values;

    std::vector<double> m_values;
    std::vector<int>    m_digits;
    std::vector<bool>   m_show_signs;

    GG::StaticGraphic*  m_icon;
    GG::TextControl*    m_text;
};

class CUIToolBar : public GG::Control {
public:
    /** \name Structors */ //@{
    CUIToolBar(GG::X x, GG::Y y, GG::X w, GG::Y h);
    //@}

    /** \name Accessors */ //@{
    virtual bool    InWindow(const GG::Pt& pt) const;
    //@}

    /** \name Mutators */ //@{
    void            Render();
    //@}
private:
};

/** A control used to pick from at list of species names. */
class SpeciesSelector : public CUIDropDownList {
public:
    /** \name Structors */ //@{
    SpeciesSelector(GG::X w, GG::Y h);                                                  ///< populates with all species in SpeciesManager
    SpeciesSelector(GG::X w, GG::Y h, const std::vector<std::string>& species_names);   ///< populates with the species in \a species_names
    //@}

    /** \name Accessors */ //@{
    const std::string&          CurrentSpeciesName() const;     ///< returns the name of the species that is currently selected
    std::vector<std::string>    AvailableSpeciesNames() const;  ///< returns the names of species in the selector
    //@}

    /** \name Mutators */ //@{
    void SelectSpecies(const std::string& species_name);
    void SetSpecies(const std::vector<std::string>& species_names);         ///< sets the species that can be selected
    //@}

    mutable boost::signal<void (const std::string&)> SpeciesChangedSignal;

private:
    void SelectionChanged(GG::DropDownList::iterator it);
};

/** A control used to pick from the empire colors returned by EmpireColors(). */
class EmpireColorSelector : public CUIDropDownList {
public:
    /** \name Structors */ //@{
    explicit EmpireColorSelector(GG::Y h);
    //@}

    /** \name Accessors */ //@{
    GG::Clr CurrentColor() const; ///< returns the color that is currently selected, or GG::CLR_ZERO if none is selected
    //@}

    /** \name Mutators */ //@{
    void SelectColor(const GG::Clr& clr);
    //@}

    mutable boost::signal<void (const GG::Clr&)> ColorChangedSignal;

private:
    void SelectionChanged(GG::DropDownList::iterator it);
};

/** A control used to pick arbitrary colors using GG::ColorDlg. */
class ColorSelector : public GG::Control {
public:
    /** \name Structors */ //@{
    ColorSelector(GG::X x, GG::Y y, GG::X w, GG::Y h, GG::Clr color, GG::Clr default_color);
    //@}

    /** \name Mutators */ //@{
    virtual void Render();
    virtual void LClick(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
    virtual void RClick(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
    //@}

    mutable boost::signal<void (const GG::Clr&)> ColorChangedSignal;

private:
    GG::Clr m_default_color;
};

/** A GG file dialog in the FreeOrion style. */
class FileDlg : public GG::FileDlg {
public:
    /** \name Structors */ //@{
    FileDlg(const std::string& directory, const std::string& filename, bool save, bool multi,
            const std::vector<std::pair<std::string, std::string> >& types);
    //@}
};

/** Despite the name, this is actually used to display info in both the Research and Production screens. */
class ProductionInfoPanel : public GG::Wnd {
public:
    /** \name Structors */ //@{
    ProductionInfoPanel(GG::X w, GG::Y h, const std::string& title, const std::string& points_str,
                        float border_thickness, const GG::Clr& color, const GG::Clr& text_and_border_color);
    //@}

    /** \name Mutators */ //@{
    virtual void Render();

    void Reset(double total_points, double total_queue_cost, int projects_in_progress, double points_to_underfunded_projects, int queue_size);
    //@}

private:
    void Draw(GG::Clr clr, bool fill);

    GG::TextControl* m_title;
    GG::TextControl* m_total_points_label;
    GG::TextControl* m_total_points;
    GG::TextControl* m_total_points_P_label;
    GG::TextControl* m_wasted_points_label;
    GG::TextControl* m_wasted_points;
    GG::TextControl* m_wasted_points_P_label;
    GG::TextControl* m_projects_in_progress_label;
    GG::TextControl* m_projects_in_progress;
    GG::TextControl* m_points_to_underfunded_projects_label;
    GG::TextControl* m_points_to_underfunded_projects;
    GG::TextControl* m_points_to_underfunded_projects_P_label;
    GG::TextControl* m_projects_in_queue_label;
    GG::TextControl* m_projects_in_queue;

    std::pair<int, int> m_center_gap;
    float m_border_thickness;
    GG::Clr m_color;
    GG::Clr m_text_and_border_color;

    static const int CORNER_RADIUS;
    static const GG::Y VERTICAL_SECTION_GAP;
};

/** Displays progress that is divided over mulitple turns, as in the Research and Production screens. */
class MultiTurnProgressBar : public GG::Control {
public:
    /** \name Structors */ //@{
    /** ctor */
    MultiTurnProgressBar(GG::X w, GG::Y h, int total_turns, double turns_completed,
                         const GG::Clr& bar_color, const GG::Clr& background, const GG::Clr& outline_color);
    //@}

    /** \name Mutators */ //@{
    virtual void Render();
    //@}

private:
    int     m_total_turns;
    double  m_turns_completed;
    GG::Clr m_bar_color;
    GG::Clr m_background;
    GG::Clr m_outline_color;
};

/** Displays current rendering frames per second. */
class FPSIndicator : public GG::TextControl {
public:
    FPSIndicator(GG::X x, GG::Y y);
    virtual void Render();
private:
    void UpdateEnabled();
    bool m_enabled;
};

/** Acts like a normal TextControl, but renders extra black copy / copies of text behind to create a
  * drop-shadow effect, impriving text readability. */
class ShadowedTextControl : public GG::TextControl {
public:
    ShadowedTextControl(GG::X x, GG::Y y, GG::X w, GG::Y h, const std::string& str, const boost::shared_ptr<GG::Font>& font,
                        GG::Clr color = GG::CLR_BLACK, GG::Flags<GG::TextFormat> format = GG::FORMAT_NONE,
                        GG::Flags<GG::WndFlag> flags = GG::Flags<GG::WndFlag>());

    ShadowedTextControl(GG::X x, GG::Y y, const std::string& str, const boost::shared_ptr<GG::Font>& font,
                        GG::Clr color = GG::CLR_BLACK, GG::Flags<GG::TextFormat> format = GG::FORMAT_NONE,
                        GG::Flags<GG::WndFlag> flags = GG::Flags<GG::WndFlag>());
 
    virtual void Render();
};

/** Functions like a StaticGraphic, except can have multiple textures that are rendered bottom to top
  * in the order, rather than a single texture. */
class MultiTextureStaticGraphic : public GG::Control {
public:
    /** \name Structors */ ///@{

    /** creates a MultiTextureStaticGraphic from multiple pre-existing Textures which are rendered back-to-front in the
      * order they are specified in \a textures with GraphicStyles specified in the same-indexed value of \a styles.
      * if \a styles is not specified or contains fewer entres than \a textures, entries in \a textures without 
      * associated styles use the style GRAPHIC_CENTER. */
    MultiTextureStaticGraphic(GG::X x, GG::Y y, GG::X w, GG::Y h, const std::vector<boost::shared_ptr<GG::Texture> >& textures,
                              const std::vector<GG::Flags<GG::GraphicStyle> >& styles = std::vector<GG::Flags<GG::GraphicStyle> >(),
                              GG::Flags<GG::WndFlag> flags = GG::Flags<GG::WndFlag>());

    /** creates a MultiTextureStaticGraphic from multiple pre-existing SubTextures which are rendered back-to-front in the
      * order they are specified in \a subtextures with GraphicStyles specified in the same-indexed value of \a styles.
      * if \a styles is not specified or contains fewer entres than \a subtextures, entries in \a subtextures without 
      * associated styles use the style GRAPHIC_CENTER. */
    MultiTextureStaticGraphic(GG::X x, GG::Y y, GG::X w, GG::Y h, const std::vector<GG::SubTexture>& subtextures,
                              const std::vector<GG::Flags<GG::GraphicStyle> >& styles = std::vector<GG::Flags<GG::GraphicStyle> >(),
                              GG::Flags<GG::WndFlag> flags = GG::Flags<GG::WndFlag>());
    //@}

    /** \name Mutators */ ///@{
    virtual void    Render();       ///< renders textures in order specified in constructor, back-to-front
    //@}

protected:
    MultiTextureStaticGraphic();    ///< default ctor

    /** Returns the area in which the graphic is actually rendered, in
        UpperLeft()-relative coordinates.  This may not be the entire area of
        the StaticGraphic, based on the style being used. */
    GG::Rect        RenderedArea(const GG::SubTexture& subtexture, GG::Flags<GG::GraphicStyle> style) const;

private:
    void            Init();
    void            ValidateStyles();      ///< ensures that the style flags are consistent

    std::vector<GG::SubTexture>                 m_graphics;
    std::vector<GG::Flags<GG::GraphicStyle> >   m_styles;   ///< position of texture wrt the window area
};

#endif // _CUIControls_h_
