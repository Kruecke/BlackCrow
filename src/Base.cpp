#include "BlackCrow.h"
#include "Base.h"
#include "Area.h"
#include "Worker.h"


namespace BlackCrow {

	
	Base::Base(BlackCrow& parent, BWEM::Base& bwemBase, Area& area) : bc(parent), bwemBase(bwemBase), area(area) {
		// Minerals
		for (const BWEM::Mineral* bwemMineral : bwemBase.Minerals()) {
			minerals.emplace_back(bwemMineral);
		}

		// Geysers
		for (const BWEM::Geyser* bwemGeyser : bwemBase.Geysers()) {
			geysers.emplace_back(bwemGeyser);
		}
	}

	void Base::onFrame() {
		// Check if the minerals exist
		for (Mineral& mineral : minerals) {
			if (!mineral.exists()) {
				std::vector<Worker> workers = mineral.workers;
				minerals.erase(std::remove(minerals.begin(), minerals.end(), mineral), minerals.end());
				for (Worker& worker : workers) {
					worker.removeFromResource();
					worker.setMineral(findMineralForWorker());
				}
			}
		}
	}

	void Base::addWorker(Worker worker) {
		worker.setMineral(findMineralForWorker());
	}

	Worker Base::removeWorker() {
		int highestWorkerCount = 0;
		for (Mineral& mineral : minerals) {
			highestWorkerCount = std::max(highestWorkerCount, (int) mineral.workers.size());
		}

		for (Mineral& mineral : minerals) {
			if (mineral.workers.size() == highestWorkerCount) {
				Worker& worker = mineral.workers.back();
				mineral.unregisterWorker(worker);
				worker.removeFromResource();
				return worker;
			}
		}
		
		// TODO Take from Gas

		assert(!"Worker was not found");
		// TODO How to handle when there are no workers?
	}

	Worker Base::removeWorker(BWAPI::Position closestTo) {
		double minDistance = std::numeric_limits < double >::max() ;
		Worker* minWorker = nullptr;


		std::vector<Worker> mineralWorkers = getMineralWorkers();
		if (mineralWorkers.size() > 0) {

			for (Worker& worker : mineralWorkers) {
				double distance = Util::distance(closestTo, worker.unit->getPosition());
				if (!minWorker) {
					minWorker = &worker;
					minDistance = distance;
				} else if (distance < minDistance) {
					minWorker = &worker;
					minDistance = distance;
				}
			}

			minWorker->removeFromResource();
			return *minWorker;
		}

		// TODO Take from Gas

		assert(!"Worker was not found");
		// TODO How to handle when there are no workers?
	}

	bool Base::workerNeeded() {
		return totalMineralWorkers() < bc.config.mineralSaturationMultiplier * minerals.size() || totalGasWorkers() < (int)(geysers.size() * 3);
	}

	int Base::workersNeeded() {
		return (int)(bc.config.mineralSaturationMultiplier * minerals.size() - totalMineralWorkers() + geysers.size() * 3 - totalGasWorkers());
	}

	Mineral& Base::findMineralForWorker() {
		for (unsigned int maxWorkers = 0;; maxWorkers++) {
			for (Mineral& mineral : minerals) {
				if (mineral.workers.size() <= maxWorkers)
					return mineral;
			}
		}
	}

	int Base::totalMineralWorkers() {
		return getMineralWorkers().size();
	}

	int Base::totalGasWorkers() {
		return getGasWorkers().size();
	}

	std::vector<Worker> Base::getMineralWorkers() {
		std::vector<Worker> mineralWorkers;
		for (Mineral& mineral : minerals) {
			mineralWorkers.insert(mineralWorkers.end(), mineral.workers.begin(), mineral.workers.end());
		}
		return mineralWorkers;
	}

	std::vector<Worker> Base::getGasWorkers() {
		std::vector<Worker> gasWorkers;
		for (Geyser& geyser : geysers) {
			gasWorkers.insert(gasWorkers.end(), geyser.workers.begin(), geyser.workers.end());
		}
		return gasWorkers;
	}
}