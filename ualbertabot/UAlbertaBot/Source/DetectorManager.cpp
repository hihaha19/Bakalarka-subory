#include "Common.h"
#include "DetectorManager.h"
#include "Global.h"
#include "Micro.h"
#include "MapTools.h"
#include <BaseLocation.h>
#include <BaseLocationManager.h>
#include "UnitUtil.h"

using namespace UAlbertaBot;

DetectorManager::DetectorManager()
{ 

}

void DetectorManager::executeMicro(const BWAPI::Unitset & targets)
{
    const BWAPI::Unitset& detectorUnits = getUnits();

    if (detectorUnits.empty())
    {
  //      printf("Ziadne detector units\n");
        return;
    }

    for (size_t i(0); i<targets.size(); ++i)
    {
        // do something here if there's targets
    }

    m_cloakedUnitMap.clear();
    BWAPI::Unitset cloakedUnits;

    // figure out targets
    for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        // conditions for targeting
        if (unit->getType() == BWAPI::UnitTypes::Zerg_Lurker ||
            unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar ||
            unit->getType() == BWAPI::UnitTypes::Terran_Wraith)
        {
            cloakedUnits.insert(unit);
            m_cloakedUnitMap[unit] = false;
        }
    }

    bool detectorUnitInBattle = false;

    // for each detectorUnit
    int tentoFramePoslany = 0;
    for (auto& detectorUnit : detectorUnits)
    {
            int sirkaPreSkautovanie = BWAPI::Broodwar->mapWidth() * 32 - 200;
            int vyskaPreSkautovanie = BWAPI::Broodwar->mapHeight() * 32 - 200;

            BWAPI::Position LavyDolny(300, vyskaPreSkautovanie);
            BWAPI::Position PravyDolny(sirkaPreSkautovanie, vyskaPreSkautovanie);
            BWAPI::Position PravyHorny(sirkaPreSkautovanie, 300);
            BWAPI::Position LavyHorny(300, 300);           

            BWAPI::Position LavyDolnyOproti(400, vyskaPreSkautovanie +100);
            BWAPI::Position PravyDolnyOproti(sirkaPreSkautovanie -100, vyskaPreSkautovanie -100);
            BWAPI::Position PravyHornyOproti(sirkaPreSkautovanie -100, 300 - 100);
            BWAPI::Position LavyHornyOproti(400, 300 + 100);           


            BWAPI::Position startLokacia = (BWAPI::Position)BWAPI::Broodwar->self()->getStartLocation();
            

            int hybajuciOverlordi = 0;

            // zistim vysku a sirku mapy v build tiloch, vynasobim ju *32, aby bola v pixeloch
            int sirka = BWAPI::Broodwar->mapWidth() * 32;
            int vyska = BWAPI::Broodwar->mapHeight() * 32;
            if (BWAPI::Broodwar->getFrameCount() > 14000) {     //ak preslo 10 minut
                //ak sa Overlord (detectorUnit) nepohybuje a nenachadza sa v nami definovanom obdlzniku, vygeneruju sa mu nahodne
                // suradnice
                 if (!detectorUnit->isMoving() && (detectorUnit->getLeft() < sirka*0.25 || detectorUnit->getLeft() > sirka*0.75) && (detectorUnit->getBottom() < vyska*0.25 || 
                    detectorUnit->getBottom() > vyska*0.75) ) {
                    
                    int x_suradnica = sirka * 0.25 + (rand() % static_cast<int>(sirka * 0.75 - sirka * 0.25 + 1));
                    int y_suradnica = vyska * 0.25 + (rand() % static_cast<int>(vyska * 0.75 - vyska * 0.25 + 1));
                    Micro::SmartMove(detectorUnit, BWAPI::Position(x_suradnica, y_suradnica));
                }

                 // v opacnom pripade, ked sa nehybe a nachadza sa v obdlzniku, vrati sa na startovaciu lokaciu
                else if(!detectorUnit->isMoving()) {
                    Micro::SmartMove(detectorUnit, startLokacia);
                }
            }          
            


            else {
                for (auto& detectorUnit : detectorUnits)
                {
                    if (detectorUnit->isMoving())
                        hybajuciOverlordi++;
                }


                double vzdialenostLH = detectorUnit->getDistance(LavyHorny);
                double vzdialenostLD = detectorUnit->getDistance(LavyDolny);
                double vzdialenostPD = detectorUnit->getDistance(PravyDolny);
                double vzdialenostPH = detectorUnit->getDistance(PravyHorny);


                 // ak mam menej ako 2880. frame, skontrolujem, ci sa už nejaká jednotka pohybuje, ak nie tak ju pošlem pohybova sa 
                if (BWAPI::Broodwar->getFrameCount() < 2880 || (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Overlord) == 1 &&
                    !detectorUnit->isMoving())) { // && BWAPI::Broodwar->getFrameCount() < 2900) {
                    int uzSaPohybuje = 0;
                    for (auto& unit : detectorUnits)
                    {
                        if (unit->isMoving() && detectorUnit->getID() != unit->getID())
                            uzSaPohybuje = 1;
                    }

                    if (uzSaPohybuje == 0 && !detectorUnit->isMoving()) {
                        if (vzdialenostLD <= vzdialenostLH && vzdialenostLD <= vzdialenostPD && vzdialenostLD <= vzdialenostPH)
                            Micro::SmartMove(detectorUnit, LavyDolny);

                        else if (vzdialenostLH <= vzdialenostLD && vzdialenostLH <= vzdialenostPD && vzdialenostLH <= vzdialenostPH)
                            Micro::SmartMove(detectorUnit, LavyHorny);

                        else if (vzdialenostPD <= vzdialenostLD && vzdialenostPD <= vzdialenostLH && vzdialenostPD <= vzdialenostPH)
                            Micro::SmartMove(detectorUnit, PravyDolny);

                        else if (vzdialenostPH <= vzdialenostLD && vzdialenostPH <= vzdialenostLH && vzdialenostPH <= vzdialenostPD)
                            Micro::SmartMove(detectorUnit, PravyHorny);
                    }
                }

                // ak presli cca 2 minuty od zaciatku zapasu alebo mame minutu, ktora je delitelna 4, napriklad 4., 8. minuta... a v tomto frame sme este neposlali
                // overlorda skautovat a jednotka ktoru prave chceme poslat sa nehybe a zaroven sa nenachadza v ziadnom rohu a mame neparny pocet pohybujucich sa 
                // overlordov a zaroven este nepreslo 10 minut zo zapasu, tak posleme overlorda na skauting (mal by to byt napriklad 2. overlord)
                if ((BWAPI::Broodwar->getFrameCount() == 2880 || BWAPI::Broodwar->getFrameCount() % 5760 == 0) &&
                    !detectorUnit->isMoving() && detectorUnit->getPosition() != LavyDolny && detectorUnit->getPosition() != LavyHorny &&
                    detectorUnit->getPosition() != PravyDolny && detectorUnit->getPosition() != PravyHorny && hybajuciOverlordi % 2 != 0 &&
                    tentoFramePoslany == 0 && BWAPI::Broodwar->getFrameCount() < 14000) {
                    tentoFramePoslany = 1;

                    // zistovanie, ktory roh po spawnuti ma overlord najblizsie
                    if (vzdialenostLD <= vzdialenostLH && vzdialenostLD <= vzdialenostPD && vzdialenostLD <= vzdialenostPH)
                        Micro::SmartMove(detectorUnit, LavyDolnyOproti);

                    else if (vzdialenostLH <= vzdialenostLD && vzdialenostLH <= vzdialenostPD && vzdialenostLH <= vzdialenostPH)
                        Micro::SmartMove(detectorUnit, LavyHornyOproti);

                    else if (vzdialenostPD <= vzdialenostLD && vzdialenostPD <= vzdialenostLH && vzdialenostPD <= vzdialenostPH)
                        Micro::SmartMove(detectorUnit, PravyDolnyOproti);

                    else if (vzdialenostPH <= vzdialenostLD && vzdialenostPH <= vzdialenostLH && vzdialenostPH <= vzdialenostPD)
                        Micro::SmartMove(detectorUnit, PravyHornyOproti);
                }


                else if ((BWAPI::Broodwar->getFrameCount() > 2880 && BWAPI::Broodwar->getFrameCount() % 2880 == 0 && !detectorUnit->isMoving() && detectorUnit->getPosition() != LavyDolny && detectorUnit->getPosition() != LavyHorny &&
                    detectorUnit->getPosition() != PravyDolny && detectorUnit->getPosition() != PravyHorny && hybajuciOverlordi % 2 == 0 && tentoFramePoslany == 0) && BWAPI::Broodwar->getFrameCount() < 14000
                    || (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Overlord) == 1 && !detectorUnit->isMoving())) {
                    tentoFramePoslany = 1;
                    if (vzdialenostLD <= vzdialenostLH && vzdialenostLD <= vzdialenostPD && vzdialenostLD <= vzdialenostPH)
                        Micro::SmartMove(detectorUnit, LavyDolny);

                    else if (vzdialenostLH <= vzdialenostLD && vzdialenostLH <= vzdialenostPD && vzdialenostLH <= vzdialenostPH)
                        Micro::SmartMove(detectorUnit, LavyHorny);

                    else if (vzdialenostPD <= vzdialenostLD && vzdialenostPD <= vzdialenostLH && vzdialenostPD <= vzdialenostPH)
                        Micro::SmartMove(detectorUnit, PravyDolny);

                    else if (vzdialenostPH <= vzdialenostLD && vzdialenostPH <= vzdialenostLH && vzdialenostPH <= vzdialenostPD)
                        Micro::SmartMove(detectorUnit, PravyHorny);
                }


                if (detectorUnit->getDistance(LavyHorny) <= 5)
                    Micro::SmartMove(detectorUnit, LavyDolny);


                else if (detectorUnit->getDistance(LavyDolny) <= 5)
                    Micro::SmartMove(detectorUnit, PravyDolny);

                else if (detectorUnit->getDistance(PravyDolny) <= 5)
                    Micro::SmartMove(detectorUnit, PravyHorny);

                else if (detectorUnit->getDistance(PravyHorny) <= 5)
                    Micro::SmartMove(detectorUnit, LavyHorny);

                else  if (detectorUnit->getDistance(LavyHornyOproti) <= 5)
                    Micro::SmartMove(detectorUnit, PravyHornyOproti);

                else if (detectorUnit->getDistance(LavyDolnyOproti) <= 5)
                    Micro::SmartMove(detectorUnit, LavyHornyOproti);

                else if (detectorUnit->getDistance(PravyDolnyOproti) <= 5)
                    Micro::SmartMove(detectorUnit, LavyDolnyOproti);

                else if (detectorUnit->getDistance(PravyHornyOproti) <= 5)
                    Micro::SmartMove(detectorUnit, PravyDolnyOproti);
            }
            
            //ak bolo na overlorda v poslednych framoch zautocene, vrati sa na startovaciu lokaciu
            if (detectorUnit->isUnderAttack())
                Micro::SmartMove(detectorUnit, startLokacia);
            
    }
}


BWAPI::Unit DetectorManager::closestCloakedUnit(const BWAPI::Unitset & cloakedUnits, BWAPI::Unit detectorUnit)
{
    BWAPI::Unit closestCloaked = nullptr;
    double closestDist = 100000;

    for (auto & unit : cloakedUnits)
    {
        // if we haven't already assigned an detectorUnit to this cloaked unit
        if (!m_cloakedUnitMap[unit])
        {
            double dist = unit->getDistance(detectorUnit);

            if (!closestCloaked || (dist < closestDist))
            {
                closestCloaked = unit;
                closestDist = dist;
            }
        }
    }

    return closestCloaked;
}