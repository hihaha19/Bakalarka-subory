#pragma once

#include "Common.h"

namespace UAlbertaBot
{
class ScoutManager
{
    friend class Global;

    BWAPI::Unit		m_workerScout       = nullptr;
    std::string		m_scoutStatus       = "None";
    std::string		m_gasStealStatus    = "None";
    int				m_numWorkerScouts   = 0;
    int				m_previousScoutHP   = 0;
    bool			m_scoutUnderAttack  = false;
    bool			m_didGasSteal       = false;
    bool			m_gasStealFinished  = false;

    bool enemyWorkerInRadius();
    bool immediateThreat();
    void gasSteal();
    void fleeScout();
    void moveScouts();
    void followPerimeter(int clockwise);
    void followPerimeter(BWAPI::Position to, BWAPI::Position from);
    std::pair<int, int> findSafePath(BWAPI::Position to, BWAPI::Position from);
    int getClosestVertexIndex(BWAPI::UnitInterface* unit);
    int getClosestVertexIndex(BWAPI::Position p);
    void setFrom(BWAPI::Position from);
    void setTo(BWAPI::Position to);
    void drawScoutInformation(int x, int y);
    BWAPI::Position	getFleePosition();
    BWAPI::Unit		getEnemyGeyser();
    BWAPI::Unit		closestEnemyWorker();

    ScoutManager();

public:

    void update();

    void setWorkerScout(BWAPI::Unit unit);
};
}