#include "Common.h"
#include "StrategyManager.h"
#include "UnitUtil.h"
#include "BaseLocationManager.h"
#include "Global.h"
#include "InformationManager.h"
#include "WorkerManager.h"
#include "Logger.h"

using namespace UAlbertaBot;

// constructor
StrategyManager::StrategyManager()
    : m_selfRace(BWAPI::Broodwar->self()->getRace())
    , m_enemyRace(BWAPI::Broodwar->enemy()->getRace())
    , m_emptyBuildOrder(BWAPI::Broodwar->self()->getRace())
{

}



const int StrategyManager::getScore(BWAPI::Player player) const
{
    return player->getBuildingScore() + player->getKillScore() + player->getRazingScore() + player->getUnitScore();
}

const BuildOrder & StrategyManager::getOpeningBookBuildOrder() const
{
    auto buildOrderIt = m_strategies.find(Config::Strategy::StrategyName);

    // look for the build order in the build order map
    if (buildOrderIt != std::end(m_strategies))
    {
        return (*buildOrderIt).second.m_buildOrder;
    }
    else
    {
        UAB_ASSERT_WARNING(false, "Strategy not found: %s, returning empty initial build order", Config::Strategy::StrategyName.c_str());
        return m_emptyBuildOrder;
    }
}


const bool StrategyManager::shouldBuildNow() const
{
    BWAPI::Race myRace = BWAPI::Broodwar->self()->getRace();

    if (myRace == BWAPI::Races::Terran) {
        std::vector<int> buildTimes = { 7, 12, 22, 32, 42, 52 };
        int frame = BWAPI::Broodwar->getFrameCount();
        int minute = frame / (24 * 60);

        int numBunkers = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Bunker);

        for (size_t i(0); i < buildTimes.size(); ++i)
        {
            if (numBunkers < (i + 3) &&  minute > buildTimes[i])
            {
                printf("\nStavaj bunker\n", i);
                return true;
            }
        }

        if (BWAPI::Broodwar->self()->minerals() > 3000)
        {
            printf("Máme viac ako 3000 mineralov, stavaj bunker\n");
            return true;
        }

    }


    if (myRace == BWAPI::Races::Zerg) {
        std::vector<int> buildTimes = { 5 };
        int frame = BWAPI::Broodwar->getFrameCount();
        int minute = frame / (24 * 60);
        
        
        std::vector<int> buildTimesSunken = { 7, 9, 12, 32, 42, 52};

        int numColony = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Creep_Colony) + 
            UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Sunken_Colony);

        size_t numDepots = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hatchery)
            + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair)
            + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive);

       /* for (size_t i(0); i < buildTimesSunken.size(); ++i)
        {
            printf("I je %d\n", i);
            if (numColony < (i + 2) && minute >= buildTimesSunken[i])
            {
                printf("\nStavaj sunken po urcitom case %d\n", i);
                return true;
            }
        }*/
        printf("num depots %d\n", numDepots);
            if (numColony <= (numDepots * 2))
            {
                printf("\nStavaj sunken pri pocte hatchery\n");
                return true;      
        }
    }

    return false;
}

const bool StrategyManager::shouldUpgradeNow() const
{
    std::vector<int> buildTimes = { 4 };
    int frame = BWAPI::Broodwar->getFrameCount();
    int minute = frame / (24 * 60);

    size_t numChamber = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Evolution_Chamber);
    size_t numLair = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair);
    if (numChamber != 0 && numLair == 0) {
        for (auto& unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Zerg_Evolution_Chamber) {
                for (size_t i(0); i < buildTimes.size(); ++i)
                {
                    unit->upgrade(BWAPI::UpgradeTypes::Zerg_Missile_Attacks);
                    printf("Missile attacks\n");
               //     printf("Gas price factor %d\n", BWAPI::UpgradeTypes::Zerg_Missile_Attacks.gasPriceFactor());
                 //   printf("Gas price  %d\n", BWAPI::UpgradeTypes::Zerg_Missile_Attacks.gasPrice());
                }


            }

        }

    }

    if (numLair != 0) {
        for (auto& unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Zerg_Evolution_Chamber) {
                unit->upgrade(BWAPI::UpgradeTypes::Zerg_Missile_Attacks);
          //      printf("Gas price factor %d\n", BWAPI::UpgradeTypes::Zerg_Missile_Attacks.gasPriceFactor());
            //    printf("Gas price  %d\n", BWAPI::UpgradeTypes::Zerg_Missile_Attacks.gasPrice());
            }
        }
    }
}

const bool StrategyManager::shouldExpandNow() const
{
    // if there is no place to expand to, we can't expand
    if (Global::Bases().getNextExpansion(BWAPI::Broodwar->self()) == BWAPI::TilePositions::None)
    {
        BWAPI::Broodwar->printf("No valid expansion location");
        return false;
    }

    size_t numDepots    = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Command_Center)
        + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Nexus)
        + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hatchery)
        + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair)
        + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive);
    int frame           = BWAPI::Broodwar->getFrameCount();
    int minute          = frame / (24*60);

    // if we have a ton of idle workers then we need a new expansion
    if (Global::Workers().getNumIdleWorkers() > 10)
    {
        printf("viac ako 10 necinnych workerov\n");
        return true;
    }

    // if we have a ridiculous stockpile of minerals, expand
    if (BWAPI::Broodwar->self()->minerals() > 3000)
    {   
        printf("Máme viac ako 3000 mineralov\n");
        return true;
    }

    // we will make expansion N after array[N] minutes have passed
    std::vector<int> expansionTimes ={5, 10, 15, 18, 20 , 50};
                                 // pokial i < 6
    for (size_t i(0); i < expansionTimes.size(); ++i)
    {
        if (numDepots < (i+2) && minute > expansionTimes[i])
        {
            printf("\nPresli %d minuty\n", i);
            return true;
        }
    }

    return false;
}

void StrategyManager::addStrategy(const std::string & name, Strategy & strategy)
{
    m_strategies[name] = strategy;
}

const MetaPairVector StrategyManager::getBuildOrderGoal()
{
    BWAPI::Race myRace = BWAPI::Broodwar->self()->getRace();

    if (myRace == BWAPI::Races::Protoss)
    {
        return getProtossBuildOrderGoal();
    }
    else if (myRace == BWAPI::Races::Terran)
    {
        return getTerranBuildOrderGoal();
    }
    else if (myRace == BWAPI::Races::Zerg)
    {
        return getZergBuildOrderGoal();
    }

    return MetaPairVector();
}

const MetaPairVector StrategyManager::getProtossBuildOrderGoal() const
{
    // the goal to return
    MetaPairVector goal;

    int numZealots          = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Zealot);
    int numPylons           = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Pylon);
    int numDragoons         = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Dragoon);
    int numProbes           = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Probe);
    int numNexusCompleted   = BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
    int numNexusAll         = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
    int numCyber            = BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
    int numCannon           = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Photon_Cannon);
    int numScout            = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Scout);
    int numReaver           = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Reaver);
    int numDarkTeplar       = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar);
    int numGateway          = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Gateway);
    int numCorsair          = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Corsair);
    int numShuttle = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Shuttle);

    if (Config::Strategy::StrategyName == "Protoss_ZealotRush")
    {
        goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Zealot, numZealots + 8));

        // once we have a 2nd nexus start making dragoons
        if (numNexusAll >= 2)
        {
            goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dragoon, numDragoons + 4));
        }
    }
    else if (Config::Strategy::StrategyName == "Protoss_Corsair")
    {
        if(numGateway > 0)
            goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Zealot, numZealots + 3));
        if(numCorsair <= 6)
            goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Corsair, numCorsair + 1));
    }

    else if (Config::Strategy::StrategyName == "Protoss_DragoonRush")
    {
        goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dragoon, numDragoons + 6));
    }
    else if (Config::Strategy::StrategyName == "Protoss_Drop")
    {
        if(numShuttle == 0)
            goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Shuttle, numShuttle+1));
        
        if (numZealots == 0)
        {
            goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Zealot, numZealots + 4));
            
        }
        else
        {
           // goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dark_Templar, 5));
            goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Zealot, numZealots + 8));
        }
    }
    else if (Config::Strategy::StrategyName == "Protoss_DTRush")
    {
        goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dark_Templar, numDarkTeplar + 2));

        // if we have a 2nd nexus then get some goons out
        if (numNexusAll >= 2)
        {
            goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dragoon, numDragoons + 4));
        }
    }
    else
    {
        UAB_ASSERT_WARNING(false, "Unknown Protoss Strategy Name: %s", Config::Strategy::StrategyName.c_str());
    }

    // if we have 3 nexus, make an observer
    if (numNexusCompleted >= 3)
    {
        goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observer, 1));
    }


    // add observer to the goal if the enemy has cloaked units 
    if (Global::Info().enemyHasCloakedUnits())
    {
        goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Robotics_Facility, 1));

        if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Facility) > 0)
        {
            goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observatory, 1));
        }
        if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Observatory) > 0)
        {
            goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observer, 1));
        }
    }

    // if we want to expand, insert a nexus into the build order
    if (shouldExpandNow())
    {
        goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Nexus, numNexusAll + 1));
        goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Photon_Cannon, numCannon + 1));
    }

    return goal;
}

const MetaPairVector StrategyManager::getTerranBuildOrderGoal() const
{
    // the goal to return
    std::vector<MetaPair> goal;

    int numWorkers      = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_SCV);
    int numCC           = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Command_Center);
    int numMarines      = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Marine);
    int numMedics       = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Medic);
    int numWraith       = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Wraith);
    int numVultures     = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Vulture);
    int numGoliath      = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Goliath);
    int numTanks        = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
        + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode);
    int numBay          = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Engineering_Bay);
    int numAcademy = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Academy);
    int numBunker = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Bunker);


    if (Config::Strategy::StrategyName == "Terran_Defense")
    {   //na koniec goal sa vyrobi +8 marines
        printf("Boss\n");
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Marine, 4));
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Firebat, 7));
        if(numMedics < 3)
            goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Medic, numMedics+1));
    }

    else if (Config::Strategy::StrategyName == "Terran_MarineRush")
    {   //na koniec goal sa vyrobi +8 marines
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Marine, numMarines + 8));
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Refinery, 1));
        if (numMarines > 5)
        {   // ak mam viac ako 5 marinakov, vyrobi sa engineering_bay
            goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Engineering_Bay, 1));
            
        }
    }

    else if (Config::Strategy::StrategyName == "Terran_4RaxMarines")
    {   // +8 marines
     //   goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Marine, numMarines + 8));
        if(numAcademy == 0)
            goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Academy, 1));


    }
    else if (Config::Strategy::StrategyName == "Terran_VultureRush")
    {   // +8 vultures
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Vulture, numVultures + 8));

        if (numVultures > 8)
        {   // ak mam viac ako 8 vultures, urob tank siege mode a 4 tanky
            goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Tank_Siege_Mode, 1));
            goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, 4));
        }
    }
    else if (Config::Strategy::StrategyName == "Terran_TankPush")
    {   // vyrob 6 tankov, +6 goliatov, potom tank siege mode
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, 6));
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Goliath, numGoliath + 6));
        goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Tank_Siege_Mode, 1));
    }
    else
    {
        BWAPI::Broodwar->printf("Warning: No build order goal for Terran Strategy: %s", Config::Strategy::StrategyName.c_str());
    }



    if (shouldExpandNow())
    {   // ak expanduje, vyrobi terran command center a +10 workerov
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Command_Center, numCC + 1));
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_SCV, numWorkers + 10));
        //nove
     //   goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Bunker, numBunker + 1));
    }

    if (shouldBuildNow())
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Bunker, numBunker + 1));

    return goal;
}

const MetaPairVector StrategyManager::getZergBuildOrderGoal() const
{
    // the goal to return
    std::vector<MetaPair> goal;

    int numWorkers      = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Drone);
    int numCC           = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hatchery)
        + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair)
        + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive);
    int numMutas        = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Mutalisk);
    int numDrones       = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Drone);
    int zerglings       = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Zergling);
    int numHydras       = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk);
    int numScourge      = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Scourge);
    int numGuardians    = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Guardian);
    int numSunken = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Sunken_Colony);
    int numCreep = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Creep_Colony);
    int numEvolutionChamber = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Evolution_Chamber);
    int numExtractor = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Extractor);
    int numOverlord = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Overlord);

    int mutasWanted = numMutas + 6;
    int hydrasWanted = numHydras + 6;

    if (Config::Strategy::StrategyName == "Zerg_ZerglingRush")
    {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Zergling, zerglings + 6));
       
        if (numDrones < 5)
            goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Drone, numDrones + 1));
        if(numExtractor == 0)
            goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Extractor, numExtractor+1));

    }

    else if (Config::Strategy::StrategyName == "Zerg_2HatchHydra")
    {
        int frame = BWAPI::Broodwar->getFrameCount();
        int minute = frame / (24 * 60);

      //  if (minute <= 25) {
            goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Hydralisk, numHydras + 15));
            //   goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Grooved_Spines, 1));
              // printf("Drones %d\n", numDrones);
            if (numHydras > 20) {
                goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Drone, numDrones + 4));
            }

            if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spire) > 0)
                goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Mutalisk, 30));

            if (BWAPI::Broodwar->self()->minerals() > 400 && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk_Den > 0))
                goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Muscular_Augments, 1));

            if (numDrones == 0) {
                goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Drone, numDrones + 6));
            }


            // co najviac hatchery a hydier, minanie zdrojov zabezpecit hatchery
            if (BWAPI::Broodwar->self()->minerals() > 600)
                goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Hatchery, numCC + 1));

            if (BWAPI::Broodwar->self()->minerals() > 200)
                goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Spire, 1));

            if (BWAPI::Broodwar->self()->minerals() > 1000 && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Queens_Nest == 0))
                goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Queens_Nest, 1));

            if (numHydras > 30)
                goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Zergling, zerglings + 10));


            if (zerglings > 5)
                goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Metabolic_Boost, 1));

            if (numEvolutionChamber == 0 && BWAPI::Broodwar->self()->minerals() > 500 && numHydras > 15)
                goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Evolution_Chamber, 2));

            if (numEvolutionChamber == 1)
                goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Evolution_Chamber, 2));

            if (numEvolutionChamber != 0 && BWAPI::Broodwar->self()->minerals() > 500)
                goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Lair, 1));

            if (BWAPI::Broodwar->self()->minerals() > 400) {
                goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Creep_Colony, numCreep + 1));
                //  printf("Stavaj sunken\n");
            }
        }       
  //  }

    else if (Config::Strategy::StrategyName == "Zerg_3HatchMuta")
    {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Hydralisk, numHydras + 12));
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Drone, numDrones + 4));
    }
    else if (Config::Strategy::StrategyName == "Zerg_3HatchScourge")
    {
        if (numScourge > 40)
        {
            goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Hydralisk, numHydras + 12));
        }
        else
        {
            goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Scourge, numScourge + 12));
        }


        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Drone, numDrones + 4));
    }

    if (shouldExpandNow())
    {
        printf("Expandujem\n");
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Hatchery, numCC + 1));
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Drone, numWorkers + 10));
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Extractor, numExtractor + 1));
    }

 //   if (shouldBuildNow()) { 
   //     goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Creep_Colony, numCreep + 2));
    //}
       

    shouldUpgradeNow();

    return goal;
}

void StrategyManager::readResults()
{
    if (!Config::Modules::UsingStrategyIO)
    {
        return;
    }

    std::string enemyName = BWAPI::Broodwar->enemy()->getName();
    std::replace(enemyName.begin(), enemyName.end(), ' ', '_');

    std::string enemyResultsFile = Config::Strategy::ReadDir + enemyName + ".txt";

    std::string strategyName;
    int wins = 0;
    int losses = 0;

    FILE *file = fopen (enemyResultsFile.c_str(), "r");
    if (file != nullptr)
    {
        char line[4096]; /* or other suitable maximum line size */
        while (fgets (line, sizeof line, file) != nullptr) /* read a line */
        {
            std::stringstream ss(line);

            ss >> strategyName;
            ss >> wins;
            ss >> losses;

            //BWAPI::Broodwar->printf("Results Found: %s %d %d", strategyName.c_str(), wins, losses);

            if (m_strategies.find(strategyName) == m_strategies.end())
            {
                //BWAPI::Broodwar->printf("Warning: Results file has unknown Strategy: %s", strategyName.c_str());
            }
            else
            {
                m_strategies[strategyName].m_wins = wins;
                m_strategies[strategyName].m_losses = losses;
            }
        }

        fclose (file);
    }
    else
    {
        //BWAPI::Broodwar->printf("No results file found: %s", enemyResultsFile.c_str());
    }
}

void StrategyManager::writeResults()
{
    if (!Config::Modules::UsingStrategyIO)
    {
        return;
    }

    std::string enemyName = BWAPI::Broodwar->enemy()->getName();
    std::replace(enemyName.begin(), enemyName.end(), ' ', '_');

    std::string enemyResultsFile = Config::Strategy::WriteDir + enemyName + ".txt";

    std::stringstream ss;

    for (auto & kv : m_strategies)
    {
        const Strategy & strategy = kv.second;

        ss << strategy.m_name << " " << strategy.m_wins << " " << strategy.m_losses << "\n";
    }

    Logger::LogOverwriteToFile(enemyResultsFile, ss.str());
}

void StrategyManager::onEnd(const bool isWinner)
{
    if (!Config::Modules::UsingStrategyIO)
    {
        return;
    }

    if (isWinner)
    {
        m_strategies[Config::Strategy::StrategyName].m_wins++;
    }
    else
    {
        m_strategies[Config::Strategy::StrategyName].m_losses++;
    }

    writeResults();
}

void StrategyManager::setLearnedStrategy()
{
    // we are currently not using this functionality for the competition so turn it off 
    return;

    if (!Config::Modules::UsingStrategyIO)
    {
        return;
    }

    const std::string & strategyName = Config::Strategy::StrategyName;
    Strategy & currentStrategy = m_strategies[strategyName];

    int totalGamesPlayed = 0;
    int strategyGamesPlayed = currentStrategy.m_wins + currentStrategy.m_losses;
    double winRate = strategyGamesPlayed > 0 ? currentStrategy.m_wins / static_cast<double>(strategyGamesPlayed) : 0;

    // if we are using an enemy specific strategy
    if (Config::Strategy::FoundEnemySpecificStrategy)
    {
        return;
    }

    // if our win rate with the current strategy is super high don't explore at all
    // also we're pretty confident in our base strategies so don't change if insufficient games have been played
    if (strategyGamesPlayed < 5 || (strategyGamesPlayed > 0 && winRate > 0.49))
    {
        BWAPI::Broodwar->printf("Still using default strategy");
        return;
    }

    // get the total number of games played so far with this race
    for (auto & kv : m_strategies)
    {
        Strategy & strategy = kv.second;
        if (strategy.m_race == BWAPI::Broodwar->self()->getRace())
        {
            totalGamesPlayed += strategy.m_wins + strategy.m_losses;
        }
    }

    // calculate the UCB value and store the highest
    double C = 0.5;
    std::string bestUCBStrategy;
    double bestUCBStrategyVal = std::numeric_limits<double>::lowest();
    for (auto & kv : m_strategies)
    {
        Strategy & strategy = kv.second;
        if (strategy.m_race != BWAPI::Broodwar->self()->getRace())
        {
            continue;
        }

        int sGamesPlayed = strategy.m_wins + strategy.m_losses;
        double sWinRate = sGamesPlayed > 0 ? currentStrategy.m_wins / static_cast<double>(strategyGamesPlayed) : 0;
        double ucbVal = C * sqrt(log((double)totalGamesPlayed / sGamesPlayed));
        double val = sWinRate + ucbVal;

        if (val > bestUCBStrategyVal)
        {
            bestUCBStrategy = strategy.m_name;
            bestUCBStrategyVal = val;
        }
    }

    Config::Strategy::StrategyName = bestUCBStrategy;
}