
#ifndef _ClientEmpireManager_h_
#include "ClientEmpireManager.h"
#endif

#ifndef _XMLObjectFactory_h_
#include "../GG/XML/XMLObjectFactory.h"
#endif

#ifndef _SitRepEntry_h_
#include "../util/SitRepEntry.h"
#endif

#ifndef __XDIFF__
#include "../network/XDiff.hpp"
#endif

#include <string>


ClientEmpireManager::ClientEmpireManager() : EmpireManager(), m_last_turn_empire_state(EmpireManager::EMPIRE_UPDATE_TAG)
{
    // no worries yet
}
   
/**
*  Handles an update for this client.  
*  The XMLElement should be an XMLDiff for the empires in this client
*  manager (that is, produced by ServerEmpireManager::CreateClientEmpireUpdate()
*
*/
bool ClientEmpireManager::HandleEmpireElementUpdate( GG::XMLElement empireElement)
{    
    // make an XMLDoc for the new empire state, initialize it to old state
    GG::XMLDoc new_state;
    new_state.root_node = m_last_turn_empire_state;
    
    // make an XMLDoc for the update 
    GG::XMLDoc update_patch;
    update_patch.root_node = empireElement;
    
    // apply the patch
    XPatch(new_state, update_patch);
    
    // if its not an empire update element, we have an error
    if(new_state.root_node.Tag() != EmpireManager::EMPIRE_UPDATE_TAG)
    {   
        return false;
    }
    
    // **********************
    // now we must decode
    // **********************
   
    for(int i=0; i<new_state.root_node.NumChildren(); i++)
    {
        Empire decoded_empire( new_state.root_node.Child(i).Child(0) );
        Empire* old_empire = Lookup(decoded_empire.EmpireID());
        
        if(old_empire)
        {
            *old_empire = decoded_empire;
        }
        else
        {
            old_empire = new Empire( decoded_empire );
            InsertEmpire(old_empire);
        }
    }
     
    return true;
}

/**
*  Takes an XMLElement representing a list of sitrep events
*  The list will be decoded, one entry at a time, and the entries
*  will be added to the sitrep of the empire to whom they belong.
* 
*  The XMLElement passed into this method should be identical to
*  that generated by ServerEmpireManager::CreateClientSitrepUpdate()
*
*/
/*bool ClientEmpireManager::HandleSitRepElementUpdate( GG::XMLElement sitRepElement)
{
    // if it's not a sitrep element, we have an error
    if(sitRepElement.Tag() != EmpireManager::SITREP_UPDATE_TAG)
    {
        return false;
    }
    
    // if it lacks the EmpireID attribute, we have an error
    std::string sID = (sitRepElement.Attribute("EmpireID"));
    if( sID == "" )
    {
        return false;
    }
    
    // figure out what empire this sitrep update is for
    int iID = atoi(sID.c_str());
    Empire* emp = Lookup(iID);
    
    // if we dont have that empire in our manager, we have an error
    if(emp == NULL)
    {
        return false;
    }
    
    // clear the empire's current sitrep. 
    // this causes all current sitrep entries to be deleted
    emp->ClearSitRep();
    
    // make object factory for sitrep entries
    GG::XMLObjectFactory<SitRepEntry> sitrep_factory;
    SitRepEntry::InitObjectFactory(sitrep_factory);
    
    // reconstruct each sitrep entry in the update element
    GG::XMLElement sitrep = sitRepElement.Child("m_sitrep_entries");
    for(unsigned int i=0; i<sitrep.NumChildren(); i++)
    {
        emp->AddSitRepEntry( sitrep_factory.GenerateObject(sitrep.Child(i)) );
    }
        
    return true;
}*/
