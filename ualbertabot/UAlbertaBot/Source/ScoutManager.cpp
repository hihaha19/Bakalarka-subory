#include "ScoutManager.h"
#include "ProductionManager.h"
#include "BaseLocationManager.h"
#include "Global.h"
#include "MicroManager.h"
#include "InformationManager.h"
#include "Micro.h"
#include "WorkerManager.h"
#include "MapTools.h"
#include <TransportManager.h>

using namespace UAlbertaBot;

ScoutManager::ScoutManager()
{
}

void ScoutManager::update()
{
    PROFILE_FUNCTION();

    if (!Config::Modules::UsingScoutManager)
    {
        return;
    }

    moveScouts();
    drawScoutInformation(200, 320);
}

void ScoutManager::setWorkerScout(BWAPI::Unit unit)
{
    // if we have a previous worker scout, release it back to the worker manager
    if (m_workerScout)
    {
        Global::Workers().finishedWithWorker(m_workerScout);
    }

    m_workerScout = unit;
    Global::Workers().setScoutWorker(m_workerScout);
}

void ScoutManager::drawScoutInformation(int x, int y)
{
    if (!Config::Debug::DrawScoutInfo)
    {
        return;
    }

    BWAPI::Broodwar->drawTextScreen(x, y, "ScoutInfo: %s", m_scoutStatus.c_str());
    BWAPI::Broodwar->drawTextScreen(x, y+10, "GasSteal: %s", m_gasStealStatus.c_str());
}
void ScoutManager::moveScouts()
{
    if (!m_workerScout || !m_workerScout->exists() || !(m_workerScout->getHitPoints() > 0))
    {
        return;
    }

    // If I didn't finish unloading the troops, wait
    BWAPI::UnitCommand currentCommand(m_workerScout->getLastCommand());
    if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Unload_All
        || currentCommand.getType() == BWAPI::UnitCommandTypes::Unload_All_Position)
        && m_workerScout->getLoadedUnits().size() > 0)
    {
        return;
    }

    if (m_to.isValid() && m_from.isValid())
    {
        followPerimeter(m_to, m_from);
    }
    else
    {
        followPerimeter();
    }
}

void ScoutManager::followPerimeter(int clockwise)
{
    BWAPI::Position goTo = getFleePosition(clockwise);

    if (Config::Debug::DrawUnitTargetInfo)
    {
        BWAPI::Broodwar->drawCircleMap(goTo, 5, BWAPI::Colors::Red, true);
    }

    Micro::SmartMove(m_workerScout, goTo);
}

void ScoutManager::followPerimeter(BWAPI::Position to, BWAPI::Position from)
{
    static int following = 0;
    if (following)
    {
        followPerimeter(following);
        return;
    }

    //assume we're near FROM! 
    if (m_workerScout->getDistance(from) < 50 && m_waypoints.empty())
    {
        //compute waypoints
        std::pair<int, int> wpIDX = findSafePath(to, from);
        bool valid = (wpIDX.first > -1 && wpIDX.second > -1);
        UAB_ASSERT_WARNING(valid, "waypoints not valid! (transport mgr)");
        m_waypoints.push_back(m_mapEdgeVertices[wpIDX.first]);
        m_waypoints.push_back(m_mapEdgeVertices[wpIDX.second]);

        BWAPI::Broodwar->printf("WAYPOINTS: [%d] - [%d]", wpIDX.first, wpIDX.second);

        Micro::SmartMove(m_workerScout, m_waypoints[0]);
    }
    else if (m_waypoints.size() > 1 && m_workerScout->getDistance(m_waypoints[0]) < 100)
    {
        BWAPI::Broodwar->printf("FOLLOW PERIMETER TO SECOND WAYPOINT!");
        //follow perimeter to second waypoint! 
        //clockwise or counterclockwise? 
        int closestPolygonIndex = getClosestVertexIndex(m_transportShip);
        UAB_ASSERT_WARNING(closestPolygonIndex != -1, "Couldn't find a closest vertex");

        if (m_mapEdgeVertices[(closestPolygonIndex + 1) % m_mapEdgeVertices.size()].getApproxDistance(m_waypoints[1]) <
            m_mapEdgeVertices[(closestPolygonIndex - 1) % m_mapEdgeVertices.size()].getApproxDistance(m_waypoints[1]))
        {
            BWAPI::Broodwar->printf("FOLLOW clockwise");
            following = 1;
            followPerimeter(following);
        }
        else
        {
            BWAPI::Broodwar->printf("FOLLOW counter clockwise");
            following = -1;
            followPerimeter(following);
        }

    }
    else if (m_waypoints.size() > 1 && m_workerScout->getDistance(m_waypoints[1]) < 50)
    {
        //if close to second waypoint, go to destination!
        following = 0;
        Micro::SmartMove(m_transportShip, to);
    }
}

std::pair<int, int> ScoutManager::findSafePath(BWAPI::Position to, BWAPI::Position from)
{
    BWAPI::Broodwar->printf("FROM: [%d,%d]", from.x, from.y);
    BWAPI::Broodwar->printf("TO: [%d,%d]", to.x, to.y);

    //closest map edge point to destination
    int endPolygonIndex = getClosestVertexIndex(to);
    //BWAPI::Broodwar->printf("end indx: [%d]", endPolygonIndex);

    UAB_ASSERT_WARNING(endPolygonIndex != -1, "Couldn't find a closest vertex");
    BWAPI::Position enemyEdge = m_mapEdgeVertices[endPolygonIndex];

    auto* enemyBaseLocation = Global::Bases().getPlayerStartingBaseLocation(BWAPI::Broodwar->enemy());
    BWAPI::Position enemyPosition = enemyBaseLocation->getPosition();

    //find the projections on the 4 edges
 //   UAB_ASSERT_WARNING((m_minCorner.isValid() && m_maxCorner.isValid()), "Map corners should have been set! (transport mgr)");
    std::vector<BWAPI::Position> p;
    p.push_back(BWAPI::Position(from.x, m_minCorner.y));
    p.push_back(BWAPI::Position(from.x, m_maxCorner.y));
    p.push_back(BWAPI::Position(m_minCorner.x, from.y));
    p.push_back(BWAPI::Position(m_maxCorner.x, from.y));

    int d1 = p[0].getApproxDistance(enemyPosition);
    int d2 = p[1].getApproxDistance(enemyPosition);
    int d3 = p[2].getApproxDistance(enemyPosition);
    int d4 = p[3].getApproxDistance(enemyPosition);

    //have to choose the two points that are not max or min (the sides!)
    int maxIndex = (d1 > d2 ? d1 : d2) > (d3 > d4 ? d3 : d4) ?
        (d1 > d2 ? 0 : 1) : (d3 > d4 ? 2 : 3);

    int minIndex = (d1 < d2 ? d1 : d2) < (d3 < d4 ? d3 : d4) ?
        (d1 < d2 ? 0 : 1) : (d3 < d4 ? 2 : 3);

    if (maxIndex > minIndex)
    {
        p.erase(p.begin() + maxIndex);
        p.erase(p.begin() + minIndex);
    }
    else
    {
        p.erase(p.begin() + minIndex);
        p.erase(p.begin() + maxIndex);
    }

    //get the one that works best from the two.
    BWAPI::Position waypoint = (enemyEdge.getApproxDistance(p[0]) < enemyEdge.getApproxDistance(p[1])) ? p[0] : p[1];

    int startPolygonIndex = getClosestVertexIndex(waypoint);

    return std::pair<int, int>(startPolygonIndex, endPolygonIndex);

}


int ScoutManager::getClosestVertexIndex(BWAPI::UnitInterface* unit)
{
    int closestIndex = -1;
    double closestDistance = 10000000;

    for (size_t i(0); i < m_mapEdgeVertices.size(); ++i)
    {
        double dist = unit->getDistance(m_mapEdgeVertices[i]);
        if (dist < closestDistance)
        {
            closestDistance = dist;
            closestIndex = i;
        }
    }

    return closestIndex;
}

int ScoutManager::getClosestVertexIndex(BWAPI::Position p)
{
    int closestIndex = -1;
    double closestDistance = 10000000;

    for (size_t i(0); i < m_mapEdgeVertices.size(); ++i)
    {

        double dist = p.getApproxDistance(m_mapEdgeVertices[i]);
        if (dist < closestDistance)
        {
            closestDistance = dist;
            closestIndex = i;
        }
    }

    return closestIndex;
}




void ScoutManager::setFrom(BWAPI::Position from)
{
    if (from.isValid()) { UAlbertaBot::TransportManager::m_from = from; }
}
void ScoutManager::setTo(BWAPI::Position to)
{
    if (to.isValid()) { m_to = to; }
}

void ScoutManager::fleeScout()
{
    const BWAPI::Position fleeTo = getFleePosition();

    if (Config::Debug::DrawScoutInfo)
    {
        BWAPI::Broodwar->drawCircleMap(fleeTo, 5, BWAPI::Colors::Red, true);
    }

    Micro::SmartMove(m_workerScout, fleeTo);
}

void ScoutManager::gasSteal()
{
    if (!Config::Strategy::GasStealWithScout)
    {
        m_gasStealStatus = "Not using gas steal";
        return;
    }

    if (m_didGasSteal)
    {
        return;
    }

    if (!m_workerScout)
    {
        m_gasStealStatus = "No worker scout";
        return;
    }

    auto enemyBaseLocation = Global::Bases().getPlayerStartingBaseLocation(BWAPI::Broodwar->enemy());
    if (!enemyBaseLocation)
    {
        m_gasStealStatus = "No enemy base location found";
        return;
    }

    const BWAPI::Unit enemyGeyser = getEnemyGeyser();
    if (!enemyGeyser)
    {
        m_gasStealStatus = "No enemy geyser found";
        false;
    }

    if (!m_didGasSteal)
    {
        Global::Production().queueGasSteal();
        m_didGasSteal = true;
        Micro::SmartMove(m_workerScout, enemyGeyser->getPosition());
        m_gasStealStatus = "Did Gas Steal";
    }
}

BWAPI::Unit ScoutManager::closestEnemyWorker()
{
    BWAPI::Unit enemyWorker = nullptr;
    BWAPI::Unit geyser      = getEnemyGeyser();
    double      maxDist     = 0;

    for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (unit->getType().isWorker() && unit->isConstructing())
        {
            return unit;
        }
    }

    // for each enemy worker
    for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (unit->getType().isWorker())
        {
            const double dist = unit->getDistance(geyser);

            if (dist < 800 && dist > maxDist)
            {
                maxDist = dist;
                enemyWorker = unit;
            }
        }
    }

    return enemyWorker;
}

BWAPI::Unit ScoutManager::getEnemyGeyser()
{
    BWAPI::Unit geyser = nullptr;
    const auto enemyBaseLocation = Global::Bases().getPlayerStartingBaseLocation(BWAPI::Broodwar->enemy());

    for (auto & unit : enemyBaseLocation->getGeysers())
    {
        geyser = unit;
    }

    return geyser;
}

bool ScoutManager::enemyWorkerInRadius()
{
    for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (unit->getType().isWorker() && (unit->getDistance(m_workerScout) < 300))
        {
            return true;
        }
    }

    return false;
}

bool ScoutManager::immediateThreat()
{
    BWAPI::Unitset enemyAttackingWorkers;
    for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (unit->getType().isWorker() && unit->isAttacking())
        {
            enemyAttackingWorkers.insert(unit);
        }
    }

    if (m_workerScout->isUnderAttack())
    {
        return true;
    }

    for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        const double dist = unit->getDistance(m_workerScout);
        const double range = unit->getType().groundWeapon().maxRange();

        if (unit->getType().canAttack() && !unit->getType().isWorker() && (dist <= range + 32))
        {
            return true;
        }
    }

    return false;
}

BWAPI::Position ScoutManager::getFleePosition()
{
    return BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
}
