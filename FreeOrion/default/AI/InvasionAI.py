import freeOrionAIInterface as fo
import FreeOrionAI as foAI
import AIstate
import AIDependencies
import EnumsAI
import FleetUtilsAI
import PlanetUtilsAI
import AITarget
import math
import ProductionAI
import ColonisationAI
import MilitaryAI

def dictFromMap(map):
    return dict(  [  (el.key(),  el.data() ) for el in map ] )

def getInvasionFleets():
    "get invasion fleets"

    allInvasionFleetIDs = FleetUtilsAI.getEmpireFleetIDsByRole(EnumsAI.AIFleetMissionType.FLEET_MISSION_INVASION)
    AIstate.invasionFleetIDs = FleetUtilsAI.extractFleetIDsWithoutMissionTypes(allInvasionFleetIDs)

    # get supplyable planets
    universe = fo.getUniverse()
    empire = fo.getEmpire()
    empireID = empire.empireID
    capitalID = PlanetUtilsAI.getCapital()
    #capitalID = empire.capitalID
    homeworld=None
    if capitalID:
        homeworld = universe.getPlanet(capitalID)
    if homeworld:
        homeSystemID = homeworld.systemID
    else:
        speciesName = ""
        homeworldName=" no remaining homeworld "
        homeSystemID = -1

    fleetSupplyableSystemIDs = empire.fleetSupplyableSystemIDs
    fleetSupplyablePlanetIDs = PlanetUtilsAI.getPlanetsInSystemsIDs(fleetSupplyableSystemIDs)
    
    primeInvadableSystemIDs = set(ColonisationAI.annexableSystemIDs)
    primeInvadablePlanetIDs = PlanetUtilsAI.getPlanetsInSystemsIDs(primeInvadableSystemIDs)

    # get competitor planets
    exploredSystemIDs = empire.exploredSystemIDs
    exploredPlanetIDs = PlanetUtilsAI.getPlanetsInSystemsIDs(exploredSystemIDs)
    
    visibleSystemIDs = foAI.foAIstate.visInteriorSystemIDs.keys() + foAI.foAIstate. visBorderSystemIDs.keys()
    visiblePlanetIDs = PlanetUtilsAI.getPlanetsInSystemsIDs(visibleSystemIDs)
    accessibleSystemIDs = [sysID for sysID in visibleSystemIDs if  universe.systemsConnected(sysID, homeSystemID, empireID) ]
    acessiblePlanetIDs = PlanetUtilsAI.getPlanetsInSystemsIDs(accessibleSystemIDs)

    #allOwnedPlanetIDs = PlanetUtilsAI.getAllOwnedPlanetIDs(exploredPlanetIDs)
    allOwnedPlanetIDs = PlanetUtilsAI.getAllOwnedPlanetIDs(acessiblePlanetIDs)#need these for unpopulated outposts
    # print "All Owned and Populated PlanetIDs: " + str(allOwnedPlanetIDs)
    
    allPopulatedPlanets=PlanetUtilsAI.getPopulatedPlanetIDs(acessiblePlanetIDs)#need this for natives
    print "All Visible and accessible Populated PlanetIDs (including this empire's):              " + str(PlanetUtilsAI.planetNameIDs(allPopulatedPlanets))

    empireOwnedPlanetIDs = PlanetUtilsAI.getOwnedPlanetsByEmpire(universe.planetIDs, empireID)
    # print "Empire Owned PlanetIDs:            " + str(empireOwnedPlanetIDs)

    invadablePlanetIDs = set(primeInvadablePlanetIDs).intersection(set(allOwnedPlanetIDs).union(allPopulatedPlanets) - set(empireOwnedPlanetIDs))
    print "Prime Invadable PlanetIDs:              " + str(PlanetUtilsAI.planetNameIDs(invadablePlanetIDs))

    print ""
    print "Current Invasion Targeted SystemIDs:       " + str(PlanetUtilsAI.sysNameIDs(AIstate.invasionTargetedSystemIDs))
    invasionTargetedPlanetIDs = getInvasionTargetedPlanetIDs(universe.planetIDs, EnumsAI.AIFleetMissionType.FLEET_MISSION_INVASION, empireID)
    allInvasionTargetedSystemIDs = PlanetUtilsAI.getSystems(invasionTargetedPlanetIDs)
 
    # export invasion targeted systems for other AI modules
    AIstate.invasionTargetedSystemIDs = allInvasionTargetedSystemIDs
    print "Current Invasion Targeted PlanetIDs:       " + str(PlanetUtilsAI.planetNameIDs(invasionTargetedPlanetIDs))

    invasionFleetIDs = FleetUtilsAI.getEmpireFleetIDsByRole(EnumsAI.AIFleetMissionType.FLEET_MISSION_INVASION)
    if not invasionFleetIDs:
        print "Available Invasion Fleets:           0"
    else:
        print "Invasion FleetIDs:                 " + str(FleetUtilsAI.getEmpireFleetIDsByRole(EnumsAI.AIFleetMissionType.FLEET_MISSION_INVASION))
 
    numInvasionFleets = len(FleetUtilsAI.extractFleetIDsWithoutMissionTypes(invasionFleetIDs))
    print "Invasion Fleets Without Missions:    " + str(numInvasionFleets)

    evaluatedPlanetIDs = list(set(invadablePlanetIDs) - set(invasionTargetedPlanetIDs)) #TODO: check if any invasionTargetedPlanetIDs need more troops assigned
    print "Evaluating potential invasions, PlanetIDs:               " + str(evaluatedPlanetIDs)

    evaluatedPlanets = assignInvasionValues(evaluatedPlanetIDs, EnumsAI.AIFleetMissionType.FLEET_MISSION_INVASION, fleetSupplyablePlanetIDs, empire)

    sortedPlanets = [(pid,  pscore,  ptroops) for (pid,  (pscore, ptroops)) in evaluatedPlanets.items() ]
    sortedPlanets.sort(lambda x, y: cmp(x[1], y[1]), reverse=True)
    sortedPlanets = [(pid,  pscore%10000,  ptroops) for (pid,  pscore, ptroops) in sortedPlanets ]

    print ""
    if sortedPlanets:
        print "Invadable planets\nIDs,    ID | Score | Name           | Race             | Troops"
        for pid,  pscore,  ptroops in sortedPlanets:
            planet = universe.getPlanet(pid)
            if planet:
                print "%6d | %6d | %16s | %16s | %d"%(pid,  pscore,  planet.name,  planet.speciesName,  ptroops)
            else:
                print "%6d | %6d | Error: invalid planet ID"%(pid,  pscore)
    else:
        print "No Invadable planets identified"

    sortedPlanets = [(pid,  pscore,  ptroops) for (pid,  pscore, ptroops) in sortedPlanets  if pscore > 0]
    # export opponent planets for other AI modules
    AIstate.opponentPlanetIDs = [pid for pid, pscore, trp in sortedPlanets]
    AIstate.invasionTargets = sortedPlanets

def getInvasionTargetedPlanetIDs(planetIDs, missionType, empireID):
    "return list of being invaded planets"

    universe = fo.getUniverse()
    invasionAIFleetMissions = foAI.foAIstate.getAIFleetMissionsWithAnyMissionTypes([missionType])

    targetedPlanets = []

    for planetID in planetIDs:
        planet = universe.getPlanet(planetID)
        # add planets that are target of a mission
        for invasionAIFleetMission in invasionAIFleetMissions:
            aiTarget = AITarget.AITarget(EnumsAI.AITargetType.TARGET_PLANET, planetID)
            if invasionAIFleetMission.hasTarget(missionType, aiTarget):
                targetedPlanets.append(planetID)

    return targetedPlanets

def assignInvasionValues(planetIDs, missionType, fleetSupplyablePlanetIDs, empire):
    "creates a dictionary that takes planetIDs as key and their invasion score as value"

    planetValues = {}

    for planetID in planetIDs:
        planetValues[planetID] = evaluateInvasionPlanet(planetID, missionType, fleetSupplyablePlanetIDs, empire)

    return planetValues

def evaluateInvasionPlanet(planetID, missionType, fleetSupplyablePlanetIDs, empire):
    "return the invasion value of a planet"
    detail = []
    buildingValues = {"BLD_IMPERIAL_PALACE":                    1000, 
                                            "BLD_CULTURE_ARCHIVES":                 1000, 
                                            "BLD_SHIPYARD_BASE":                        100, 
                                            "BLD_SHIPYARD_ORG_ORB_INC":     200, 
                                            "BLD_SHIPYARD_ORG_XENO_FAC": 200, 
                                            "BLD_SHIPYARD_ORG_CELL_GRO_CHAMB": 200, 
                                            "BLD_SHIPYARD_CON_NANOROBO": 300, 
                                            "BLD_SHIPYARD_CON_GEOINT":      400, 
                                            "BLD_SHIPYARD_CON_ADV_ENGINE": 1000, 
                                            "BLD_SHIPYARD_AST":                             150, 
                                            "BLD_SHIPYARD_AST_REF":                     500, 
                                            "BLD_SHIPYARD_ENRG_COMP":           500, 
                                            "BLD_SHIPYARD_ENRG_SOLAR":          1500, 
                                            "BLD_INDUSTRY_CENTER":                   500, 
                                            "BLD_GAS_GIANT_GEN":                           200, 
                                            "BLD_SOL_ORB_GEN":                              800, 
                                            "BLD_BLACK_HOLE_POW_GEN":       2000, 
                                            "BLD_ENCLAVE_VOID":                             500, 
                                            "BLD_NEUTRONIUM_EXTRACTOR": 2000, 
                                            "BLD_NEUTRONIUM_SYNTH":             2000, 
                                            "BLD_NEUTRONIUM_FORGE":             1000, 
                                            "BLD_CONC_CAMP":                                    100, 
                                            "BLD_BIOTERROR_PROJECTOR":      1000, 
                                            "BLD_SHIPYARD_ENRG_COMP":         3000, 
                                            }
    #TODO: add more factors, as used for colonization
    universe = fo.getUniverse()
    empireID = empire.empireID
    maxJumps=8
    planet = universe.getPlanet(planetID)
    if (planet == None) :  #TODO: exclude planets with stealth higher than empireDetection
        print "invasion AI couldn't get current info on planet %d"%planetID
        return 0, 0

    sysPartialVisTurn = dictFromMap(universe.getVisibilityTurnsMap(planet.systemID,  empireID)).get(fo.visibility.partial, -9999)
    planetPartialVisTurn = dictFromMap(universe.getVisibilityTurnsMap(planetID,  empireID)).get(fo.visibility.partial, -9999)

    if planetPartialVisTurn < sysPartialVisTurn:
            return 0, 0  #last time we had partial vis of the system, the planet was stealthed to us #TODO: track detection strength, order new scouting when it goes up
        
    specName=planet.speciesName
    species=fo.getSpecies(specName)
    if not species: #TODO: iterate over this Empire's available species with which it could colonize after and invasion
        planetEval = ColonisationAI.assignColonisationValues([planetID],  EnumsAI.AIFleetMissionType.FLEET_MISSION_COLONISATION,  [planetID],  species,  empire, detail) #evaluatePlanet is imported from ColonisationAI
        popVal = max( 0.5*planetEval[planetID][0],   ColonisationAI.evaluatePlanet(planetID,  EnumsAI.AIFleetMissionType.FLEET_MISSION_OUTPOST,  [planetID],  species,  empire, detail)  )
    else:
        popVal = ColonisationAI.evaluatePlanet(planetID,  EnumsAI.AIFleetMissionType.FLEET_MISSION_COLONISATION,  [planetID],  species,  empire, detail) #evaluatePlanet is imported from ColonisationAI

    bldTally=0
    for bldType in [universe.getObject(bldg).buildingTypeName for bldg in planet.buildingIDs]:
        bval = buildingValues.get(bldType,  50)
        bldTally += bval
        detail.append("%s: %d"%(bldType,  bval))
        
        capitolID = PlanetUtilsAI.getCapital()
        if capitolID:
            homeworld = universe.getPlanet(capitolID)
            if homeworld:
                homeSystemID = homeworld.systemID
                evalSystemID = planet.systemID
                leastJumpsPath = len(universe.leastJumpsPath(homeSystemID, evalSystemID, empireID))
            maxJumps =  leastJumpsPath
        
    troops = planet.currentMeterValue(fo.meterType.troops)
    maxTroops = planet.currentMeterValue(fo.meterType.maxTroops)
    
    popTSize = planet.currentMeterValue(fo.meterType.targetPopulation)#TODO: adjust for empire tech
    planetSpecials = list(planet.specials)
    pSysID = planet.systemID#TODO: check star value
    
    pmaxShield = planet.currentMeterValue(fo.meterType.maxShield)
    sysFThrt = foAI.foAIstate.systemStatus.get(pSysID, {}).get('fleetThreat', 1000 )
    sysMThrt = foAI.foAIstate.systemStatus.get(pSysID, {}).get('monsterThreat', 0 )
    sysPThrt = foAI.foAIstate.systemStatus.get(pSysID, {}).get('planetThreat', 0 )
    sysTotThrt = sysFThrt + sysMThrt + sysPThrt
    print "invasion eval of %s  %d --- maxShields %.1f -- sysFleetThreat %.1f  -- sysMonsterThreat %.1f"%(planet.name,  planetID,  pmaxShield,  sysFThrt,  sysMThrt)
    supplyVal=0
    enemyVal=0
    if planet.owner!=-1 : #value in taking this away from an enemy
        enemyVal= 20* (planet.currentMeterValue(fo.meterType.targetIndustry) +  2*planet.currentMeterValue(fo.meterType.targetResearch))
    if planetID  in fleetSupplyablePlanetIDs: #TODO: extend to rings
        supplyVal =  100
        if planet.owner== -1:
        #if  (pmaxShield <10):
            if ( sysFThrt < 0.5*ProductionAI.curBestMilShipRating() ):
               if ( sysMThrt < 3*ProductionAI.curBestMilShipRating()):
                    supplyVal = 50
               else:
                    supplyVal = 20
            else:
                supplyVal *= int( min(1, ProductionAI.curBestMilShipRating() /  sysFThrt )  )
    threatFactor = min(1,  0.2*MilitaryAI.totMilRating/(sysTotThrt+0.001))**2  #devalue invasions that would require too much military force
    buildTime=4
    plannedTroops = min(troops+maxJumps+buildTime,  maxTroops)
    if ( empire.getTechStatus("SHP_ORG_HULL") != fo.techStatus.complete ):
        troopCost = math.ceil( plannedTroops/6.0) *  ( 40*( 1+foAI.foAIstate.shipCount * AIDependencies.shipUpkeep ) )
    else:
        troopCost = math.ceil( plannedTroops/6.0) *  ( 20*( 1+foAI.foAIstate.shipCount * AIDependencies.shipUpkeep ) )
    invscore =  threatFactor*max(0,  popVal+supplyVal+bldTally+enemyVal-0.8*troopCost),  plannedTroops
    print invscore, "projected Troop Cost:",  troopCost,  ", threatFactor: ", threatFactor,  ", planet detail ",   detail, "popval,  supplyval,  bldval,  enemyval",   popVal,  supplyVal,  bldTally,  enemyVal
    return   invscore

def getPlanetPopulation(planetID):
    "return planet population"

    universe = fo.getUniverse()

    planet = universe.getPlanet(planetID)
    planetPopulation = planet.currentMeterValue(fo.meterType.population)
 
    if planet == None: return 0
    else:
        return planetPopulation

def sendInvasionFleets(invasionFleetIDs, evaluatedPlanets, missionType):
    "sends a list of invasion fleets to a list of planet_value_pairs"
    universe=fo.getUniverse()
    invasionPool = invasionFleetIDs[:]  #need to make a copy
    bestShip,  bestDesign,  buildChoices = ProductionAI.getBestShipInfo( EnumsAI.AIPriorityType.PRIORITY_PRODUCTION_INVASION)
    if bestDesign:
        troopsPerBestShip = 2*(  list(bestDesign.parts).count("GT_TROOP_POD") )
    else:
        troopsPerBestShip=5 #may actually not have any troopers available, but this num will do for now
        
    #sortedTargets=sorted( [  ( pscore-ptroops/2 ,  pID,  pscore,  ptroops) for pID,  pscore,  ptroops in evaluatedPlanets ] ,  reverse=True)

    invasionPool=set(invasionPool)
    for  pID,  pscore,  ptroops in evaluatedPlanets: # 
        if not invasionPool: return
        planet=universe.getPlanet(pID)
        if not planet: continue
        sysID = planet.systemID
        foundFleets = []
        podsNeeded= int(math.ceil( (ptroops+1.1)/2.0)+0.0001)
        foundStats={}
        minStats= {'rating':0, 'troopPods':podsNeeded}
        targetStats={'rating':10,'troopPods':podsNeeded+2} 
        theseFleets = FleetUtilsAI.getFleetsForMission(1, targetStats , minStats,   foundStats,  "",  systemsToCheck=[sysID],  systemsChecked=[], fleetPoolSet=invasionPool,   fleetList=foundFleets,  verbose=False)
        if theseFleets == []:
            if not FleetUtilsAI.statsMeetReqs(foundStats,  minStats):
                print "Insufficient invasion troop  allocation for system %d ( %s ) -- requested  %s , found %s"%(sysID,  universe.getSystem(sysID).name,  minStats,  foundStats)
                invasionPool.update( foundFleets )
                continue
            else:
                theseFleets = foundFleets
        aiTarget = AITarget.AITarget(EnumsAI.AITargetType.TARGET_PLANET, pID)
        print "assigning invasion fleets %s to target %s"%(theseFleets,  aiTarget)
        for fleetID in theseFleets:
            fleet=universe.getFleet(fleetID)
            aiFleetMission = foAI.foAIstate.getAIFleetMission(fleetID)
            aiFleetMission.clearAIFleetOrders()
            aiFleetMission.clearAITargets( (aiFleetMission.getAIMissionTypes() + [-1])[0] )
            aiFleetMission.addAITarget(missionType, aiTarget)

def assignInvasionFleetsToInvade():
    # assign fleet targets to invadable planets
    invasionFleetIDs = AIstate.invasionFleetIDs

    sendInvasionFleets(invasionFleetIDs, AIstate.invasionTargets, EnumsAI.AIFleetMissionType.FLEET_MISSION_INVASION)
    allInvasionFleetIDs = FleetUtilsAI.getEmpireFleetIDsByRole(EnumsAI.AIFleetMissionType.FLEET_MISSION_INVASION)
    for fid in  FleetUtilsAI.extractFleetIDsWithoutMissionTypes(allInvasionFleetIDs):
        thisMission = foAI.foAIstate.getAIFleetMission(fid)
        thisMission.checkMergers(context="Post-send consolidation of unassigned troops")

