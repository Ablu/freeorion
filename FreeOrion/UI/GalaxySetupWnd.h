// -*- C++ -*-
#ifndef _GalaxySetupWnd_h_
#define _GalaxySetupWnd_h_

#include "../universe/Universe.h"
#include "CUIWnd.h"
#include "CUISpin.h"

namespace GG {
    class RadioButtonGroup;
    class StaticGraphic;
    class TextControl;
    class Texture;
}
class CUIButton;
class CUIStateButton;
class EmpireColorSelector;
struct GalaxySetupData;

/** Encapsulates the galaxy setup options so that they may be reused in the GalaxySetupWnd and the MultiPlayerLobbyWnd. */
class GalaxySetupPanel : public GG::Control {
public:
    static const GG::X DEFAULT_WIDTH;

    /** \name Signal Types */ //@{
    typedef boost::signal<void ()>                               SettingsChangedSignalType; ///< emitted when the any of the settings controls changes
    typedef boost::signal<void (boost::shared_ptr<GG::Texture>)> ImageChangedSignalType;    ///< emitted when the galaxy preview image changes
    //@}

    /** \name Slot Types */ //@{
    typedef SettingsChangedSignalType::slot_type SettingsChangedSlotType; ///< type of functor(s) invoked on a SettingsChangedSignalType
    typedef ImageChangedSignalType::slot_type    ImageChangedSlotType;    ///< type of functor(s) invoked on a ImageChangedSignalType
    //@}

    /** \name Structors*/ //!@{
    GalaxySetupPanel(GG::X x, GG::Y y, GG::X w = DEFAULT_WIDTH);
    //!@}

    /** \name Accessors*/ //!@{
    unsigned int                    GetSeed() const;                //!< Returns the seed used in creating the galaxy
    int                             Systems() const;                //!< Returns the number of star systems to use in generating the galaxy
    Shape                           GetShape() const;               //!< Returns the shape of the galaxy
    GalaxySetupOption               GetAge() const;                 //!< Returns the age of the galaxy
    GalaxySetupOption               GetStarlaneFrequency() const;   //!< Returns the frequency of starlanes in the galaxy
    GalaxySetupOption               GetPlanetDensity() const;       //!< Returns the density of planets within systems
    GalaxySetupOption               GetSpecialsFrequency() const;   //!< Returns the rarity of planetary and system specials
    GalaxySetupOption               GetMonsterFrequency() const;    //!< Returns the frequency of space monsters
    GalaxySetupOption               GetNativeFrequency() const;     //!< Returns the frequency of natives
    Aggression                      GetAIAggression() const;        //!< Returns the  maximum AI aggression level 
    
    boost::shared_ptr<GG::Texture>  PreviewImage() const;           //!< Returns the current preview image texture

    mutable SettingsChangedSignalType SettingsChangedSignal;        ///< the settings changed signal object for this GalaxySetupPanel
    mutable ImageChangedSignalType    ImageChangedSignal;           ///< the image changed signal object for this GalaxySetupPanel
    //!@}

    /** \name Mutators*/ //!@{
    virtual void Render() {}
    virtual void Disable(bool b = true);

    void SetFromSetupData(const GalaxySetupData& setup_data); ///< sets the controls from a GalaxySetupData
    void GetSetupData(GalaxySetupData& setup_data) const;     ///< fills values in \a setup_data from the panel's current state
    //!@}

private:
    void Init();
    void SettingChanged_(int);
    void SettingChanged(GG::ListBox::iterator);
    void ShapeChanged(GG::ListBox::iterator it);

    CUIEdit*            m_seed_edit;            //!< The seed used in the generation of the galaxy
    CUISpin<int>*       m_stars_spin;           //!< The number of stars to include in the galaxy
    CUIDropDownList*    m_galaxy_shapes_list;   //!< The possible shapes for the galaxy
    CUIDropDownList*    m_galaxy_ages_list;     //!< The possible ages for the galaxy
    CUIDropDownList*    m_starlane_freq_list;   //!< The frequency of starlanes in the galaxy
    CUIDropDownList*    m_planet_density_list;  //!< The density of planets in each system
    CUIDropDownList*    m_specials_freq_list;   //!< The frequency of specials in systems and on planets
    CUIDropDownList*    m_monster_freq_list;    //!< The frequency of monsters
    CUIDropDownList*    m_native_freq_list;     //!< The frequency of natives
    CUIDropDownList*    m_ai_aggression_list;   //!< The max aggression choices for AI opponents
    
    std::vector<boost::shared_ptr<GG::Texture> > m_textures; //!< textures for galaxy previews
};

//! This class is the Galaxy Setup window.  It is a modal window
//! that allows the user to choose a galaxy style, size, etc.
class GalaxySetupWnd : public CUIWnd {
public:
    /** \name Structors*/ //!@{
    GalaxySetupWnd();   //!< default ctor
    //!@}

    /** \name Accessors*/ //!@{
    /** returns true iff the dialog is finished running and it was closed with the "OK" button */
    bool                    EndedWithOk() const {return m_done && m_ended_with_ok;}

    /** returns the panel containing all the user-chosen options. */
    const GalaxySetupPanel& Panel()      const  {return *m_galaxy_setup_panel;}
    const std::string&      EmpireName() const;
    GG::Clr                 EmpireColor() const;
    const std::string&      StartingSpeciesName() const;
    int                     NumberAIs() const;
    //!@}

    /** \name Mutators*/ //!@{
    virtual void Render();    //!< drawing code
    virtual void KeyPress (GG::Key key, boost::uint32_t key_code_point, GG::Flags<GG::ModKey> mod_keys);
    //!@}

private:
    void Init();
    void PreviewImageChanged(boost::shared_ptr<GG::Texture> new_image);
    void EmpireNameChanged(const std::string& name);
    void PlayerNameChanged(const std::string& name);
    void OkClicked();
    void CancelClicked();

    bool m_ended_with_ok;    //!< indicates whether or not we ended the dialog with OK or not

    GalaxySetupPanel*       m_galaxy_setup_panel;    //!< The GalaxySetupPanel that does most of the work of the dialog
    GG::TextControl*        m_player_name_label;
    CUIEdit*                m_player_name_edit;
    GG::TextControl*        m_empire_name_label;
    CUIEdit*                m_empire_name_edit;
    GG::TextControl*        m_empire_color_label;
    EmpireColorSelector*    m_empire_color_selector;
    SpeciesSelector*        m_starting_secies_selector;
    GG::TextControl*        m_starting_species_label;
    GG::TextControl*        m_number_ais_label;
    CUISpin<int>*           m_number_ais_spin;
    GG::StaticGraphic*      m_preview_image;         //!< The galaxy shape preview image
    CUIButton*              m_ok;                    //!< OK button
    CUIButton*              m_cancel;                //!< Cancel button

    GG::Pt                  m_preview_ul;
};

#endif // _GalaxySetupWnd_h_
