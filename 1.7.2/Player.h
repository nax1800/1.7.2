#pragma once
#include "pch.h"
#include "Utils.h"


class Player {
private:
	DefUHookOg(ServerReadyToStartMatch);
	static void ServerAcknowledgePossession(UObject*, FFrame&);
public:
	DefHookOg(void, GetPlayerViewPoint, APlayerController*, FVector&, FRotator&);
private:
	static void ServerExecuteInventoryItem(UObject*, FFrame&);
	static void ServerReturnToMainMenu(UObject*, FFrame&);

public:
	static int32 PayBuildingRepairCost(AFortPlayerController* Context, ABuildingSMActor* BuildingToRepair);
private:
	DefHookOg(void, ClientOnPawnDied, AFortPlayerControllerAthena*, FFortPlayerDeathReport&);


	InitHooks;
};