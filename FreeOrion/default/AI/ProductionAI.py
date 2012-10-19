import freeOrionAIInterface as fo
import FreeOrionAI as foAI
from EnumsAI import AIExplorableSystemType, AIPriorityType, getAIPriorityResourceTypes, getAIPriorityProductionTypes, AIFocusType,  AIEmpireProductionTypes
from EnumsAI import AIShipDesignTypes, AIShipRoleType,  AIFleetMissionType,  AIPriorityNames
import PlanetUtilsAI
import AIstate
import FleetUtilsAI
from random import choice
import sys

shipTypeMap = dict( zip( [AIPriorityType.PRIORITY_PRODUCTION_EXPLORATION,  AIPriorityType.PRIORITY_PRODUCTION_OUTPOST,  AIPriorityType.PRIORITY_PRODUCTION_COLONISATION,  AIPriorityType.PRIORITY_PRODUCTION_INVASION,  AIPriorityType.PRIORITY_PRODUCTION_MILITARY], 
                                            [AIShipDesignTypes.explorationShip,  AIShipDesignTypes.outpostShip,  AIShipDesignTypes.colonyShip,  AIShipDesignTypes.troopShip,  AIShipDesignTypes.attackShip ] ) )

def curBestMilShipRating():
    bestShip,  bestDesign,  buildChoices = getBestShipInfo( AIPriorityType.PRIORITY_PRODUCTION_MILITARY)
    if bestDesign is None:
        return 0.00001  #  empire cannot currently produce any military ships, don't make zero though, to avoid divide-by-zero
    stats = foAI.foAIstate.getDesignIDStats(bestDesign.id)
    return stats['attack'] * ( stats['structure'] + stats['shields'] )

def getBestShipInfo(priority):
    empire = fo.getEmpire()
    theseDesigns = [shipDesign for shipDesign in empire.availableShipDesigns if shipTypeMap.get(priority,  "nomatch")  in fo.getShipDesign(shipDesign).name(False)  and getAvailableBuildLocations(shipDesign) != [] ]
    if theseDesigns == []: 
        return None,  None,  None #must be missing a Shipyard
    ships = [ ( fo.getShipDesign(shipDesign).name(False),  shipDesign) for shipDesign in theseDesigns ]
    bestShip = sorted( ships)[-1][-1]
    buildChoices = getAvailableBuildLocations(bestShip)
    bestDesign=  fo.getShipDesign(bestShip)
    return bestShip,  bestDesign,  buildChoices

def generateProductionOrders():
    "generate production orders"
    empire = fo.getEmpire()
    universe = fo.getUniverse()
    homeworld = universe.getPlanet(PlanetUtilsAI.getCapital())
    print "Production Queue Management:"
    empire = fo.getEmpire()
    totalPP = empire.productionPoints
    print ""
    print "  Total Available Production Points: " + str(totalPP)

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
                print "buildingType  object:",  buildingType
                print "dir(buildingType): ",  dir(buildingType)
                print "    " + str(buildingType.name) # + " cost:" + str(buildingType.productionCost) + " time:" + str(buildingType.productionTime)
                print "'try'ing to print  'cost:' + str(buildingType.productionCost) + ' time:' + str(buildingType.productionTime)"
                try:
                    print " cost:" + str(buildingType.productionCost(empire.empireID,  homeworld.id)) + " time:" + str(buildingType.productionTime(empire.empireID,  homeworld.id))
                except:
                    print"failed with error:",  sys.exc_info()[0]
                
            possibleBuildingTypes = [fo.getBuildingType(buildingTypeID) and  fo.getBuildingType(buildingTypeID).name  for buildingTypeID in possibleBuildingTypeIDs ] #makes sure is not None before getting name

            print ""
            print "Buildings already in Production Queue:"
            productionQueue = empire.productionQueue
            queuedBldgs=[element for element in productionQueue if element.buildType == AIEmpireProductionTypes.BT_BUILDING]
            for bldg in queuedBldgs:
                print "    " + bldg.name + " turns:" + str(bldg.turnsLeft) + " PP:" + str(bldg.allocation)
            if queuedBldgs == []: print "None"
            print
            queuedBldgNames=[ bldg.name for bldg in queuedBldgs ]


            if  ("BLD_INDUSTRY_CENTER" in possibleBuildingTypes) and ("BLD_INDUSTRY_CENTER" not in (capitalBldgs+queuedBldgNames)):
                print "Enqueueing BLD_INDUSTRY_CENTER"
                fo.issueEnqueueBuildingProductionOrder("BLD_INDUSTRY_CENTER", empire.capitalID)

            if  ("BLD_EXOBOT_SHIP" in possibleBuildingTypes) and ("BLD_EXOBOT_SHIP" not in capitalBldgs+queuedBldgNames):
                print "Enqueueing BLD_EXOBOT_SHIP"
                fo.issueEnqueueBuildingProductionOrder("BLD_EXOBOT_SHIP", empire.capitalID)

    totalPPSpent = fo.getEmpire().productionQueue.totalSpent
    print "  Total Production Points Spent:     " + str(totalPPSpent)

    wastedPP = totalPP - totalPPSpent
    print "  Wasted Production Points:          " + str(wastedPP)

    print ""
    print "Possible ship designs to build:"
    for shipDesignID in empire.availableShipDesigns:
        shipDesign = fo.getShipDesign(shipDesignID)
        print "    " + str(shipDesign.name(True)) + " cost:" + str(shipDesign.productionCost) + " time:" + str(shipDesign.productionTime)

    print ""
    print "Projects already in Production Queue:"
    productionQueue = empire.productionQueue
    print "production summary: %s"%[elem.name for elem in productionQueue]
    queuedColonyShips=0
    
    #TODO:  blocked items might not need dequeuing, but rather for supply lines to be un-blockaded 
    anyItemsDequeued=False  
    itemDequeued=True
    lastIndex=0
    while (itemDequeued):# this outside loop used to  deal with indices changeing upon item dequeuing
        itemDequeued=False
        for queue_index  in range(lastIndex,  len(productionQueue)):
            lastIndex=queue_index
            element=productionQueue[queue_index]
            print "    " + element.name + " turns:" + str(element.turnsLeft) + " PP:%.2f"%element.allocation + " being built at " + universe.getObject(element.locationID).name
            if element.turnsLeft == -1:
                print "element %s will never be completed as stands -- deleting from queue"%element.name 
                fo.issueDequeueProductionOrder(queue_index) 
                itemDequeued=True
                anyItemsDequeued=True
                break
            if element.buildType == AIEmpireProductionTypes.BT_SHIP:
                 if foAI.foAIstate.getShipRole(element.designID) ==       AIShipRoleType.SHIP_ROLE_CIVILIAN_COLONISATION:
                     queuedColonyShips +=1
    if queuedColonyShips:
        print "\nFound %d colony ships in build queue"%queuedColonyShips

    if anyItemsDequeued:
        print " "
        print "after dequeuing of broken production entries:"
        totalPPSpent = fo.getEmpire().productionQueue.totalSpent
        wastedPP = totalPP - totalPPSpent
        print "  Total Production Points Spent:   %.1f     ; Wasted Production Points: %.1f"%(totalPPSpent,  wastedPP)

    print ""
    # get the highest production priorities
    productionPriorities = {}
    for priorityType in getAIPriorityProductionTypes():
        productionPriorities[priorityType] = foAI.foAIstate.getPriority(priorityType)

    sortedPriorities = productionPriorities.items()
    sortedPriorities.sort(lambda x,y: cmp(x[1], y[1]), reverse=True)

    topPriority = -1
    topscore = -1
    numColonyTargs=len(AIstate.colonisablePlanetIDs )
    numColonyFleets=len( FleetUtilsAI.getEmpireFleetIDsByRole( AIFleetMissionType.FLEET_MISSION_COLONISATION) )# counting existing colony fleets each as one ship
    totColonyFleets = queuedColonyShips + numColonyFleets
    
    print "Production Queue Priorities:"
    filteredPriorities = {}
    for ID,  score in sortedPriorities:
        if topscore < score:
            topPriority = ID
            topscore = score #don't really need topscore nor sorting with current handling
        print " Score: %4d -- %s "%(score,  AIPriorityNames[ID] )
        if ID != AIPriorityType.PRIORITY_PRODUCTION_BUILDINGS:
            if ( ID != AIPriorityType.PRIORITY_PRODUCTION_COLONISATION ) or (  totColonyFleets <  numColonyTargs ):
                filteredPriorities[ID]= score
    if filteredPriorities == {}:
        print "No non-building-production priorities with nonzero score, setting to default: Military"
        filteredPriorities [AIPriorityType.PRIORITY_PRODUCTION_MILITARY ] =  1 

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
        
    while (wastedPP > 0) and (loopCount <100) and (priorityChoices != [] ): #make sure don't get stuck in some nonbreaking loop like if all shipyards captured
        loopCount +=1
        print "Beginning  build enqueue loop %d; %.1f PP available"%(loopCount,  wastedPP)
        thisPriority = choice( priorityChoices )
        print "selected priority: ",  AIPriorityNames[thisPriority]
        if ( thisPriority ==  AIPriorityType.PRIORITY_PRODUCTION_COLONISATION ):
            if ( totColonyFleets >=  numColonyTargs + 1):
                print "Already sufficient colony ships in queue,  trying next priority choice"
                print ""
                continue
            else:
                totColonyFleets +=1  # assumes the enqueueing below succeeds, but really no harm if assumption proves wrong
        bestShip,  bestDesign,  buildChoices = bestShips[thisPriority]
        loc = choice(buildChoices)
        print "adding new ship to production queue:  %s; per turn production cost %.1f"%(bestDesign.name(True),  (float(bestDesign.productionCost) / bestDesign.productionTime))
        print ""
        fo.issueEnqueueShipProductionOrder(bestShip, loc)
        wastedPP -=  ( float(bestDesign.productionCost) / bestDesign.productionTime)
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
