//ClientUI.h
#ifndef _ClientUI_h_
#define _ClientUI_h_

//include relevant controls

#include "ToolContainer.h"
#include "CUIControls.h"

//FreeOrion defines
class ClientNetwork;
class ClientUniverse;
class ClientEmpire;
class SystemObjects;
class Planet;
class System;
class Fleet;
class Ship;
class Tech;
class SitRepEvent;
class Combat;

//! \brief ClientUI Main Module
//!This is the main class of the ClientUI module.
//!it contains objects of many other classes and controls the
//!display of all information onscreen.

class ClientUI
{
public:

//!Internal States
//!These determine what screen is currently being displayed

    enum        
    {
        STATE_STARTUP     = 0,
        STATE_INTRO       = 1,
        STATE_SETTINGS    = 2,
        STATE_EMPIRESEL   = 3,
        STATE_TURNSTART   = 4,
        STATE_MAP         = 5,
        STATE_SITREP      = 6,
        STATE_PROCESS     = 7,
        STATE_BATTLE      = 8,
        STATE_SAVE        = 9,
        STATE_LOAD        = 10,
        STATE_SHUTDOWN    = 11
    };//enum

public:
    //! \name Construction & Initialization
    //!@{
    
    ClientUI();        //!< construction (calls Initialize())
    ~ClientUI();    //!< destruction (calls Cleanup())

    bool Initialize();    //!< provided to setup initial variables. 
    bool Cleanup();        //!< provided to clean up ClientUI's memory space. 

    //!@}
    
    //! \name Utility Functions
    //!@{
    
    //! @param width screen width
    //! @param height screen height
    bool ChangeResolution(int width, int height);    //!< changes the screen resolution and modifies any images or data required to make the change.  
    bool Freeze();                        //!< freezes the interface so that user input is ignored.
    bool Unfreeze();                    //!< unfreezes the UI and input is recognized again
    
    //!@}

    //! \name GameCore Interface functions
    //! calling these changes internal state to display the proper screen
    //! and initializes and displays the screen
    //! see Interface Doc for details
    
    //!@{
    
    void ScreenIntro();                        //!< Intro Screen
    void ScreenSettings(ClientNetwork &net);    //!< Game/Network Options Screen
    void ScreenEmpireSelect();                    //!< Empire Selection Screen
    void ScreenTurnStart();                    //!< Turn Start Splash Screen
    
    //! @param u address of ClientUniverse module
    //! @param e address of ClientEmpire module
    void ScreenMap(const ClientUniverse &u, const ClientEmpire &e);     //!< Universe Map Screen

    //! @param events vector containing all the events to be listed
    void ScreenSitrep(const std::vector<SitRepEvent> events);    //!< Sitrep Screen
    
    //! @param state integer code pertaining to the message to display on the turn-processing screen
    void ScreenProcessTurn(int state);                //!< Turn-processing screen
    
    //! @param combat pointer to a Combat module
    void ScreenBattle(Combat* combat);                //!< Battle Screen
    
    //! @param show true if the screen is to be displayed, false if it is to be turned off
    void ScreenSave(bool show);                    //!< Savegame Screen
    
    //! @param show true if the screen is to be displayed, false if it is to be turned off
    void ScreenLoad(bool show);                    //!< Load Game Screen

    //!@}
    
    //! \name Zooming Functions
    //!@{
    
    //! @param p address of a planet that we wish to zoom to
    //! @return true if successful, false if object doesn't exist
    bool ZoomTo(const Planet& p);    //!< Zooms to a particular planet on the galaxy map and opens the planet screen for that planet

    //! @param s address of system that we wish to zoom to
    //! @return true if successful, false if object doesn't exist
    bool ZoomTo(const System& s);    //!< Zooms to a particular system on the galaxy map
    
    //! @param s address of fleet that we wish to zoom to
    //! @return true if successful, false if object doesn't exist
    bool ZoomTo(const Fleet& f);    //!< Zooms to a particular fleet on the galaxy map and opens the fleet window
    
    //! @param s address of ship that we wish to zoom to
    //! @return true if successful, false if object doesn't exist
    bool ZoomTo(const Ship& s);    //!< Zooms to a particular ship on the galaxy map and opens its fleet and/or ship window
    
    //! @param t address of technology that we wish to zoom to
    //! @return true if successful, false if object doesn't exist
    bool ZoomTo(const Tech& t);    //!< Opens the technology screen and presents a description of the given technology
   
   //!@}
   
#ifdef DEBUG

    // \name Debugging Functions
    //!@{
    
    //! @param message The message to display
    void MessageBox(const std::string message);        //!< displays a the message in a popup window for debugging
    
    //!@}
#endif

private:
    static const Uint32 TOOLTIP_DELAY;    //!<number of milliseconds to initialize tooltips to
    static ToolContainer* m_tooltips;        //!< the single toolcontainer object
    static int m_state;                    //!< represents the screen currently being displayed

};//ClientUI


#endif
