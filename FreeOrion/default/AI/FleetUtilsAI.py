import freeOrionAIInterface as fo
import FreeOrionAI as foAI
import EnumsAI
from EnumsAI import AIFleetMissionType, AIShipRoleType, AIExplorableSystemType
import ProductionAI

__designStats={}
__AIShipRoleTypeNames = AIShipRoleType()
__AIFleetMissionTypeNames = AIFleetMissionType()

def clearShipDesignInfo():
    __designRoles.clear()
    __designStats.clear()
    
def statsMeetReqs(stats,  reqs):
    try:
        for key in reqs:
            #print " -- FleetUtilsAI.statsMeetsReqs checking stat %s for value >= %f   :  found  val % f "%(key,   reqs[key],  stats.get(key,  0) )
            if stats.get(key,  0) < reqs[key]:
                return False
        return True
    except:
        return False
        
def countPartsFleetwide(fleetID,  partsList):
    tally=0
    universe = fo.getUniverse()
    fleet = universe.getFleet(fleetID)
    if not fleet:  return 0
    for shipID in fleet.shipIDs:
        ship = universe.getShip(shipID)
        if not ship: continue
        design = ship.design
        if not design: continue
        for part in design.parts:
            if part in partsList:
                tally += 1
    return tally

def getFleetsForMission(nships,  targetStats,  minStats,  curStats,  species,  systemsToCheck,  systemsChecked, fleetPool,   fleetList, takeAny=False,  verbose=False): #implements breadth-first search through systems
    if verbose:
        print "getFleetsForMission: (nships:%1d,  targetStats:%6d,  minStats:%6d, curStats:%6d,  species:%6s,  systemsToCheck:%8s,  systemsChecked:%8s, fleetPool:%8s,   fleetList:%8s) "%(
                                                                                                                                        nships,  targetStats,  minStats, curStats,  species,  systemsToCheck,  systemsChecked, fleetPool,   fleetList)
    universe = fo.getUniverse()
    if not (systemsToCheck and fleetPool):
        if verbose: 
            print "no more systems or fleets to check"
        if takeAny or ( statsMeetReqs(curStats,  minStats)  and ( sum( [len(universe.getFleet(fID).shipIDs) for fID in fleetList] )  >= nships)):
            return fleetList
        else:
            return []
    thisSystemID = systemsToCheck.pop(0) #take the head of the line
    systemsChecked.append(thisSystemID)
    #thisSys = universe.getSystem(thisSystemID)
    #if not thisSys:
    #    return getFleetsForMission(nships....
    fleetsHere = [fleetID for fleetID in fleetPool if ( foAI.foAIstate.fleetStatus.get(fleetID,  {}).get('sysID',  -1) == thisSystemID ) ]
    if verbose:
        print "found fleetPool Fleets  %s"%fleetsHere
    while fleetsHere !=[]:
        fleetID=fleetsHere.pop(0)
        fleet = universe.getFleet(fleetID)
        if not fleet: 
            del fleetPool[ fleetPool.index( fleetID) ]
            continue
        if len (list(fleet.shipIDs)) > 1:
            newFleets = splitFleet(fleetID) # try splitting fleet
            fleetPool.extend(newFleets)
            fleetsHere.extend(newFleets)
        meetsSpeciesReq=False
        if (species == ""): 
            meetsSpeciesReq=True
        else:
            for shipID in fleet.shipIDs:
                ship = universe.getShip(shipID)
                if ((foAI.foAIstate.getShipRole(ship.design.id) == AIShipRoleType.SHIP_ROLE_CIVILIAN_COLONISATION)  and ship.speciesName==species):
                    meetsSpeciesReq=True
                    break
        if meetsSpeciesReq:
            fleetList.append(fleetID)
            del fleetPool[ fleetPool.index( fleetID) ]
            curStats['rating'] = curStats.get('rating',  0) + foAI.foAIstate.getRating(fleetID)
            if 'troopPods' in targetStats:
                curStats['troopPods'] = curStats.get('troopPods',  0) + countPartsFleetwide(fleetID,  ["GT_TROOP_POD"])
            if  ( sum( [len(universe.getFleet(fID).shipIDs) for fID in fleetList] )  >= nships ) and statsMeetReqs(curStats,  targetStats)    :
                if verbose: 
                    print  "returning fleetlist: %s"%fleetList
                return fleetList
    # finished loop without meeting reqs
    thisSys = universe.getSystem(thisSystemID)
    for neighborID in [el.key() for el in universe.getSystemNeighborsMap(thisSystemID,  foAI.foAIstate.empireID) ]:
        if neighborID not in systemsChecked and neighborID in foAI.foAIstate.exploredSystemIDs:
            systemsToCheck.append(neighborID)
    return getFleetsForMission(nships,  targetStats,  minStats,  curStats,  species,  systemsToCheck,  systemsChecked, fleetPool,  fleetList,  takeAny,  verbose)
    
    
def getEmpireFleets(empireID=None):
    if empireID is None:
        empireID=foAI.foAIstate.empireID
    universe = fo.getUniverse()
    empireFleets=[]
    for fleetID in universe.fleetIDs:  #this actually currently pulls all visible fleets, not just empire-owned
        fleet=universe.getFleet(fleetID)
        if fleet and fleet.ownedBy(empireID):
            empireFleets.append(fleetID)
    newFleets=[]
    for fleetID in foAI.foAIState.newlySplitFleets:
        fleet=universe.getFleet(fleetID)
        if fleet:
            print "adding newly split fleet %d to empire fleets"%fleetID
            newFleets.append(fleetID)
        else:
            print "newly split fleet %d unavailable from universe; can't add  to empire fleets"%fleetID
            
    return empireFleets+newFleets

        
def splitFleet(fleetID):
    "splits a fleet into its ships"

    universe = fo.getUniverse()
    empireID = fo.empireID()

    fleet = universe.getFleet(fleetID)
    newfleets = []

    if fleet == None: return []
    if not fleet.ownedBy(empireID): return []

    if len(list(fleet.shipIDs)) <= 1:  # fleet with only one ship cannot be split
       return [fleetID]
    shipIDs = list( fleet.shipIDs )
    for shipID in shipIDs[1:]:
        newFleetID = fo.issueNewFleetOrder("Fleet %d"%(shipID), shipID)
        if newFleetID:
            newFleet=universe.getFleet(newFleetID)
            if not newFleet:
                print "Error: newly split fleet %d not available from universe"%newFleetID
            fo.issueRenameOrder(newFleetID,  "Fleet %5d"%newFleetID) #to ease review of debugging logs
            fo.issueAggressionOrder(newFleetID,  True)
            role = foAI.foAIstate.getFleetRole(newFleetID) #and mission?
            foAI.foAIstate.getRating(newFleetID) #
            newfleets.append(newFleetID)
            foAI.foAIstate.newlySplitFleets[newFleetID]=True
        else:
            print "Error - got no fleet ID back after trying to split a ship from fleet %d"%fleetID
    foAI.foAIstate.getFleetRole(fleetID) #
    foAI.foAIstate.getRating(fleetID) #
    foAI.foAIstate.ensureHaveFleetMissions(newfleets)

    return newfleets

def fleetHasShipWithRole(fleetID, shipRole):
    "returns True if a ship with shipRole is in the fleet"

    universe = fo.getUniverse()
    fleet = universe.getFleet(fleetID)

    if fleet == None: return False
    for shipID in fleet.shipIDs:
        ship = universe.getShip(shipID)
        if (foAI.foAIstate.getShipRole(ship.design.id) == shipRole):
            return True

    return False

def getShipIDWithRole(fleetID, shipRole):
    "returns a ship with the specified role in the fleet"

    if not fleetHasShipWithRole(fleetID, shipRole):
        print "No ship with role " + shipRole + " found."
        return None

    universe = fo.getUniverse()
    fleet = universe.getFleet(fleetID)

    for shipID in fleet.shipIDs:
        ship = universe.getShip(shipID)
        if (foAI.foAIstate.getShipRole(ship.design.id) == shipRole):
            return shipID

def getAllEverVisibleFleetIDs(): #may be only currently visible
    return  fo.getUniverse().fleetIDs

def getEmpireFleetIDs( empireID=None):
    "returns all fleetIDs of specified empire, defauls to current empire"
    if empireID is None:
        empireID = foAI.foAIstate.empireID
    universe = fo.getUniverse()
    empireFleetIDs = []
    destroyedObjectIDs = universe.destroyedObjectIDs(empireID)
    for fleetID in universe.fleetIDs:
        fleet = universe.getFleet(fleetID)
        if (fleet == None): continue
        if ( fleet.ownedBy(empireID))  and (fleetID not in destroyedObjectIDs) and (not(fleet.empty) )and  (not (len(fleet.shipIDs)==0) ):
            empireFleetIDs.append( fleetID )
    return empireFleetIDs

def getEmpireFleetIDsByRole(fleetRole):
    "returns a list with fleetIDs that have the specified role"
    fleetIDs = getEmpireFleetIDs()
    fleetIDsWithRole = []
    for fleetID in fleetIDs:
        if not (foAI.foAIstate.getFleetRole(fleetID) == fleetRole): continue
        fleetIDsWithRole.append(fleetID)
    return fleetIDsWithRole

def extractFleetIDsWithoutMissionTypes(fleetIDs):
    "extracts a list with fleetIDs that have no mission"
    fleetIDsWithoutMission = []
    for fleetID in fleetIDs:
        aiFleetMission = foAI.foAIstate.getAIFleetMission(fleetID)
        if not aiFleetMission.hasAnyAIMissionTypes():
            fleetIDsWithoutMission.append(fleetID)
    return fleetIDsWithoutMission

def assessFleetRole(fleetID):
    "assesses ShipRoles represented in a fleet and returns a corresponding overall fleetRole"
    universe = fo.getUniverse()
    shipRoles = {}
    fleet = universe.getFleet(fleetID)
    if not fleet:
        print "couldn't get fleet with id " + str(fleetID)
        return AIShipRoleType.SHIP_ROLE_INVALID

    # count shipRoles
    for shipID in fleet.shipIDs:
        ship = universe.getShip(shipID)
        if ship.design:
            role = foAI.foAIstate.getShipRole(ship.design.id)
        else:
            role = AIShipRoleType.SHIP_ROLE_INVALID

        if role != AIShipRoleType.SHIP_ROLE_INVALID:
            shipRoles[role] = shipRoles.get(role,  0)  + 1
    # determine most common shipRole
    favouriteRole = AIShipRoleType.SHIP_ROLE_INVALID
    for shipRole in shipRoles:
        if shipRoles[shipRole] == max(shipRoles.values()):
            favouriteRole = shipRole

    # assign fleet role
    if  AIShipRoleType.SHIP_ROLE_CIVILIAN_COLONISATION in shipRoles:
        selectedRole= AIFleetMissionType.FLEET_MISSION_COLONISATION
    elif AIShipRoleType.SHIP_ROLE_CIVILIAN_OUTPOST in shipRoles:
        selectedRole= AIFleetMissionType.FLEET_MISSION_OUTPOST
        
    elif favouriteRole == AIShipRoleType.SHIP_ROLE_CIVILIAN_EXPLORATION:
        selectedRole= AIFleetMissionType.FLEET_MISSION_EXPLORATION
    elif favouriteRole == AIShipRoleType.SHIP_ROLE_MILITARY_INVASION:
        selectedRole= AIFleetMissionType.FLEET_MISSION_INVASION
    elif favouriteRole == AIShipRoleType.SHIP_ROLE_MILITARY_ATTACK:
        selectedRole= AIFleetMissionType.FLEET_MISSION_ATTACK
    elif favouriteRole == AIShipRoleType.SHIP_ROLE_MILITARY:
        selectedRole= AIFleetMissionType.FLEET_MISSION_MILITARY
    else:
        selectedRole= AIShipRoleType.SHIP_ROLE_INVALID
    print "fleetID %d : primary fleet mission type %d: '%s' ; found ship roles %s : %s"%(fleetID,selectedRole,   __AIFleetMissionTypeNames.name(selectedRole),  
                                                                               shipRoles,  [ "%s: %d "%(__AIShipRoleTypeNames.name(rtype),  rnum) for rtype, rnum in  shipRoles.items()] )
    return selectedRole

def assessShipDesignRole(design):
    if design.parts.__contains__("CO_OUTPOST_POD"):
        return AIShipRoleType.SHIP_ROLE_CIVILIAN_OUTPOST
    if design.parts.__contains__("CO_COLONY_POD"):
        return AIShipRoleType.SHIP_ROLE_CIVILIAN_COLONISATION
    if design.parts.__contains__("CO_SUSPEND_ANIM_POD"):
        return AIShipRoleType.SHIP_ROLE_CIVILIAN_COLONISATION
    if design.parts.__contains__("GT_TROOP_POD"):
        return AIShipRoleType.SHIP_ROLE_MILITARY_INVASION
    stats = foAI.foAIstate.getDesignStats(design)
    rating = stats['attack'] * ( stats['structure'] + stats['shields'] )
    if rating < 0.25* ProductionAI.curBestMilShipRating():
        for part in design.parts:
            if "DT_DETECTOR" in part:
                return AIShipRoleType.SHIP_ROLE_CIVILIAN_EXPLORATION
    if rating > 0: #positive attack stat
        return AIShipRoleType.SHIP_ROLE_MILITARY
    else:
        return AIShipRoleType.SHIP_ROLE_CIVILIAN_EXPLORATION  #let this be the default since even without detection part a ship has some inherent
    #return AIShipRoleType.SHIP_ROLE_INVALID

def assessDesignIDStats(designID):
    design = fo.getShipDesign(designID)
    if design is None:
        return  {'attack':0, 'structure':0, 'shields':0}
    else:
        return  {'attack':design.attack, 'structure':design.structure, 'shields':design.shields}

def assessShipRole(shipID):
    "decides which role a ship has"
    ship = fo.getUniverse().getShip(shipID)
    if ship:
        return assessShipDesignRole( fo.getShipDesign(ship.designID) )
    else:
        return AIShipRoleType.SHIP_ROLE_INVALID

def generateAIFleetOrdersForAIFleetMissions():
    "generates fleet orders from targets"
    print("Generating fleet orders")

    print ""
    print "Exploration Fleets : " + str(getEmpireFleetIDsByRole(AIFleetMissionType.FLEET_MISSION_EXPLORATION))
    print "Colonization Fleets: " + str(getEmpireFleetIDsByRole(AIFleetMissionType.FLEET_MISSION_COLONISATION))
    print "Outpost Fleets     : " + str(getEmpireFleetIDsByRole(AIFleetMissionType.FLEET_MISSION_OUTPOST))
    print "Attack Fleets      : " + str(getEmpireFleetIDsByRole(AIFleetMissionType.FLEET_MISSION_ATTACK))
    print "Defend Fleets      : " + str(getEmpireFleetIDsByRole(AIFleetMissionType.FLEET_MISSION_DEFEND))
    print "Invasion Fleets    : " + str(getEmpireFleetIDsByRole(AIFleetMissionType.FLEET_MISSION_INVASION))
    print "Military Fleets    : " + str(getEmpireFleetIDsByRole(AIFleetMissionType.FLEET_MISSION_MILITARY))

    print ""
    print "Explored systems  :"
    printSystems(foAI.foAIstate.getExplorableSystems(AIExplorableSystemType.EXPLORABLE_SYSTEM_EXPLORED))
    print "Unexplored systems:"
    printSystems(foAI.foAIstate.getExplorableSystems(AIExplorableSystemType.EXPLORABLE_SYSTEM_UNEXPLORED))
    print ""

    explorationAIFleetMissions = foAI.foAIstate.getAIFleetMissionsWithAnyMissionTypes([AIFleetMissionType.FLEET_MISSION_EXPLORATION])
    if len( explorationAIFleetMissions) >0:
        print "Exploration targets: "
    else:
        print "Exploration targets:  None"
    for explorationAIFleetMission in explorationAIFleetMissions:
        print "    " + str(explorationAIFleetMission)

    colonisationAIFleetMissions = foAI.foAIstate.getAIFleetMissionsWithAnyMissionTypes([AIFleetMissionType.FLEET_MISSION_COLONISATION])
    if len( colonisationAIFleetMissions) >0:
        print "Colonization targets: "
    else:
        print "Colonization targets:  None"
    for colonisationAIFleetMission in colonisationAIFleetMissions:
        print "    " + str(colonisationAIFleetMission)

    outpostAIFleetMissions = foAI.foAIstate.getAIFleetMissionsWithAnyMissionTypes([AIFleetMissionType.FLEET_MISSION_OUTPOST])
    if len( outpostAIFleetMissions) >0:
        print "Outpost targets: "
    else:
        print "Outpost targets:  None"
    for outpostAIFleetMission in outpostAIFleetMissions:
        print "    " + str(outpostAIFleetMission)

    invasionAIFleetMissions = foAI.foAIstate.getAIFleetMissionsWithAnyMissionTypes([AIFleetMissionType.FLEET_MISSION_INVASION])
    if len( invasionAIFleetMissions) >0:
        print "Invasion targets: "
    else:
        print "Invasion targets:  None"
    for invasionAIFleetMission in invasionAIFleetMissions:
        print "    " + str(invasionAIFleetMission)

    militaryAIFleetMissions = foAI.foAIstate.getAIFleetMissionsWithAnyMissionTypes([AIFleetMissionType.FLEET_MISSION_MILITARY])
    if len( militaryAIFleetMissions) >0:
        print "Military targets: "
    else:
        print "Military targets:  None"
    for militaryAIFleetMission in militaryAIFleetMissions:
        print "    " + str(militaryAIFleetMission)

    aiFleetMissions = foAI.foAIstate.getAllAIFleetMissions()
    for aiFleetMission in aiFleetMissions:
        aiFleetMission.generateAIFleetOrders()

def issueAIFleetOrdersForAIFleetMissions():
    "issues fleet orders"

    print ""
    print "issuing fleet orders:"
    aiFleetMissions = foAI.foAIstate.getAllAIFleetMissions()
    for aiFleetMission in aiFleetMissions:
        fleet = aiFleetMission.getAITarget().getTargetObj()
        if (not fleet) or ( len(fleet.shipIDs)==0):  # in case fleet was merged into another previously during this turn
            continue
        aiFleetMission.issueAIFleetOrders()
    print ""

def printSystems(systemIDs):
    universe = fo.getUniverse()
    empire = fo.getEmpire()
    fleetSupplyableSystemIDs = empire.fleetSupplyableSystemIDs
    for systemID in systemIDs:
        # determine if system is in supplied
        suppliedSystem = ""
        if systemID in fleetSupplyableSystemIDs:
            suppliedSystem = " supplied"

        system = universe.getSystem(systemID)
        if system:
            print "    name:" + system.name + " id:" + str(systemID) + suppliedSystem
        else:
            print "    name:??? id:" + str(systemID) + suppliedSystem

