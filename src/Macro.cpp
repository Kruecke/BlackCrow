#include "Macro.h"
#include "BlackCrow.h"
#include "Util.h"

namespace BlackCrow {

	using namespace BWAPI;
	using namespace Filter;

	Macro::Macro(BlackCrow &parent) : bc(parent) {
		supplyUnderway = 0;
		autoBuildOverlords = false;

		bases = new std::list<BaseInformation*>();
		plannedUnits = new std::list<PlannedUnit*>();
		macroHatcheries = new BWAPI::Unitset();
		reservedLarvae = new BWAPI::Unitset();
	}

	void Macro::onStart() {
		for (const BWEM::Area& area : bc.bwem.Areas()) {
			for (const BWEM::Base& base : area.Bases()) {
				BaseInformation* baseInformation = new BaseInformation();
				baseInformation->hatchery = nullptr;
				baseInformation->underAttack = false;
				baseInformation->extractor = nullptr;
				baseInformation->base = &base;
				baseInformation->isExpanding = false;
				baseInformation->plannedExpansion = nullptr;
				baseInformation->isSafe = true;

				bases->push_back(baseInformation);


				if (base.Starting()) {
					BWAPI::Unit startingHatchery = getStartingHatchery();
					if (base.Location().x == startingHatchery->getTilePosition().x && base.Location().y == startingHatchery->getTilePosition().y) {

						baseInformation->hatchery = startingHatchery;
						baseInformation->workersOnMinerals = Broodwar->getUnitsInRadius(startingHatchery->getPosition(), 350, BWAPI::Filter::IsWorker);
						for (Unit worker : baseInformation->workersOnMinerals) {
							worker->gather(worker->getClosestUnit(IsMineralField));
						}

						firstBase = baseInformation;
					}
				}
			}
		}
	}

	void Macro::update() {

		// Handle Planned Units
		for (PlannedUnit* pu : *plannedUnits) {

			if (pu->planType == PlannedUnit::BUILDING || pu->planType == PlannedUnit::BUILDING_WITH_LOCATION) {
				if (pu->larvaEgg == nullptr && Broodwar->self()->minerals() >= pu->type.mineralPrice()) {

					BWAPI::Unit worker = getDroneForBuilding(pu->requestPosition);
					if (worker) {
						pu->larvaEgg = worker;

						// Get a building location
						if (pu->planType == PlannedUnit::BUILDING && pu->buildLocationChosen == false) {
							// Get a building spot for the building TODO Maybe let the strategy manager decide where to built it?
							if (pu->type == BWAPI::UnitTypes::Zerg_Extractor) {
								pu->extractorTilePosition = (*pu->base->base->Geysers().begin())->TopLeft();
								pu->buildLocation = pu->extractorTilePosition;
							} else {
								//pu->buildLocation = Broodwar->getBuildLocation(pu->type, pu->base->base->Location(), 250, true);
								pu->buildLocation = bc.builder.getBuildingSpot(pu->type, pu->base->base->Location(), false);
							}

							pu->buildLocationChosen = true;
						}

						if (!pu->larvaEgg->build(pu->type, pu->buildLocation)) {
							if (Broodwar->getLastError() != BWAPI::Errors::Unit_Busy) {
								Broodwar << Broodwar->getLastError() << std::endl; // TODO Error Spam
							}
						}
					}
				} else {
					// Assign renegade Extractor to planned unit
					if (pu->type == BWAPI::UnitTypes::Zerg_Extractor && pu->larvaEgg->getType() != BWAPI::UnitTypes::Zerg_Extractor) {
						BWAPI::Unit extractorBuilding = *Broodwar->getUnitsOnTile(pu->extractorTilePosition, IsBuilding).begin(); // TODO Can Nullpointer if no units are found? Change on Unit Complete too
						if (extractorBuilding->getType() == BWAPI::UnitTypes::Zerg_Extractor) { // Instead it would be the Resource_Vespin_Geysir
							pu->larvaEgg = extractorBuilding;;
						}
					}
				}
			}

			if (pu->planType == PlannedUnit::EXPANSION) {
				if (pu->larvaEgg == nullptr) {
					pu->larvaEgg = getDroneForBuilding(pu->base->base->Center());
				}

				if (pu->larvaEgg && !pu->larvaEgg->getType().isBuilding()) {
					if (Util::distance(pu->larvaEgg->getPosition().x, pu->larvaEgg->getPosition().y, pu->base->base->Center().x, pu->base->base->Center().y) <= BWAPI::UnitTypes::Zerg_Drone.sightRange() * 0.8) {
						if (Broodwar->self()->minerals() >= pu->type.mineralPrice()) {
							if (!pu->larvaEgg->build(pu->type, pu->buildLocation)) {
								if (Broodwar->getLastError() != BWAPI::Errors::Unit_Busy) {
									Broodwar << Broodwar->getLastError() << std::endl; // TODO Error Spam
								}
							}
						}
					} else {
						pu->larvaEgg->move(pu->base->base->Center());
					}
				}
			}

			if (pu->planType == PlannedUnit::UNIT) {

				if (pu->larvaEgg != nullptr && !pu->larvaEgg->exists())
					pu->larvaEgg = nullptr;

				// If no larva is being selected or unit is not morphing
				if (pu->larvaEgg == nullptr) {
					pu->larvaEgg = getNonReservedLarva(pu->requestPosition);
				}

				// Check if there are enough ressources to morph
				if (Broodwar->self()->minerals() >= pu->type.mineralPrice()) {
					if (pu->larvaEgg && pu->larvaEgg->getType() == BWAPI::UnitTypes::Zerg_Larva) {
						pu->larvaEgg->train(pu->type);
					}
				}
			}
		}

		updateGasMineralsRatio();
		updateExpansionBuilding();
		updateShiftWorkers();
	}

	void Macro::onUnitDestroyed(Unit unit) {
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Hatchery
			|| unit->getType() == BWAPI::UnitTypes::Zerg_Lair
			|| unit->getType() == BWAPI::UnitTypes::Zerg_Hive) {

			for (BaseInformation* bi : *bases) {
				if (bi->hatchery == unit) {
					bi->hatchery = nullptr;
					return;
				}
			}
			macroHatcheries->erase(unit);
		}

		// Remove Planned Unit
		for (auto it = plannedUnits->begin(); it != plannedUnits->end();) {
			PlannedUnit* p = (*it);

			if (p->larvaEgg && p->larvaEgg->getID() == unit->getID() && p->type != UnitTypes::Zerg_Extractor) {
				it = plannedUnits->erase(it);
			} else
				it++;
		}

		// Remove workers from bases
		for (BaseInformation* bi : *bases) {
			// Minerals
			for (auto it = bi->workersOnMinerals.begin(); it != bi->workersOnMinerals.end();) {
				Unit worker = (*it);
				if (worker->getID() == unit->getID())
					it = bi->workersOnMinerals.erase(it);
				else
					it++;
			}
			// Gas
			for (auto it = bi->workersOnGas.begin(); it != bi->workersOnGas.end();) {
				Unit worker = (*it);
				if (worker->getID() == unit->getID())
					it = bi->workersOnGas.erase(it);
				else
					it++;
			}
		}

		// TODO If its a planned overlord that was being built, remove the incoming supply
		// When clicking a unit, simulate as if it were destroyed. Stop the unit.
	}

	// If the mineral base is set to expand, plan a hatchery
	void Macro::updateExpansionBuilding() {
		for (BaseInformation* base : *bases) {
			if (base->isExpanding && !base->plannedExpansion) {
				base->plannedExpansion = planExpansion(BWAPI::UnitTypes::Zerg_Hatchery, base->base->Center(), base, base->base->Location());
			}
		}
	}

	BWAPI::Unit Macro::getStartingHatchery() {
		for (auto &unit : Broodwar->self()->getUnits()) {
			if (unit->getType().isResourceDepot()) {
				return unit;
			}
		}
		return nullptr;
	}

	std::list<BaseInformation*>* Macro::getAllBaseInformation() {
		return bases;
	}

	BaseInformation* Macro::getStartBaseInformation() {
		return firstBase;
	}

	// Queue up a unit, larva closest to position is chosen, optional base for building placement and drone binding
	PlannedUnit* Macro::plan(BWAPI::UnitType unitType, BWAPI::Position position) {
		PlannedUnit* pu = new PlannedUnit();

		pu->type = unitType;
		pu->larvaEgg = nullptr;
		pu->drone = nullptr;
		pu->base = nullptr;
		pu->extractorTilePosition;
		pu->requestPosition = position;
		pu->buildLocation;
		pu->buildLocationChosen = false;
		pu->isMacroHatchery = false;

		plannedUnits->push_back(pu);

		return pu;
	}

	PlannedUnit* Macro::planUnit(BWAPI::UnitType unitType, BWAPI::Position position) {
		PlannedUnit* pu = plan(unitType, position);
		pu->planType = PlannedUnit::UNIT;

		if (unitType == BWAPI::UnitTypes::Zerg_Overlord)
			supplyUnderway += UnitTypes::Zerg_Overlord.supplyProvided();

		// Check if suppy is needed, add an overlord at the end of the queue
		if (autoBuildOverlords && unitType != BWAPI::UnitTypes::Zerg_Overlord && !unitType.isBuilding()) {
			// Do we need an overlord? If supply is 2 or less after morphing the unit, or more
			if (Broodwar->self()->supplyTotal() - Broodwar->self()->supplyUsed() - unitType.supplyRequired() + supplyUnderway <= 2
				|| ((Broodwar->self()->supplyTotal() > 40 || Broodwar->self()->minerals() > 500) && Broodwar->self()->supplyTotal() - Broodwar->self()->supplyUsed() - unitType.supplyRequired() + supplyUnderway <= 10)) {
				planUnit(BWAPI::UnitTypes::Zerg_Overlord, position); // TODO Get a safe position for the overlords
			}
		}

		return pu;
	}

	PlannedUnit* Macro::planBuilding(BWAPI::UnitType unitType, BWAPI::Position position, BaseInformation* base) {
		PlannedUnit* pu = plan(unitType, position);
		pu->base = base;
		pu->planType = PlannedUnit::BUILDING;

		return pu;
	}

	PlannedUnit* Macro::planBuilding(BWAPI::UnitType unitType, BWAPI::Position position, BaseInformation* base, BWAPI::TilePosition buildLocation) {
		PlannedUnit* pu = plan(unitType, position);
		pu->base = base;
		pu->buildLocation = buildLocation;
		pu->buildLocationChosen = true;
		pu->planType = PlannedUnit::BUILDING_WITH_LOCATION;

		return pu;
	}

	PlannedUnit* Macro::planExpansion(BWAPI::UnitType unitType, BWAPI::Position position, BaseInformation* base, BWAPI::TilePosition buildLocation) {
		PlannedUnit* pu = plan(unitType, position);
		pu->base = base;
		pu->buildLocation = buildLocation;
		pu->buildLocationChosen = true;
		pu->planType = PlannedUnit::EXPANSION;

		return pu;
	}

	int Macro::getReservedMinerals() {
		int minerals = 0;
		for (PlannedUnit* pu : *plannedUnits) {
			if (pu->type.isBuilding()) {
				if (pu->larvaEgg == nullptr || !pu->larvaEgg->getType().isBuilding()) {
					minerals += pu->type.mineralPrice();
				}
			} else {
				if (pu->larvaEgg == nullptr || pu->larvaEgg->getType() == BWAPI::UnitTypes::Zerg_Larva) {
					minerals += pu->type.mineralPrice();
				}
			}
		}
		return minerals;
	}

	int Macro::getUnreservedMinerals() {
		return Broodwar->self()->minerals() - getReservedMinerals();
	}

	void Macro::onUnitComplete(BWAPI::Unit unit) {
		// Are planned units finished?
		std::list<PlannedUnit*>::iterator i = plannedUnits->begin();
		while (i != plannedUnits->end()) {
			PlannedUnit* pu = *i;

			if (unit->getType() == BWAPI::UnitTypes::Zerg_Extractor && pu->type == BWAPI::UnitTypes::Zerg_Extractor) {
				if (unit->getTilePosition().x == pu->extractorTilePosition.x && unit->getTilePosition().y == pu->extractorTilePosition.y) {
					pu->base->extractor = unit;
					i = plannedUnits->erase(i);
					delete pu;
				} else {
					++i;
				}
			} else {
				if (pu->larvaEgg != nullptr && pu->larvaEgg->getID() == unit->getID() && pu->type == unit->getType()) {
					i = plannedUnits->erase(i);

					switch (pu->planType) {
					case PlannedUnit::UNIT:

						// Drone
						if (pu->type == BWAPI::UnitTypes::Zerg_Drone) {

							if (!pu->base) {
								pu->base = findClosestMiningBase(unit->getPosition());

								if (pu->base) {
									auto minerals = pu->base->base->Minerals();
									if (minerals.size() > 0)
										unit->gather(minerals.front()->Unit());
								}
							}

							pu->base->workersOnMinerals.insert(unit);
						}

						// Overlord
						if (pu->type == BWAPI::UnitTypes::Zerg_Overlord) {
							supplyUnderway -= UnitTypes::Zerg_Overlord.supplyProvided();
						}

						break;

					case PlannedUnit::EXPANSION:
						pu->base->hatchery = unit;
						pu->base->isExpanding = false;
						pu->base->plannedExpansion = nullptr;
						break;

					case PlannedUnit::BUILDING:
					case PlannedUnit::BUILDING_WITH_LOCATION:
						if (pu->isMacroHatchery)
							macroHatcheries->insert(pu->larvaEgg);
						break;
					}

					reservedLarvae->erase(pu->larvaEgg);
					delete pu;
				} else {
					++i;
				}
			}
		}
	}

	void Macro::updateGasMineralsRatio() {
		for (BaseInformation* base : *bases) {
			if (base->hatchery != nullptr && base->hatchery->exists()) {

				// Worker Minerals ratio, at least have 20% mineral workers before gas
				int goalMineralWorkers = base->base->Minerals().size();
				std::size_t mineralWorkerThreshold = (std::size_t)(goalMineralWorkers * 0.2); // 20%

				if (base->extractor != nullptr && base->extractor->exists()) {
					while (base->workersOnMinerals.size() > mineralWorkerThreshold && base->workersOnGas.size() < 3) {
						Unit worker = *base->workersOnMinerals.begin();
						base->workersOnMinerals.erase(worker);
						base->workersOnGas.insert(worker);
						worker->gather(base->extractor);
					}
				} else {
					if (base->workersOnGas.size() > 0) { // TODO 1 worker is idle when extractor is being destroyed
						for (auto it = base->workersOnGas.begin(); it != base->workersOnGas.end();) {
							Unit worker = (*it);
							it = base->workersOnGas.erase(it);
							base->workersOnMinerals.insert(worker);
							worker->gather((*base->base->Minerals().begin())->Unit());
						}
					}
				}
			}
		}
	}


	void Macro::updateShiftWorkers() {
		for (BaseInformation* base : *bases) {
			if (base->hatchery != nullptr && base->hatchery->exists()) {

				int dronesAvailable = -dronesCurrentlyNeeded(base);
				if (dronesAvailable > 0) {

					//Broodwar->sendText("Drones available: %i", dronesAvailable);

					// Find a base that needs drones
					for (BaseInformation* base2 : *bases) {
						int dn = dronesCurrentlyNeeded(base2);
						while (dn > 0 && dronesAvailable > 0) {
							BWAPI::Unit worker = *base->workersOnMinerals.begin();
							base->workersOnMinerals.erase(worker);
							base2->workersOnMinerals.insert(worker);

							auto minerals = base2->base->Minerals();
							worker->gather(minerals.front()->Unit()); // TODO Nullpointer, when a base was built and destroyed it will try to mine minerals which are not been seen so getMinerals() gives size 0 back

							dn--;
							dronesAvailable--;
						}
					}
				}
			}
		}
	}


	int Macro::totalDronesNeeded() {
		int totalNumberOfDrones = 0;
		int totalDronesNeeded = 0;

		for (BaseInformation* base : *bases) {
			totalNumberOfDrones += base->workersOnMinerals.size() + base->workersOnGas.size();
			totalDronesNeeded += dronesCurrentlyNeeded(base);
		}

		for (PlannedUnit* pu : *plannedUnits) {
			if (pu->type == BWAPI::UnitTypes::Zerg_Drone) {
				totalDronesNeeded--;
			}
		}

		if (totalNumberOfDrones < bc.config.maxDrones)
			return totalDronesNeeded;
		else
			return 0;
	}

	int Macro::dronesCurrentlyNeeded(BaseInformation* base) {
		if (base->hatchery) {
			int dronesForSaturation = dronesTotalNeeded(base);
			return dronesForSaturation - base->workersOnMinerals.size() - base->workersOnGas.size();
		}
		return 0;
	}

	int Macro::dronesTotalNeeded(BaseInformation* base) {
		return (int)(base->base->Minerals().size() * bc.config.mineralSaturationMultiplier) + base->base->Geysers().size() * 3;
	}

	bool Macro::buildDrone() {
		for (BaseInformation* base : *bases) {
			if (Broodwar->self()->supplyTotal() - Broodwar->self()->supplyUsed() >= 1) {
				planUnit(BWAPI::UnitTypes::Zerg_Drone, base->base->Center());
				return true;
			}
		}

		return false;
	}

	bool Macro::expansionNeeded() {

		bool alreadyExpanding = false;
		int totalNumberOfDrones = 0;
		int totalNumberOfDronesNeeded = 0;

		for (BaseInformation* base : *bases) {
			if (base->hatchery) {
				totalNumberOfDrones += base->workersOnMinerals.size() + base->workersOnGas.size();
				totalNumberOfDronesNeeded += dronesTotalNeeded(base);
			}

			if (base->isExpanding) {
				alreadyExpanding = true;
			}
		}

		//Broodwar << ((float) totalNumberOfDrones / (float) totalNumberOfDronesNeeded) << std::endl;

		if ((float)totalNumberOfDrones / (float)totalNumberOfDronesNeeded >= bc.config.saturationLevel && !alreadyExpanding) //If base saturation is over 80%
			return true;

		return false;
	}

	void Macro::buildExpansion() {
		BaseInformation* expandBase = getClosestExpansionLocation(getStartBaseInformation()->base->Location());
		if (expandBase) {
			assert(expandBase != nullptr); // TODO No Expansions left, grab Island expansions
			expandBase->isExpanding = true;
		}
	}

	BWAPI::Unitset* Macro::getLarvae() {
		BWAPI::Unitset* allLarvae = new BWAPI::Unitset();

		// Base Hatcheries
		for (BaseInformation* base : *bases) {
			if (base->hatchery) {
				BWAPI::Unitset hatchLarva = base->hatchery->getLarva();
				allLarvae->insert(hatchLarva.begin(), hatchLarva.end());
			}
		}

		// Macro Hatcheries
		for (BWAPI::Unit mh : *macroHatcheries) {
			BWAPI::Unitset macroHatchLarva = mh->getLarva();
			allLarvae->insert(macroHatchLarva.begin(), macroHatchLarva.end());
		}

		return allLarvae;
	}

	int Macro::getLarvaeAmount() {
		return getLarvae()->size();
	}

	BWAPI::Unitset* Macro::getNonReservedLarvae() {
		BWAPI::Unitset* allLarvae = getLarvae();
		BWAPI::Unitset* unreservedLarva = new Unitset();

		for (BWAPI::Unit al : *allLarvae) {
			bool isReserved = false;
			for (BWAPI::Unit rl : *reservedLarvae) {
				if (al->getID() == rl->getID())
					isReserved = true;
			}

			if (!isReserved)
				unreservedLarva->insert(al);
		}
		return unreservedLarva;
	}

	int Macro::getNonReservedLarvaeAmount() {
		return getNonReservedLarvae()->size();
	}

	BWAPI::Unit Macro::getNonReservedLarva(BWAPI::Position position) {
		BWAPI::Unitset* larvae = getNonReservedLarvae();
		if (larvae->size() > 0) {
			BWAPI::Unit closestLarva = Util::findClosestUnit(larvae, position);
			reservedLarvae->insert(closestLarva);
			return closestLarva;
		} else {
			return nullptr;
		}
	}

	BWAPI::Unit Macro::getDroneForBuilding(BWAPI::Position position) {
		BaseInformation* base = nullptr;
		base = findClosestMiningBaseWithWorkers(position);

		if (base) {
			BWAPI::Unit worker;
			if (base->workersOnMinerals.size() > 0) {
				worker = *base->workersOnMinerals.begin();
				base->workersOnMinerals.erase(worker);
				return worker;
			} else if (base->workersOnGas.size() > 0) {
				worker = *base->workersOnGas.begin();
				base->workersOnMinerals.erase(worker);
				return worker;
			}
		}
		return nullptr;
	}


	PlannedUnit* Macro::buildMacroHatchery(BaseInformation* base) {
		PlannedUnit* pu = planBuilding(BWAPI::UnitTypes::Zerg_Hatchery, base->base->Center(), base);
		pu->isMacroHatchery = true;
		return pu;
	}

	int Macro::amountMacroHatcheriesBeingBuilt() {
		int macroHatcheriesBeingBuilt = 0;
		for (PlannedUnit* pu : *plannedUnits) {
			if (pu->isMacroHatchery)
				macroHatcheriesBeingBuilt++;
		}
		return macroHatcheriesBeingBuilt;
	}

	int Macro::amountHatcheriesBeingBuilt() {
		int hatcheriesBeingBuilt = 0;
		for (PlannedUnit* pu : *plannedUnits) {
			if (pu->type == BWAPI::UnitTypes::Zerg_Hatchery)
				hatcheriesBeingBuilt++;
		}
		return hatcheriesBeingBuilt;
	}

	// Base that can be actively mined from
	BaseInformation* Macro::findClosestMiningBase(BWAPI::Position position) {
		double closestDistance = 99999999999;
		BaseInformation* closestBase = nullptr;

		for (BaseInformation* base : *bc.macro.bases) {
			if (base->hatchery) {
				double dst = Util::distance(position.x, position.y, base->base->Center().x, base->base->Center().y);
				if (dst < closestDistance) {
					closestDistance = dst;
					closestBase = base;
				}
			}
		}

		return closestBase;
	}

	// Base that has either worker on minerals or gas
	BaseInformation* Macro::findClosestMiningBaseWithWorkers(BWAPI::Position position) {
		double closestDistance = 99999999999;
		BaseInformation* closestBase = nullptr;

		for (BaseInformation* base : *bc.macro.bases) {
			if (base->hatchery && (base->workersOnMinerals.size() > 0 || base->workersOnGas.size() > 0)) {
				double dst = Util::distance(position.x, position.y, base->base->Center().x, base->base->Center().y);
				if (dst < closestDistance) {
					closestDistance = dst;
					closestBase = base;
				}
			}
		}

		return closestBase;
	}

	BaseInformation* Macro::getClosestExpansionLocation(BWAPI::TilePosition position) {
		double closestDistance = 99999999999;
		BaseInformation* closestBase = nullptr;

		for (BaseInformation* base : *bc.macro.bases) {
			if (base->hatchery == nullptr && !base->isExpanding && base->base->GetArea()->AccessibleNeighbours().size() > 0) { // Handle Island Expansion
				double dst = Util::distance(base->base->Location().x, base->base->Location().y, position.x, position.y);
				if (dst < closestDistance) {
					closestDistance = dst;
					closestBase = base;
				}
			}
		}

		return closestBase;
	}
}