import freeOrionAIInterface as fo
import FreeOrionAI as foAI
from EnumsAI import AIExplorableSystemType, AIPriorityType, getAIPriorityResourceTypes, getAIPriorityProductionTypes, AIFocusType,  AIEmpireProductionTypes
from EnumsAI import AIShipDesignTypes, AIShipRoleType,  AIFleetMissionType,  AIPriorityNames
import PlanetUtilsAI
import AIstate
import FleetUtilsAI
from random import choice
import sys
import traceback
import math
from ColonisationAI import empireSpecies, empireColonizers,  empireSpeciesSystems
import TechsListsAI


shipTypeMap = dict( zip( [AIPriorityType.PRIORITY_PRODUCTION_EXPLORATION,  AIPriorityType.PRIORITY_PRODUCTION_OUTPOST,  AIPriorityType.PRIORITY_PRODUCTION_COLONISATION,  AIPriorityType.PRIORITY_PRODUCTION_INVASION,  AIPriorityType.PRIORITY_PRODUCTION_MILITARY], 
                                            [AIShipDesignTypes.explorationShip,  AIShipDesignTypes.outpostShip,  AIShipDesignTypes.colonyShip,  AIShipDesignTypes.troopShip,  AIShipDesignTypes.attackShip ] ) )

def curBestMilShipRating():
    bestShip,  bestDesign,  buildChoices = getBestShipInfo( AIPriorityType.PRIORITY_PRODUCTION_MILITARY)
    if bestDesign is None:
        return 0.00001  #  empire cannot currently produce any military ships, don't make zero though, to avoid divide-by-zero
    stats = foAI.foAIstate.getDesignIDStats(bestDesign.id)
    return stats['attack'] * ( stats['structure'] + stats['shields'] )

def checkTroopShips():
    empire = fo.getEmpire()
    troopDesignIDs = [shipDesignID for shipDesignID in empire.allShipDesigns if shipTypeMap.get(AIPriorityType.PRIORITY_PRODUCTION_INVASION,  "nomatch")  in fo.getShipDesign(shipDesignID).name(False) ]
    troopShipNames = [fo.getShipDesign(shipDesignID).name(False) for shipDesignID in troopDesignIDs]
    print "Current Troopship Designs: %s"%troopShipNames
    
    if fo.currentTurn() >1 : return
    
    if False and len(troopDesignIDs) ==1 : 
        try:
            res=fo.issueCreateShipDesignOrder("SD_TROOP_SHIP_A2",  "Medium Hulled Troopship for economical large quantities of troops",  
                                                                                    "SH_BASIC_MEDIUM",  ["GT_TROOP_POD", "GT_TROOP_POD", "GT_TROOP_POD"],  "",  "fighter",  False)
            print "added  Troopship SD_TROOP_SHIP_A2, with result %d"%res
        except:
            print "Error: exception triggered and caught:  ",  traceback.format_exc()
    if "SD_TROOP_SHIP_A3" not in troopShipNames:
        try:
            res=fo.issueCreateShipDesignOrder("SD_TROOP_SHIP_A3",  "multicell Hulled Troopship for economical large quantities of troops",  
                                                                                    "SH_STATIC_MULTICELLULAR",  ["GT_TROOP_POD",  "GT_TROOP_POD",  "SR_WEAPON_2",  "GT_TROOP_POD", ""],  "",  "fighter",  False)
            print "added  Troopship SD_TROOP_SHIP_A3, with result %d"%res
        except:
            print "Error: exception triggered and caught:  ",  traceback.format_exc()
    if "SD_TROOP_SHIP_A4" not in troopShipNames:
        try:
            res=fo.issueCreateShipDesignOrder("SD_TROOP_SHIP_A4",  "multicell Hulled Troopship for economical large quantities of troops",  
                                                                                    "SH_STATIC_MULTICELLULAR",  ["GT_TROOP_POD",  "GT_TROOP_POD",  "SR_WEAPON_5",  "GT_TROOP_POD", ""],  "",  "fighter",  False)
            print "added  Troopship SD_TROOP_SHIP_A4, with result %d"%res
        except:
            print "Error: exception triggered and caught:  ",  traceback.format_exc()
    bestShip,  bestDesign,  buildChoices = getBestShipInfo( AIPriorityType.PRIORITY_PRODUCTION_INVASION)
    if bestDesign:
        print "Best Troopship buildable is %s"%bestDesign.name(False)
    else:
        print "Troopships apparently unbuildable at present,  ruh-roh"

def checkScouts():
    empire = fo.getEmpire()
    scoutDesignIDs = [shipDesignID for shipDesignID in empire.allShipDesigns if shipTypeMap.get(AIPriorityType.PRIORITY_PRODUCTION_EXPLORATION,  "nomatch")  in fo.getShipDesign(shipDesignID).name(False) ]
    scoutShipNames = [fo.getShipDesign(shipDesignID).name(False) for shipDesignID in scoutDesignIDs]
    #print "Current Scout Designs: %s"%scoutShipNames
    #                                                            name               desc            hull                partslist                              icon                 model
    newScoutDesigns = []
    desc = "SD_SCOUT_DESC"
    model = "fighter"
    srb = "SR_WEAPON_%1d"
    nb,  hull =  "SD_SCOUT_A%1d_%1d",   "SH_STATIC_MULTICELLULAR"
    db = "DT_DETECTOR_%1d"
    is1,  is2 = "FU_BASIC_TANK",  "ST_CLOAK_1"
    for id in [1, 2, 3, 4]:
        newScoutDesigns += [ (nb%(id, iw),  desc,  hull,  [ db%id,  srb%iw, srb%iw,  is1,  is1],  "",  model)    for iw in range(1, 9) ]
    nb =  "SD_SCOUT_B%1d_%1d"
    for id in [1, 2, 3, 4]:
        newScoutDesigns += [ (nb%(id, iw),  desc,  hull,  [ db%id,  srb%iw, srb%iw,  is1,  is2],  "",  model)    for iw in range(1, 9) ]

    currentTurn=fo.currentTurn()
    needsAdding=[]
    namesToAdd=[]
    for name,  desc,  hull,  partslist,  icon,  model in newScoutDesigns:
        if name not in scoutShipNames:
            needsAdding.append( ( name,  desc,  hull,  partslist,  icon,  model) )
            namesToAdd.append( name )

    if needsAdding != []:
        print "--------------"
        print "Current Scout Designs: %s"%scoutShipNames
        print "-----------"
        print "Scout design names apparently needing to be added: %s"%namesToAdd
        print "-------"
        if currentTurn ==1:  #due to some apparent problem with these repeatedly being added, only do it on first turn
            for name,  desc,  hull,  partslist,  icon,  model in needsAdding:
                try:
                    res=fo.issueCreateShipDesignOrder( name,  desc,  hull,  partslist,  icon,  model, False)
                    print "added  Scout Design %s, with result %d"%(name,  res)
                except:
                    print "Error: exception triggered and caught adding scout %s:  "%name,  traceback.format_exc()

    bestShip,  bestDesign,  buildChoices = getBestShipInfo( AIPriorityType.PRIORITY_PRODUCTION_EXPLORATION)
    if bestDesign:
        print "Best Scout buildable is %s"%bestDesign.name(False)
    else:
        print "Scouts apparently unbuildable at present,  ruh-roh"
    #TODO: add more advanced designs


def checkMarks():
    empire = fo.getEmpire()
    markDesignIDs = [shipDesignID for shipDesignID in empire.allShipDesigns if shipTypeMap.get(AIPriorityType.PRIORITY_PRODUCTION_MILITARY,  "nomatch")  in fo.getShipDesign(shipDesignID).name(False) ]
    markShipNames = [fo.getShipDesign(shipDesignID).name(False) for shipDesignID in markDesignIDs]
    #print "Current Mark Designs: %s"%markShipNames
    #                                                            name               desc            hull                partslist                              icon                 model
    newMarkDesigns = []
    desc = "SD_MARK1_DESC"
    model = "fighter"
    srb = "SR_WEAPON_%1d"
    nb,  hull =  "SD_MARK_Z_A%1d",   "SH_BASIC_MEDIUM"
    newMarkDesigns += [ (nb%iw,  desc,  hull,  [ srb%iw,  srb%iw,  ""],  "",  model)    for iw in range(1, 9) ]
    
    nb,  hull =  "SD_MARK_Z_C_MC%1d",   "SH_STATIC_MULTICELLULAR"
    is1,  is2 = "FU_BASIC_TANK",  "FU_BASIC_TANK"
    newMarkDesigns += [ (nb%iw,  desc,  hull,  [ srb%iw,  srb%iw, srb%iw,  is1,  is2],  "",  model)    for iw in range(2, 9) ]
    nb=  "SD_MARK_Z_C2_MC%1d"
    is2 = "SH_DEFLECTOR"
    newMarkDesigns += [ (nb%iw,  desc,  hull,  [ srb%iw,  srb%iw, srb%iw,  is1,  is2],  "",  model)    for iw in range(7, 9) ]
    
    nb,  hull =  "SD_MARK_Z_D_ENDO%1d",   "SH_ENDOMORPHIC"
    is1 = "FU_BASIC_TANK"
    #intentionally skipping 9 due to jump in expense
    newMarkDesigns += [ (nb%iw,  desc,  hull,  4*[srb%iw] + 3*[ is1],  "",  model)    for iw in [5, 6, 7, 8,   10, 11, 12 ] ]

    nb =  "SD_MARK_Z_D2_ENDO%1d"
    is3 = "SH_DEFLECTOR"
    #intentionally skipping 9 due to jump in expense
    newMarkDesigns += [ (nb%iw,  desc,  hull,  4*[srb%iw] + [ is1,  is1,  is3],  "",  model)    for iw in [7, 8,   10, 11, 12 ] ]

    nb =  "SD_MARK_Z_D3_ENDO%1d"
    ar1 = "AR_LEAD_PLATE"
    #intentionally skipping 9 due to jump in expense
    newMarkDesigns += [ (nb%iw,  desc,  hull,  3*[srb%iw]+[ar1] + [ is1,  is1,  is1],  "",  model)    for iw in [5, 6, 7, 8,   10, 11, 12 ] ]

    nb =  "SD_MARK_Z_D4_ENDO%1d"
    #intentionally skipping 9 due to jump in expense
    newMarkDesigns += [ (nb%iw,  desc,  hull,  3*[srb%iw]+[ar1] + [ is1,  is1,  is3],  "",  model)    for iw in [7, 8,   10, 11, 12 ] ]

    nb =  "SD_MARK_Z_D5_ENDO%1d"
    ar2= "AR_ZORTRIUM_PLATE"
    #intentionally skipping 9 due to jump in expense
    newMarkDesigns += [ (nb%iw,  desc,  hull,  3*[srb%iw]+[ar2] + [ is1,  is1,  is3],  "",  model)    for iw in [7, 8,   10, 11, 12,  14,  15,  16,  17 ] ]
    
    nb =  "SD_MARK_Z_D6_ENDO%1d"
    ar3= "AR_NEUTRONIUM_PLATE"
    #intentionally skipping 9 due to jump in expense
    newMarkDesigns += [ (nb%iw,  desc,  hull,  3*[srb%iw] +[ar3]+ [ is1,  is3,  is3],  "",  model)    for iw in [7, 8,   10, 11, 12,  14,  15,  16,  17 ] ]
    
    nb =  "SD_MARK_Z_D7_ENDO%1d"
    is3= "SH_MULTISPEC"
    #intentionally skipping 9 due to jump in expense
    newMarkDesigns += [ (nb%iw,  desc,  hull,  4*[srb%iw] + [ is3,  is3,  is3],  "",  model)    for iw in [7, 8,   10, 11, 12,  14,  15,  16,  17 ] ]
    
    nb =  "SD_MARK_Z_D8_ENDO%1d"
    #intentionally skipping 9 due to jump in expense
    newMarkDesigns += [ (nb%iw,  desc,  hull,  3*[srb%iw]+[ar2] + [ is3,  is3,  is3],  "",  model)    for iw in [7, 8,   10, 11, 12,  14,  15,  16,  17 ] ]
    
    nb =  "SD_MARK_Z_D9_ENDO%1d"
    #intentionally skipping 9 due to jump in expense
    newMarkDesigns += [ (nb%iw,  desc,  hull,  3*[srb%iw]+[ar3] + [ is3,  is3,  is3],  "",  model)    for iw in [7, 8,   10, 11, 12,  14,  15,  16,  17 ] ]
    
    currentTurn=fo.currentTurn()
    needsAdding=[]
    namesToAdd=[]
    for name,  desc,  hull,  partslist,  icon,  model in newMarkDesigns:
        if name not in markShipNames:
            needsAdding.append( ( name,  desc,  hull,  partslist,  icon,  model) )
            namesToAdd.append( name )

    if needsAdding != []:
        print "--------------"
        print "Current Mark Designs: %s"%markShipNames
        print "-----------"
        print "Mark design names apparently needing to be added: %s"%namesToAdd
        print "-------"
        if currentTurn ==1:  #due to some apparent problem with these repeatedly being added, only do it on first turn
            for name,  desc,  hull,  partslist,  icon,  model in needsAdding:
                try:
                    res=fo.issueCreateShipDesignOrder( name,  desc,  hull,  partslist,  icon,  model,  False)
                    print "added  Mark Design %s, with result %d"%(name,  res)
                except:
                    print "Error: exception triggered and caught adding mark %s:  "%name,  traceback.format_exc()

    bestShip,  bestDesign,  buildChoices = getBestShipInfo( AIPriorityType.PRIORITY_PRODUCTION_MILITARY)
    if bestDesign:
        print "Best Mark buildable is %s"%bestDesign.name(False)
    else:
        print "Marks apparently unbuildable at present,  ruh-roh"
    #TODO: add more advanced designs

def checkOutpostShips():
    empire = fo.getEmpire()
    outpostDesignIDs = [shipDesignID for shipDesignID in empire.allShipDesigns if shipTypeMap.get(AIPriorityType.PRIORITY_PRODUCTION_OUTPOST,  "nomatch")  in fo.getShipDesign(shipDesignID).name(False) ]
    outpostShipNames = [fo.getShipDesign(shipDesignID).name(False) for shipDesignID in outpostDesignIDs]
    #print "Current Outpost Designs: %s"%scoutShipNames
    #                                                            name               desc            hull                partslist                              icon                 model
    newOutpostDesigns = []
    desc = "SD_OUTPOST_SHIP_DESC"
    srb = "SR_WEAPON_%1d"
    model = "seed"
    nb,  hull =  "SD_OUTPOST_SHIP_A%1d_%1d",   "SH_STATIC_MULTICELLULAR"
    op = "CO_OUTPOST_POD"
    db = "DT_DETECTOR_%1d"
    is1,  is2 = "FU_BASIC_TANK",  "ST_CLOAK_1"
    for id in [1, 2, 3, 4]:
        newOutpostDesigns += [ (nb%(id, iw),  desc,  hull,  [ srb%iw, db%id, "",  op,  is1],  "",  model)    for iw in range(2, 9) ]

    currentTurn=fo.currentTurn()
    needsAdding=[]
    namesToAdd=[]
    for name,  desc,  hull,  partslist,  icon,  model in newOutpostDesigns:
        if name not in outpostShipNames:
            needsAdding.append( ( name,  desc,  hull,  partslist,  icon,  model) )
            namesToAdd.append( name )

    if needsAdding != []:
        print "--------------"
        print "Current Outpost Designs: %s"%outpostShipNames
        print "-----------"
        print "Outpost design names apparently needing to be added: %s"%namesToAdd
        print "-------"
        if currentTurn ==1:  #due to some apparent problem with these repeatedly being added, only do it on first turn
            for name,  desc,  hull,  partslist,  icon,  model in needsAdding:
                try:
                    res=fo.issueCreateShipDesignOrder( name,  desc,  hull,  partslist,  icon,  model,  False)
                    print "added  Mark Design %s, with result %d"%(name,  res)
                except:
                    print "Error: exception triggered and caught adding outpost %s:  "%name,  traceback.format_exc()

    bestShip,  bestDesign,  buildChoices = getBestShipInfo( AIPriorityType.PRIORITY_PRODUCTION_OUTPOST)
    if bestDesign:
        print "Best Outpost buildable is %s"%bestDesign.name(False)
    else:
        print "Outpost ships apparently unbuildable at present,  ruh-roh"

def checkColonyShips():
    empire = fo.getEmpire()
    colonyDesignIDs = [shipDesignID for shipDesignID in empire.allShipDesigns if shipTypeMap.get(AIPriorityType.PRIORITY_PRODUCTION_COLONISATION,  "nomatch")  in fo.getShipDesign(shipDesignID).name(False) ]
    colonyShipNames = [fo.getShipDesign(shipDesignID).name(False) for shipDesignID in colonyDesignIDs]
    #print "Current Outpost Designs: %s"%scoutShipNames
    #                                                            name               desc            hull                partslist                              icon                 model
    newColonyDesigns = []
    desc = "SD_COLONY_SHIP_DESC"
    model = "seed"
    srb = "SR_WEAPON_%1d"
    nb,  hull =  "SD_COLONY_SHIP_A%1d_%1d",   "SH_STATIC_MULTICELLULAR"
    cp = "CO_COLONY_POD"
    db = "DT_DETECTOR_%1d"
    is1,  is2 = "FU_BASIC_TANK",  "ST_CLOAK_1"
    for id in [1, 2, 3, 4]:
        newColonyDesigns += [ (nb%(id, iw),  desc,  hull,  [ srb%iw, db%id, "",  cp,  is1],  "",  model)    for iw in range(2, 6) ]
    for id in [1, 2, 3, 4]:
        newColonyDesigns += [ (nb%(id, iw),  desc,  hull,  [ srb%iw, db%id, "",  cp,  cp],  "",  model)    for iw in range(6, 9) ] # when farther along, use 2 pods

    nb =  "SD_COLONY_SHIP_B%1d_%1d"
    cp = "CO_SUSPEND_ANIM_POD"
    for id in [1, 2, 3, 4]:
        newColonyDesigns += [ (nb%(id, iw),  desc,  hull,  [ srb%iw, db%id, "",  cp,  is1],  "",  model)    for iw in range(2, 9) ]

    currentTurn=fo.currentTurn()
    needsAdding=[]
    namesToAdd=[]
    for name,  desc,  hull,  partslist,  icon,  model in newColonyDesigns:
        if name not in colonyShipNames:
            needsAdding.append( ( name,  desc,  hull,  partslist,  icon,  model) )
            namesToAdd.append( name )

    if needsAdding != []:
        print "--------------"
        print "Current Colony  Designs: %s"%newColonyDesigns
        print "-----------"
        print "Colony design names apparently needing to be added: %s"%namesToAdd
        print "-------"
        if currentTurn ==1:  #due to some apparent problem with these repeatedly being added, only do it on first turn
            for name,  desc,  hull,  partslist,  icon,  model in needsAdding:
                try:
                    res=fo.issueCreateShipDesignOrder( name,  desc,  hull,  partslist,  icon,  model,  False)
                    print "added  Colony Design %s, with result %d"%(name,  res)
                except:
                    print "Error: exception triggered and caught adding colony  %s:  "%name,  traceback.format_exc()

    bestShip,  bestDesign,  buildChoices = getBestShipInfo( AIPriorityType.PRIORITY_PRODUCTION_COLONISATION)
    if bestDesign:
        print "Best Colony ship buildable is %s"%bestDesign.name(False)
    else:
        print "Colony ships apparently unbuildable at present,  ruh-roh"

    
def getBestShipInfo(priority):
    "returns designID,  design,  buildLocList"
    empire = fo.getEmpire()
    theseDesigns = [shipDesign for shipDesign in empire.availableShipDesigns if shipTypeMap.get(priority,  "nomatch")  in fo.getShipDesign(shipDesign).name(False)  and getAvailableBuildLocations(shipDesign) != [] ]
    if theseDesigns == []: 
        return None,  None,  None #must be missing a Shipyard (or checking for outpost ship but missing tech)
    ships = [ ( fo.getShipDesign(shipDesign).name(False),  shipDesign) for shipDesign in theseDesigns ]
    bestShip = sorted( ships)[-1][-1]
    buildChoices = getAvailableBuildLocations(bestShip)
    bestDesign=  fo.getShipDesign(bestShip)
    return bestShip,  bestDesign,  buildChoices

def generateProductionOrders():
    "generate production orders"
    empire = fo.getEmpire()
    universe = fo.getUniverse()
    capitolID = PlanetUtilsAI.getCapital()
    homeworld = universe.getPlanet(capitolID)
    print "Production Queue Management:"
    empire = fo.getEmpire()
    productionQueue = empire.productionQueue
    totalPP = empire.productionPoints
    print ""
    print "  Total Available Production Points: " + str(totalPP)
    
    try:    checkScouts()
    except: print "Error: exception triggered and caught:  ",  traceback.format_exc()
    try:    checkTroopShips()
    except: print "Error: exception triggered and caught:  ",  traceback.format_exc()
    try:    checkMarks()
    except: print "Error: exception triggered and caught:  ",  traceback.format_exc()
    try:    checkColonyShips()
    except: print "Error: exception triggered and caught:  ",  traceback.format_exc()
    try:    checkOutpostShips()
    except: print "Error: exception triggered and caught:  ",  traceback.format_exc()
    
    

    movedCapital=False
    if not homeworld:
        print "no capital, should get around to capturing or colonizing a new one"#TODO
    else:
        print "Empire ID %d has current Capital  %s:"%(empire.empireID,  homeworld.name )
        print "Buildings present at empire Capital (ID, Name, Type, Tags, Specials, OwnedbyEmpire):"
        for bldg in homeworld.buildingIDs:
            thisObj=universe.getObject(bldg)
            tags=",".join( thisObj.tags)
            specials=",".join(thisObj.specials)
            print  "%8s | %20s | type:%20s | tags:%20s | specials: %20s | owner:%d "%(bldg,  thisObj.name,  "_".join(thisObj.buildingTypeName.split("_")[-2:])[:20],  tags,  specials,  thisObj.owner )
        
        capitalBldgs = [universe.getObject(bldg).buildingTypeName for bldg in homeworld.buildingIDs]

        possibleBuildingTypeIDs = [bldTID for bldTID in empire.availableBuildingTypes if  fo.getBuildingType(bldTID).canBeProduced(empire.empireID,  homeworld.id)]
        if  possibleBuildingTypeIDs:
            print "Possible building types to build:"
            for buildingTypeID in possibleBuildingTypeIDs:
                buildingType = fo.getBuildingType(buildingTypeID)
                #print "buildingType  object:",  buildingType
                #print "dir(buildingType): ",  dir(buildingType)
                print "    " + str(buildingType.name)  + " cost: " +str(buildingType.productionCost(empire.empireID,  homeworld.id)) + " time: " + str(buildingType.productionTime(empire.empireID,  homeworld.id))
                
            possibleBuildingTypes = [fo.getBuildingType(buildingTypeID) and  fo.getBuildingType(buildingTypeID).name  for buildingTypeID in possibleBuildingTypeIDs ] #makes sure is not None before getting name

            print ""
            print "Buildings already in Production Queue:"
            capitolQueuedBldgs=[element for element in productionQueue if (element.buildType == AIEmpireProductionTypes.BT_BUILDING and element.locationID==homeworld.id)]
            for bldg in capitolQueuedBldgs:
                print "    " + bldg.name + " turns:" + str(bldg.turnsLeft) + " PP:" + str(bldg.allocation)
            if capitolQueuedBldgs == []: print "None"
            print
            queuedBldgNames=[ bldg.name for bldg in capitolQueuedBldgs ]

            if  ("BLD_INDUSTRY_CENTER" in possibleBuildingTypes) and ("BLD_INDUSTRY_CENTER" not in (capitalBldgs+queuedBldgNames)):
                res=fo.issueEnqueueBuildingProductionOrder("BLD_INDUSTRY_CENTER", empire.capitalID)
                print "Enqueueing BLD_INDUSTRY_CENTER, with result %d"%res

            if  ("BLD_SHIPYARD_BASE" in possibleBuildingTypes) and ("BLD_SHIPYARD_BASE" not in (capitalBldgs+queuedBldgNames)):
                try:
                    res=fo.issueEnqueueBuildingProductionOrder("BLD_SHIPYARD_BASE", empire.capitalID)
                    print "Enqueueing BLD_SHIPYARD_BASE, with result %d"%res
                except:
                    print "Error: cant build shipyard at new capital,  probably no population; we're hosed"
                    print "Error: exception triggered and caught:  ",  traceback.format_exc()

            if  ("BLD_SHIPYARD_ORG_ORB_INC" in possibleBuildingTypes) and ("BLD_SHIPYARD_ORG_ORB_INC" not in (capitalBldgs+queuedBldgNames)):
                try:
                    res=fo.issueEnqueueBuildingProductionOrder("BLD_SHIPYARD_ORG_ORB_INC", empire.capitalID)
                    print "Enqueueing BLD_SHIPYARD_ORG_ORB_INC, with result %d"%res
                except:
                    print "Error: exception triggered and caught:  ",  traceback.format_exc()

            if  ("BLD_EXOBOT_SHIP" in possibleBuildingTypes) and ("BLD_EXOBOT_SHIP" not in capitalBldgs+queuedBldgNames):
                if  len( empireColonizers.get("SP_EXOBOT", []))==0 : #don't have an exobot shipyard yet
                    try:
                        res=fo.issueEnqueueBuildingProductionOrder("BLD_EXOBOT_SHIP", empire.capitalID)
                        print "Enqueueing BLD_EXOBOT_SHIP, with result %d"%res
                    except:
                        print "Error: exception triggered and caught:  ",  traceback.format_exc()

#TODO: add totalPP checks below, so don't overload queue

    queuedShipyardLocs = [element.locationID for element in productionQueue if (element.name=="BLD_SHIPYARD_BASE") ]
    theseSystems={}
    theseplanets={} #unused for now
    for specName in empireColonizers:
        if (len( empireColonizers[specName])==0) and (specName in empireSpecies): #no current shipyards for this species#TODO: also allow orbital incubators and/or asteroid ships
            for pID in empireSpecies.get(specName, []): #SP_EXOBOT may not actually have a colony yet but be in empireColonizers
                if pID in queuedShipyardLocs:
                    break
            else:  #no queued shipyards, get planets with target pop >3, and queue a shipyard on the one with biggest current pop
                planetList = zip( map( universe.getPlanet,  empireSpecies[specName]),  empireSpecies[specName] )
                pops = sorted(  [ (planet.currentMeterValue(fo.meterType.population), pID) for planet, pID in planetList if ( planet and planet.currentMeterValue(fo.meterType.targetPopulation)>=3.0)] )
                if pops != []:
                    buildLoc= pops[-1][1]
                    res=fo.issueEnqueueBuildingProductionOrder("BLD_SHIPYARD_BASE", buildLoc)
                    print "Enqueueing BLD_SHIPYARD_BASE at planet %d (%s) for colonizer species %s, with result %d"%(buildLoc, universe.getPlanet(buildLoc).name,  specName,   res)
            for pid in empireSpecies.get(specName, []): 
                planet=universe.getPlanet(pid)
                if planet:
                    theseSystems.setdefault(planet.systemID,  {}).setdefault('pids', []).append(pid)
                    theseplanets[pid]=planet.systemID

    bldName = "BLD_SHIPYARD_ORG_ORB_INC"
    if empire.buildingTypeAvailable(bldName):
        queuedBldLocs = [element.locationID for element in productionQueue if (element.name==bldName) ]
        bldType = fo.getBuildingType(bldName)
        for pid in AIstate.popCtrIDs:
            if  pid not in queuedBldLocs and bldType.canBeProduced(empire.empireID,  pid):#TODO: verify that canBeProduced() checks for prexistence of a barring building
                res=fo.issueEnqueueBuildingProductionOrder(bldName, pid)
                if res: queuedBldLocs.append(pid)
                print "Enqueueing %s at planet %d (%s) , with result %d"%(bldName,  pid, universe.getPlanet(pid).name,  res)

    bldName = "BLD_GAS_GIANT_GEN"
    if empire.buildingTypeAvailable(bldName):
        queuedBldLocs = [element.locationID for element in productionQueue if (element.name==bldName) ]
        bldType = fo.getBuildingType(bldName)
        for pid in list(AIstate.popCtrIDs) + list(AIstate.outpostIDs):
            if  pid not in queuedBldLocs and bldType.canBeProduced(empire.empireID,  pid):#TODO: verify that canBeProduced() checks for prexistence of a barring building
                res=fo.issueEnqueueBuildingProductionOrder(bldName, pid)
                if res: queuedBldLocs.append(pid)
                print "Enqueueing %s at planet %d (%s) , with result %d"%(bldName,  pid, universe.getPlanet(pid).name,  res)
    
    bldName = "BLD_SOL_ORB_GEN"
    if empire.buildingTypeAvailable(bldName):
        bldType = fo.getBuildingType(bldName)
        alreadyGotOne=99
        for pid in list(AIstate.popCtrIDs) + list(AIstate.outpostIDs):
            planet=universe.getPlanet(pid)
            if planet and bldName in [bld.name for bld in map( universe.getObject,  planet.buildingIDs)]:
                system = universe.getSystem(planet.systemID)
                if system and system.starType < alreadyGotOne:
                    alreadyGotOne = system.starType
        bestType=fo.starType.white
        best_locs = AIstate.empireStars.get(fo.starType.blue,  []) + AIstate.empireStars.get(fo.starType.white,  [])
        if best_locs==[]:
            bestType=fo.starType.orange
            best_locs = AIstate.empireStars.get(fo.starType.yellow,  []) + AIstate.empireStars.get(fo.starType.orange,  [])
        if ( not best_locs) or ( alreadyGotOne<99 and alreadyGotOne <= bestType  ):
            pass # could consider building at a red star if have a lot of  PP but somehow no better stars
        else:
            useNewLoc=True
            queuedBldLocs = [element.locationID for element in productionQueue if (element.name==bldName) ]
            if queuedBldLocs != []:
                queuedStarTypes = {}
                for loc in queuedBldLocs:
                    planet = universe.getPlanet(loc)
                    if not planet: continue
                    system = universe.getSystem(planet.systemID)
                    queuedStarTypes.setdefault(system.starType,  []).append(loc)
                if queuedStarTypes:
                    bestQueued = sorted(queuedStarTypes.keys())[0]
                    if bestQueued > bestType:  # i.e., bestQueued is yellow, bestType available is blue or white
                        pass #should probably evaluate cancelling the existing one under construction
                    else:
                        useNewLoc=False
            if useNewLoc:  # (of course, may be only loc, not really new)
                if not homeworld:
                    useSys=best_locs[0]#as good as any
                else:
                    distanceMap={}
                    for sysID in best_locs: #want to build close to capitol for defense
                        try:
                            distanceMap[sysID] = len(universe.leastJumpsPath(homeworld.systemID, sysID, empire.empireID))
                        except:
                            pass
                    useSys = ([-1] + sorted(  [ (dist,  sysID) for sysID,  dist in distanceMap.items() ] ))[:2][-1]  # kinda messy, but ensures a value
                if useSys!= -1:
                    try:
                        useLoc = AIstate.colonizedSystems[useSys][0]
                        res=fo.issueEnqueueBuildingProductionOrder(bldName, useLoc)
                        print "Enqueueing %s at planet %d (%s) , with result %d"%(bldName,  useLoc, universe.getPlanet(useLoc).name,  res)
                    except:
                        pass
    
    bldName = "BLD_BLACK_HOLE_POW_GEN"
    if empire.buildingTypeAvailable(bldName):
        bldType = fo.getBuildingType(bldName)
        alreadyGotOne=False
        for pid in list(AIstate.popCtrIDs) + list(AIstate.outpostIDs):
            planet=universe.getPlanet(pid)
            if planet and bldName in [bld.name for bld in map( universe.getObject,  planet.buildingIDs)]:
                alreadyGotOne = True
        queuedBldLocs = [element.locationID for element in productionQueue if (element.name==bldName) ]
        if (len( AIstate.empireStars.get(fo.starType.blackHole,  [])) > 0) and len (queuedBldLocs)==0 and not alreadyGotOne:  #
            if not homeworld:
                useSys= AIstate.empireStars.get(fo.starType.blackHole,  [])[0]
            else:
                for sysID in AIstate.empireStars.get(fo.starType.blackHole,  []):
                    try:
                        distanceMap[sysID] = len(universe.leastJumpsPath(homeworld.systemID, sysID, empire.empireID))
                    except:
                        pass
                useSys = ([-1] + sorted(  [ (dist,  sysID) for sysID,  dist in distanceMap.items() ] ))[:2][-1]  # kinda messy, but ensures a value
            if useSys!= -1:
                try:
                    useLoc = AIstate.colonizedSystems[useSys][0]
                    res=fo.issueEnqueueBuildingProductionOrder(bldName, useLoc)
                    print "Enqueueing %s at planet %d (%s) , with result %d"%(bldName,  useLoc, universe.getPlanet(useLoc).name,  res)
                except:
                    pass

    bldName = "BLD_ENCLAVE_VOID"
    if empire.buildingTypeAvailable(bldName):
        bldType = fo.getBuildingType(bldName)
        alreadyGotOne=False
        for pid in list(AIstate.popCtrIDs) + list(AIstate.outpostIDs):
            planet=universe.getPlanet(pid)
            if planet and bldName in [bld.name for bld in map( universe.getObject,  planet.buildingIDs)]:
                alreadyGotOne = True
        queuedLocs = [element.locationID for element in productionQueue if (element.name==bldName) ]
        if len (queuedLocs)==0 and homeworld and not alreadyGotOne:  #
            try:
                res=fo.issueEnqueueBuildingProductionOrder(bldName, capitolID)
                print "Enqueueing %s at planet %d (%s) , with result %d"%(bldName,  capitolID, universe.getPlanet(capitolID).name,  res)
            except:
                pass

    bldName = "BLD_GENOME_BANK"
    if empire.buildingTypeAvailable(bldName):
        bldType = fo.getBuildingType(bldName)
        alreadyGotOne=False
        for pid in list(AIstate.popCtrIDs) + list(AIstate.outpostIDs):
            planet=universe.getPlanet(pid)
            if planet and bldName in [bld.name for bld in map( universe.getObject,  planet.buildingIDs)]:
                alreadyGotOne = True
        queuedLocs = [element.locationID for element in productionQueue if (element.name==bldName) ]
        if len (queuedLocs)==0 and homeworld and not alreadyGotOne:  #
            try:
                res=fo.issueEnqueueBuildingProductionOrder(bldName, capitolID)
                print "Enqueueing %s at planet %d (%s) , with result %d"%(bldName,  capitolID, universe.getPlanet(capitolID).name,  res)
            except:
                pass

    bldName = "BLD_NEUTRONIUM_EXTRACTOR"
    if empire.buildingTypeAvailable(bldName) and ( [element.locationID for element in productionQueue if (element.name==bldName) ]==[]):
        bldType = fo.getBuildingType(bldName)
        alreadyGotOne=False
        for pid in list(AIstate.popCtrIDs) + list(AIstate.outpostIDs):
            planet=universe.getPlanet(pid)
            if planet and( planet.systemID in  AIstate.empireStars.get(fo.starType.neutron,  [])  )   and (bldName in [bld.name for bld in map( universe.getObject,  planet.buildingIDs)]):
                alreadyGotOne = True
        if not alreadyGotOne:
            if not homeworld:
                useSys= AIstate.empireStars.get(fo.starType.neutron,  [])[0]
            else:
                for sysID in AIstate.empireStars.get(fo.starType.neutron,  []):
                    try:
                        distanceMap[sysID] = len(universe.leastJumpsPath(homeworld.systemID, sysID, empire.empireID))
                    except:
                        pass
                useSys = ([-1] + sorted(  [ (dist,  sysID) for sysID,  dist in distanceMap.items() ] ))[:2][-1]  # kinda messy, but ensures a value
            if useSys!= -1:
                try:
                    useLoc = AIstate.colonizedSystems[useSys][0]
                    res=fo.issueEnqueueBuildingProductionOrder(bldName, useLoc)
                    print "Enqueueing %s at planet %d (%s) , with result %d"%(bldName,  useLoc, universe.getPlanet(useLoc).name,  res)
                except:
                    pass
    
    bldName = "BLD_NEUTRONIUM_FORGE"
    if empire.buildingTypeAvailable(bldName):
        queuedBldLocs = [element.locationID for element in productionQueue if (element.name==bldName) ]
        bldType = fo.getBuildingType(bldName)
        if len(queuedBldLocs) < 2 :  #don't build too many at once
            for pid in AIstate.popCtrIDs:
                if  pid not in queuedBldLocs:
                    planet=universe.getPlanet(pid)
                    if bldType.canBeProduced(empire.empireID,  pid):#TODO: verify that canBeProduced() checks for prexistence of a barring building
                        if  bldName not in [bld.name for bld in map( universe.getObject,  planet.buildingIDs)]:
                            if  "BLD_SHIPYARD_BASE" not in [bld.name for bld in map( universe.getObject,  planet.buildingIDs)]:
                                res=fo.issueEnqueueBuildingProductionOrder(bldName, pid)
                                print "Enqueueing %s at planet %d (%s) , with result %d"%(bldName,  pid, universe.getPlanet(pid).name,  res)
                                if res: 
                                    break #only initiate max of one new build per turn

#TODO:more advanced shipyards?


    totalPPSpent = fo.getEmpire().productionQueue.totalSpent
    print "  Total Production Points Spent:     " + str(totalPPSpent)

    wastedPP = totalPP - totalPPSpent
    print "  Wasted Production Points:          " + str(wastedPP)

    print ""
    print "Possible ship designs to build:"
    for shipDesignID in empire.availableShipDesigns:
        shipDesign = fo.getShipDesign(shipDesignID)
        print "    " + str(shipDesign.name(True)) + " cost:" + str(shipDesign.productionCost(empire.empireID,  homeworld.id) )+ " time:" + str(shipDesign.productionTime(empire.empireID,  homeworld.id))

    print ""
    print "Projects already in Production Queue:"
    productionQueue = empire.productionQueue
    print "production summary: %s"%[elem.name for elem in productionQueue]
    queuedColonyShips={}
    queuedOutpostShips = 0
    
    #TODO:  blocked items might not need dequeuing, but rather for supply lines to be un-blockaded 
    for queue_index  in range( len(productionQueue)):
        element=productionQueue[queue_index]
        print "    " + element.name + " turns:" + str(element.turnsLeft) + " PP:%.2f"%element.allocation + " being built at " + universe.getObject(element.locationID).name
        if element.turnsLeft == -1:
            print "element %s will never be completed as stands  "%element.name 
            #fo.issueDequeueProductionOrder(queue_index) 
            break
        elif element.buildType == AIEmpireProductionTypes.BT_SHIP:
             if foAI.foAIstate.getShipRole(element.designID) ==       AIShipRoleType.SHIP_ROLE_CIVILIAN_COLONISATION:
                 thisSpec=universe.getPlanet(element.locationID).speciesName
                 queuedColonyShips[thisSpec] =  queuedColonyShips.get(thisSpec, 0) +  element.remaining*element.blocksize
             if foAI.foAIstate.getShipRole(element.designID) ==       AIShipRoleType.SHIP_ROLE_CIVILIAN_OUTPOST:
                 queuedOutpostShips+=  element.remaining*element.blocksize
    if queuedColonyShips:
        print "\nFound  colony ships in build queue: %s"%queuedColonyShips
    if queuedOutpostShips:
        print "\nFound  colony ships in build queue: %s"%queuedOutpostShips

    print ""
    # get the highest production priorities
    productionPriorities = {}
    for priorityType in getAIPriorityProductionTypes():
        productionPriorities[priorityType] = foAI.foAIstate.getPriority(priorityType)

    sortedPriorities = productionPriorities.items()
    sortedPriorities.sort(lambda x,y: cmp(x[1], y[1]), reverse=True)

    topPriority = -1
    topscore = -1
    numColonyTargs=len(foAI.foAIstate.colonisablePlanetIDs )
    numColonyFleets=len( FleetUtilsAI.getEmpireFleetIDsByRole( AIFleetMissionType.FLEET_MISSION_COLONISATION) )# counting existing colony fleets each as one ship
    totColonyFleets = sum(queuedColonyShips.values()) + numColonyFleets
    bestShip,  bestDesign,  buildChoices = getBestShipInfo(AIPriorityType.PRIORITY_PRODUCTION_COLONISATION)
    colonyBuildLocs=[]
    speciesMap = {}
    for loc in buildChoices:
        thisSpec = universe.getPlanet(loc).speciesName
        speciesMap.setdefault(thisSpec,  []).append( loc)
    colonyBuildChoices=[]
    for pid,  score_Spec_tuple in foAI.foAIstate.colonisablePlanetIDs:
        score,  thisSpec = score_Spec_tuple
        colonyBuildChoices.extend( int(math.ceil(score))*speciesMap.get(thisSpec,  [])  )#TODO: make the list cover the best colony ship buildable for each species; don't rule out a species if can't make overall best

    numOutpostTargs=len(foAI.foAIstate.colonisableOutpostIDs )
    numOutpostFleets=len( FleetUtilsAI.getEmpireFleetIDsByRole( AIFleetMissionType.FLEET_MISSION_OUTPOST) )# counting existing outpost fleets each as one ship
    totOutpostFleets = queuedOutpostShips + numOutpostFleets

    print "Production Queue Priorities:"
    filteredPriorities = {}
    for ID,  score in sortedPriorities:
        if topscore < score:
            topPriority = ID
            topscore = score #don't really need topscore nor sorting with current handling
        print " Score: %4d -- %s "%(score,  AIPriorityNames[ID] )
        if ID != AIPriorityType.PRIORITY_PRODUCTION_BUILDINGS:
            if ( ID == AIPriorityType.PRIORITY_PRODUCTION_COLONISATION ) and (  totColonyFleets <  numColonyTargs+1+int(fo.currentTurn()/10)   ) and len(colonyBuildChoices) >0:
                filteredPriorities[ID]= score
            elif ( ID == AIPriorityType.PRIORITY_PRODUCTION_OUTPOST ) and (  totOutpostFleets <  numOutpostTargs+1+int(fo.currentTurn()/20)   ):
                filteredPriorities[ID]= score
            elif ID not in [AIPriorityType.PRIORITY_PRODUCTION_OUTPOST ,  AIPriorityType.PRIORITY_PRODUCTION_COLONISATION ]:
                filteredPriorities[ID]= score
    if filteredPriorities == {}:
        print "No non-building-production priorities with nonzero score, setting to default: Military"
        filteredPriorities [AIPriorityType.PRIORITY_PRODUCTION_MILITARY ] =  1 
    while (topscore >5000):
        topscore = 0
        for pty in filteredPriorities:
            score = filteredPriorities[pty]
            if score <= 500:
                score = math.ceil(score/50)
            else:
                score = math.ceil(score/100)
            filteredPriorities[pty] = score
            if score > topscore:
                topscore=score

    bestShips={}
    for priority in list(filteredPriorities):
        bestShip,  bestDesign,  buildChoices = getBestShipInfo(priority)
        if bestShip is None:
            del filteredPriorities[priority] #must be missing a shipyard -- TODO build a shipyard if necessary
            continue
        bestShips[priority] = [bestShip,  bestDesign,  buildChoices ]
        
    priorityChoices=[]
    for priority in filteredPriorities:
        priorityChoices.extend( int(filteredPriorities[priority]) * [priority] )

    #print "  Top Production Queue Priority: " + str(topPriority)
    #print "\n ship priority selection list: \n %s \n\n"%str(priorityChoices)
    loopCount = 0
        
    nextIdx = len(productionQueue)
    while (wastedPP > 0) and (loopCount <100) and (priorityChoices != [] ): #make sure don't get stuck in some nonbreaking loop like if all shipyards captured
        loopCount +=1
        print "Beginning  build enqueue loop %d; %.1f PP available"%(loopCount,  wastedPP)
        thisPriority = choice( priorityChoices )
        print "selected priority: ",  AIPriorityNames[thisPriority]
        makingColonyShip=False
        makingOutpostShip=False
        if ( thisPriority ==  AIPriorityType.PRIORITY_PRODUCTION_COLONISATION ):
            if ( totColonyFleets >=  numColonyTargs+1+int(fo.currentTurn()/10) ):
                print "Already sufficient colony ships in queue,  trying next priority choice"
                print ""
                continue
            else:
                makingColonyShip=True
        if ( thisPriority ==  AIPriorityType.PRIORITY_PRODUCTION_OUTPOST ):
            if ( totOutpostFleets >=  numOutpostTargs+1+int(fo.currentTurn()/20) ):
                print "Already sufficient outpost ships in queue,  trying next priority choice"
                print ""
                continue
            else:
                makingOutpostShip=True
        bestShip,  bestDesign,  buildChoices = bestShips[thisPriority]
        if makingColonyShip:
            loc = choice(colonyBuildChoices)
        else:
            loc = choice(buildChoices)
        numShips=1
        perTurnCost = (float(bestDesign.productionCost(empire.empireID,  homeworld.id)) / bestDesign.productionTime(empire.empireID,  loc))
        if  not ( makingColonyShip or  makingOutpostShip ): #TODO: consider whether to allow multiples of colony or outpost ships; if not, priority sampling gets skewed
            while ( totalPP > 25*perTurnCost):
                numShips *= 5
                perTurnCost *= 5
        retval  = fo.issueEnqueueShipProductionOrder(bestShip, loc)
        if retval !=0:
            print "adding %d new ship(s) to production queue:  %s; per turn production cost %.1f"%(numShips,  bestDesign.name(True),  perTurnCost)
            print ""
            if numShips>1:
                fo.issueChangeProductionQuantityOrder(nextIdx,  1,  numShips)
            wastedPP -=  perTurnCost
            nextIdx+=1
            if makingColonyShip:
                totColonyFleets +=numShips  # assumes the enqueueing below succeeds, but really no harm if assumption proves wrong
            if makingOutpostShip:
                totOutpostFleets +=numShips  # assumes the enqueueing below succeeds, but really no harm if assumption proves wrong
    print ""

def getAvailableBuildLocations(shipDesignID):
    "returns locations where shipDesign can be built"

    result = []

    systemIDs = foAI.foAIstate.getExplorableSystems(AIExplorableSystemType.EXPLORABLE_SYSTEM_EXPLORED)
    planetIDs = PlanetUtilsAI.getPlanetsInSystemsIDs(systemIDs)
    shipDesign = fo.getShipDesign(shipDesignID)
    empire = fo.getEmpire()
    empireID = empire.empireID
    for planetID in planetIDs:
        if shipDesign.productionLocationForEmpire(empireID, planetID):
            result.append(planetID)

    return result

def spentPP():
    "calculate PPs spent this turn so far"

    queue = fo.getEmpire().productionQueue
    return queue.totalSpent
