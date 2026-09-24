#include "pch.h"
#include "Player.h"
#include "Abilities.h"
#include "Inventory.h"


void Player::ServerReadyToStartMatch(UObject* Context, FFrame& Stack)
{
	Stack.IncrementCode();
	auto PlayerController = (AFortPlayerController*)Context;

	PlayerController->QuickBars = Utils::SpawnActor<AFortQuickBars>(FVector{});
	PlayerController->QuickBars->SetOwner(PlayerController);

	callOG(PlayerController, Stack.CurrentNativeFunction, ServerReadyToStartMatch);
}

void Player::ServerAcknowledgePossession(UObject* Context, FFrame& Stack)
{
	APawn* Pawn;
	Stack.StepCompiledIn(&Pawn);
	Stack.IncrementCode();
	auto PlayerController = (AFortPlayerController*)Context;
	PlayerController->AcknowledgedPawn = Pawn;
}

void Player::GetPlayerViewPoint(APlayerController* PlayerController, FVector& Loc, FRotator& Rot)
{
	static auto SFName = FName(L"Spectating");
	if (PlayerController->StateName == SFName)
	{
		Loc = PlayerController->LastSpectatorSyncLocation;
		Rot = PlayerController->LastSpectatorSyncRotation;
	}
	else if (PlayerController->GetViewTarget())
	{
		Loc = PlayerController->GetViewTarget()->K2_GetActorLocation();
		Rot = PlayerController->GetControlRotation();
	}
	else return GetPlayerViewPointOG(PlayerController, Loc, Rot);
}

void Player::ServerExecuteInventoryItem(UObject* Context, FFrame& Stack)
{
	FGuid ItemGuid;
	Stack.StepCompiledIn(&ItemGuid);
	Stack.IncrementCode();
	auto PlayerController = (AFortPlayerController*)Context;
	if (!PlayerController) return;
	auto entry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry) {
		return entry.ItemGuid == ItemGuid;
		});

	if (!entry || !PlayerController->MyFortPawn) return;
	UFortWeaponItemDefinition* ItemDefinition = entry->ItemDefinition->IsA<UFortGadgetItemDefinition>() ? ((UFortGadgetItemDefinition*)entry->ItemDefinition)->GetDecoItemDefinition() : (UFortWeaponItemDefinition*)entry->ItemDefinition;
	auto Weapon = PlayerController->MyFortPawn->EquipWeaponDefinition(ItemDefinition, ItemGuid);
	if (auto BuildingTool = Weapon->Cast<AFortWeap_BuildingTool>())
	{
		static auto RoofPiece = Utils::FindObject<UFortBuildingItemDefinition>(L"/Game/Items/Weapons/BuildingTools/BuildingItemData_RoofS.BuildingItemData_RoofS");
		static auto FloorPiece = Utils::FindObject<UFortBuildingItemDefinition>(L"/Game/Items/Weapons/BuildingTools/BuildingItemData_Floor.BuildingItemData_Floor");
		static auto WallPiece = Utils::FindObject<UFortBuildingItemDefinition>(L"/Game/Items/Weapons/BuildingTools/BuildingItemData_Wall.BuildingItemData_Wall");
		static auto StairPiece = Utils::FindObject<UFortBuildingItemDefinition>(L"/Game/Items/Weapons/BuildingTools/BuildingItemData_Stair_W.BuildingItemData_Stair_W");


		if (ItemDefinition == RoofPiece)
		{
			static auto RoofMetadata = Utils::FindObject<UBuildingEditModeMetadata>(L"/Game/Building/EditModePatterns/Roof/EMP_Roof_RoofC.EMP_Roof_RoofC");
			BuildingTool->DefaultMetadata = RoofMetadata;
		}
		else if (ItemDefinition == StairPiece)
		{
			static auto StairMetadata = Utils::FindObject<UBuildingEditModeMetadata>(L"/Game/Building/EditModePatterns/Stair/EMP_Stair_StairW.EMP_Stair_StairW");
			BuildingTool->DefaultMetadata = StairMetadata;
		}
		else if (ItemDefinition == WallPiece)
		{
			static auto WallMetadata = Utils::FindObject<UBuildingEditModeMetadata>(L"/Game/Building/EditModePatterns/Wall/EMP_Wall_Solid.EMP_Wall_Solid");
			BuildingTool->DefaultMetadata = WallMetadata;
		}
		else if (ItemDefinition == FloorPiece)
		{
			static auto FloorMetadata = Utils::FindObject<UBuildingEditModeMetadata>(L"/Game/Building/EditModePatterns/Floor/EMP_Floor_Floor.EMP_Floor_Floor");
			BuildingTool->DefaultMetadata = FloorMetadata;
		}

		BuildingTool->OnRep_DefaultMetadata();
	}
}

void Player::ServerReturnToMainMenu(UObject* Context, FFrame& Stack)
{
	Stack.IncrementCode();
	return ((AFortPlayerController*)Context)->ClientReturnToMainMenu(L"");
}

int32 Player::PayBuildingRepairCost(AFortPlayerController* Context, ABuildingSMActor* BuildingToRepair)
{
	if (Context == nullptr || BuildingToRepair == nullptr)
		return -1;

	int32 AmountPaid = 0;
	UFortGameData* GameData = UFortGameData::Get();

	if (Context->bBuildFree == false)
	{
		UFortResourceItemDefinition* ResourceDef = GameData->GetResourceItemDefinition(BuildingToRepair->ResourceType);
		UFortWorldItem* ResourceItem = *Context->WorldInventory->Inventory.ItemInstances.Search([&](UFortWorldItem* entry) {
			return entry->GetItemDefinitionBP() == ResourceDef;
			});

		FFortItemEntry* ItemEntry = Context->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry) {
			return entry.ItemDefinition == ResourceDef;
			});

		if (ResourceItem)
		{
			int32 RepairCost = BuildingToRepair->GetCostToRepair(Context);
			if (RepairCost != -1)
			{
				Context->UpdateSpendingStats(ResourceItem, RepairCost);

				Log(L"RepairCost: %i", RepairCost);
				FGuid ItemGuid = ResourceItem->GetItemGuid();

				ItemEntry->Count -= RepairCost;
				if (ItemEntry->Count <= 0)
					Inventory::Remove(Context, ItemEntry->ItemGuid);

				Inventory::ReplaceEntry((AFortPlayerControllerAthena*)Context, *ItemEntry);

				AmountPaid = RepairCost;
			}
		}
	}

	/*AFortPlayerStateZone* ZonePlayerState = Context->Cast<AFortPlayerStateZone>();
	if (ZonePlayerState)
	{
		const UFortItem* WoodItem = FindExistingItemForDefinition(GameData->GetResourceItemDefinition(EFortResourceType::Wood), false);
		const UFortItem* StoneItem = FindExistingItemForDefinition(GameData->GetResourceItemDefinition(EFortResourceType::Stone), false);
		const UFortItem* MetalItem = FindExistingItemForDefinition(GameData->GetResourceItemDefinition(EFortResourceType::Metal), false);

		FVector BuildingLocation;
		BuildingToRepair->GetCentroid(BuildingLocation);

		// Stat IDs: 19 = Wood, 20 = Stone, 21 = Metal
		if (WoodItem && WoodItem->GetItemGuid() == ItemGuid)
		{
			ZonePlayerState->ClientReportZoneStat(19, RepairCost, RepairCost, BuildingLocation);
		}
		else if (StoneItem && StoneItem->GetItemGuid() == ItemGuid)
		{
			ZonePlayerState->ClientReportZoneStat(20, RepairCost, RepairCost, BuildingLocation);
		}
		else if (MetalItem && MetalItem->GetItemGuid() == ItemGuid)
		{
			ZonePlayerState->ClientReportZoneStat(21, RepairCost, RepairCost, BuildingLocation);
		}
	}*/

	return AmountPaid;
}

void Player::ClientOnPawnDied(AFortPlayerControllerAthena* PlayerController, FFortPlayerDeathReport& DeathReport)
{
	if (!PlayerController)
		return ClientOnPawnDiedOG(PlayerController, DeathReport);
	auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
	auto GameState = (AFortGameStateAthena*)GameMode->GameState;
	auto PlayerState = (AFortPlayerStateAthena*)PlayerController->PlayerState;

	auto KillerPlayerState = (AFortPlayerStateAthena*)DeathReport.KillerPlayerState;
	auto KillerPawn = (AFortPlayerPawnAthena*)DeathReport.KillerPawn;

	if (KillerPlayerState && KillerPawn && KillerPawn->Controller && KillerPawn->Controller->IsA<AFortPlayerControllerAthena>() && KillerPawn->Controller != PlayerController)
	{
		KillerPlayerState->Kills++;
		KillerPlayerState->OnRep_Kills();

		KillerPlayerState->ClientReportKill(L"uhh");

		auto KillerPC = (AFortPlayerControllerAthena*)KillerPlayerState->Owner;
	}


	PlayerState->DeathInfo.bDBNO = PlayerController->MyFortPawn ? PlayerController->MyFortPawn->IsDBNO() : false;
	//PlayerState->DeathInfo.DeathCause = AFortPlayerStateAthena::ToDeathCause(PlayerController->MyFortPawn ? *(FGameplayTagContainer*)(__int64(PlayerController->MyFortPawn) + 0x20a8) : DeathReport.Tags, PlayerState->DeathInfo.bDBNO);
	PlayerState->OnRep_DeathInfo();

	if (PlayerController->MyFortPawn ? !PlayerController->MyFortPawn->IsDBNO() : true)
	{

		PlayerState->Place = GameState->PlayersLeft;
		PlayerState->OnRep_Place();

		AFortWeapon* DamageCauser = nullptr;
		if (auto Weapon = DeathReport.DamageCauser ? DeathReport.DamageCauser->Cast<AFortWeapon>() : nullptr)
			DamageCauser = Weapon;

		//((void (*)(AFortGameModeAthena*, AFortPlayerController*, APlayerState*, AFortPawn*, UFortWeaponItemDefinition*, EDeathCause, char))(ImageBase + 0x606f83c))(GameMode, PlayerController, KillerPlayerState, KillerPawn, DamageCauser ? DamageCauser->WeaponData : nullptr, PlayerState->DeathInfo.DeathCause, 0);

		if (PlayerController->MyFortPawn && ((KillerPlayerState && KillerPlayerState->Place == 1) || PlayerState->Place == 1))
		{
			if (PlayerState->Place == 1)
			{
				KillerPlayerState = PlayerState;
				KillerPawn = (AFortPlayerPawnAthena*)PlayerController->MyFortPawn;
			}
			auto KillerPlayerController = (AFortPlayerControllerAthena*)KillerPlayerState->Owner;
			auto KillerWeapon = DamageCauser ? DamageCauser->WeaponData : nullptr;

			KillerPlayerController->PlayWinEffects();
			KillerPlayerController->ClientNotifyWon();
			KillerPlayerController->ClientNotifyTeamWon();

			GameState->WinningTeam = int32(KillerPlayerState->TeamIndex);
			GameState->OnRep_WinningTeam();
		}
	}

	return ClientOnPawnDiedOG(PlayerController, DeathReport);
}

void Player::Hook()
{
	Utils::ExecHook(L"/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch", ServerReadyToStartMatch, ServerReadyToStartMatchOG);
	Utils::ExecHook(L"/Script/Engine.PlayerController.ServerAcknowledgePossession", ServerAcknowledgePossession);
	Utils::Hook(ImageBase + 0x2320D00, GetPlayerViewPoint, GetPlayerViewPointOG);
	Utils::ExecHook(L"/Script/FortniteGame.FortPlayerController.ServerExecuteInventoryItem", ServerExecuteInventoryItem);
	Utils::ExecHook(L"/Script/FortniteGame.FortPlayerController.ServerReturnToMainMenu", ServerReturnToMainMenu);
	Utils::Hook(ImageBase + 0x9CA190, ClientOnPawnDied, ClientOnPawnDiedOG);

	Utils::Hook(ImageBase + 0x81E2B0, PayBuildingRepairCost);
}
