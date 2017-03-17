#pragma once
#include <BWAPI.h>
#include "MapData.h"
#include <BWEM/bwem.h>

class BuildingManager {
public:
	BuildingManager();
	~BuildingManager();

	struct AdditionalDimensions {
		int left = 0;
		int right = 0;
		int top = 0;
		int bottom = 0;
	};

	std::list<BWAPI::Position> points;

	void onStart();
	BWAPI::TilePosition getBuildingSpot(BWAPI::UnitType type, BWAPI::Position searchPosition, bool inMineralLine);
	BWAPI::TilePosition getBuildingSpot(BWAPI::UnitType type, BWAPI::TilePosition searchPosition, bool inMineralLine);
	void onBuildingDetected(BWAPI::Unit unit);
	void onBuildingDestroyed(BWAPI::Unit unit);
	bool canBuildTypeAt(BWAPI::UnitType type, int x, int y);

private:
	void setBuildable(int xStart, int xEnd, int yStart, int yEnd, bool buildable);
	void setResourceBuildable(int xStart, int xEnd, int yStart, int yEnd, bool buildable);
	void recheckNearbyBuildings(BWAPI::Unit building, AdditionalDimensions ad);
	AdditionalDimensions getAdditionalDimensions(BWAPI::UnitType buildingType);
};

extern BuildingManager* buildingManager;