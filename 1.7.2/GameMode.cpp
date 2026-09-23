#include "pch.h"
#include "GameMode.h"
#include "Misc.h"
#include "Abilities.h"
#include "Player.h"
#include "Inventory.h"

void SetPlaylist(AFortGameModeAthena* GameMode) 
{
	AFortGameStateAthena* GameState = GameMode->GameState->Cast<AFortGameStateAthena>();

	GameState->CurrentPlaylistId = GameMode->CurrentPlaylistId = 1;

	GameMode->bAlwaysDBNO = false;
}

bool bReady = false;
void GameMode::ReadyToStartMatch(UObject* Context, FFrame& Stack, bool* Ret) 
{
	Stack.IncrementCode();
	AFortGameModeAthena* GameMode = Context->Cast<AFortGameModeAthena>();
	if (GameMode == nullptr) 
	{
		*Ret = callOGWithRet(((AGameMode*)Context), Stack.CurrentNativeFunction, ReadyToStartMatch);
		return;
	}
	AFortGameStateAthena* GameState = GameMode->GameState->Cast<AFortGameStateAthena>();
	if (GameMode->WarmupRequiredPlayerCount != 1)
	{
		GameMode->WarmupRequiredPlayerCount = 1;

		SetPlaylist(GameMode);
	}

	if (!GameMode->bWorldIsReady) 
	{
		TArray<AFortPlayerStartWarmup*> Starts = Utils::GetAll<AFortPlayerStartWarmup>();
		int32 StartsNum = Starts.Num();
		Starts.Free();
		if (StartsNum == 0 || GameState->MapInfo == nullptr) 
		{
			*Ret = false;
			return;
		}

		AbilitySets.Add(Utils::FindObject<UFortAbilitySet>(L"/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_DefaultPlayer.GAS_DefaultPlayer"));

		GameMode->DefaultPawnClass = Utils::FindObject<UClass>(L"/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C");
		Utils::Patch<uint8>(ImageBase + 0x137DBC0, 0xc3);
		SetConsoleTitleA("1.7.2: Ready || Credits to @plooshi");
		GameMode->bWorldIsReady = true;
	}

	*Ret = callOGWithRet(((AGameMode*)Context), Stack.CurrentNativeFunction, ReadyToStartMatch);
}

APawn* GameMode::SpawnDefaultPawnFor(UObject* Context, FFrame& Stack, APawn** Ret) 
{
	AController* NewPlayer;
	AActor* StartSpot;
	Stack.StepCompiledIn(&NewPlayer);
	Stack.StepCompiledIn(&StartSpot);
	Stack.IncrementCode();
	AFortGameModeAthena* GameMode = Context->Cast<AFortGameModeAthena>();
	FTransform Transform = StartSpot->GetTransform();
	APawn* Pawn = GameMode->SpawnDefaultPawnAtTransform(NewPlayer, Transform);

	AFortPlayerControllerAthena* PlayerController = NewPlayer->Cast<AFortPlayerControllerAthena>();
	if (PlayerController == nullptr) 
		return *Ret = Pawn;

	int32 Num = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Num();
	if (Num != 0) 
	{
		bool bStarting = false;
		for (FFortItemEntry& Entry : PlayerController->WorldInventory->Inventory.ReplicatedEntries)
		{
			for (FItemAndCount& StartingItem : GameMode->StartingItems)
			{
				if (StartingItem.Item == Entry.ItemDefinition)
					bStarting = true;
			}
			
			if (!bStarting)
				Inventory::Remove(PlayerController, Entry.ItemGuid);
		}
	}
	else 
	{
		Inventory::GiveItem(PlayerController, Utils::FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Harvest_Pickaxe_Athena_C_T01.WID_Harvest_Pickaxe_Athena_C_T01"));
		for (FItemAndCount& StartingItem : GameMode->StartingItems)
		{
			if (StartingItem.Count)
				Inventory::GiveItem(PlayerController, StartingItem.Item, StartingItem.Count);
		}
	}


	if (Num == 0)
	{
		AFortPlayerStateAthena* PlayerState = PlayerController->PlayerState->Cast<AFortPlayerStateAthena>();

		for (UFortAbilitySet* AbilitySet : AbilitySets)
		{
			Abilities::GiveAbilitySet(PlayerState->AbilitySystemComponent, AbilitySet);
		}

		TArray<UFortHeroType*> HeroTypes;
		for (int i = 0; i < UObject::GObjects->Num(); i++)
		{
			UFortHeroType* HeroType = UObject::GObjects->GetByIndex(i)->Cast<UFortHeroType>();
			if (UKismetSystemLibrary::GetPathName(HeroType).ToString().starts_with("/Game/Athena/Heroes/"))
				HeroTypes.Add(HeroType);
		}

		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<std::size_t> dist(0, HeroTypes.Num() - 1);

		for (UFortHeroSpecialization* Spec : HeroTypes[int32(dist(gen))]->Specializations)
		{
			for (UCustomCharacterPart* Part : Spec->CharacterParts)
			{
				PlayerState->CharacterParts[(int) Part->CharacterPartType] = Part;
			}
		}

		((void (*)(APlayerState*, APawn*)) (ImageBase + 0x217DB10))(PlayerController->PlayerState, Pawn);
	}

	return *Ret = Pawn;
}

EFortTeam GameMode::PickTeam(AFortGameModeAthena* GameMode, uint8_t PreferredTeam, AFortPlayerControllerAthena* Controller) 
{
	uint8_t ret = CurrentTeam;

	if (++PlayersOnCurTeam >= 1) 
	{
		CurrentTeam++;
		PlayersOnCurTeam = 0;
	}

	return EFortTeam(ret);
}

UClass** GetGameSessionClass(AFortGameMode*, UClass** OutClass) 
{
	*OutClass = AFortGameSessionDedicated::StaticClass();
	return OutClass;
}


void GameMode::HandleStartingNewPlayer(UObject* Context, FFrame& Stack) 
{
	AFortPlayerControllerAthena* NewPlayer;
	Stack.StepCompiledIn(&NewPlayer);
	Stack.IncrementCode();

	AFortGameModeAthena* GameMode = Context->Cast<AFortGameModeAthena>();
	AFortGameStateAthena* GameState = GameMode->GameState->Cast<AFortGameStateAthena>();
	AFortPlayerStateAthena* PlayerState = NewPlayer->PlayerState->Cast<AFortPlayerStateAthena>();

	return callOG(GameMode, Stack.CurrentNativeFunction, HandleStartingNewPlayer, NewPlayer);
}


void GameMode::Hook()
{
	Utils::ExecHook(L"/Script/Engine.GameMode.ReadyToStartMatch", ReadyToStartMatch, ReadyToStartMatchOG);
	Utils::ExecHook(L"/Script/Engine.GameModeBase.SpawnDefaultPawnFor", SpawnDefaultPawnFor);
	//Utils::Hook(ImageBase + 0x444620, PickTeam, PickTeamOG);
	Utils::ExecHook(L"/Script/Engine.GameModeBase.HandleStartingNewPlayer", HandleStartingNewPlayer, HandleStartingNewPlayerOG);
	//Utils::Hook(Sarah::Offsets::ImageBase + 0x19A2D44, GetGameSessionClass);
}
