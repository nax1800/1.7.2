// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "Utils.h"
#include "Replication.h"

void Main() {
    ReplicationOffsets::Init();
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    SetConsoleTitleA("1.7.2: Setting up || Credits to @plooshi");
    LogCategory = FName(L"LogGameserver");

    Sleep(2000);

    MH_Initialize();
    for (auto& HookFunc : _HookFuncs)
        HookFunc();
    MH_EnableHook(MH_ALL_HOOKS);
    srand((uint32_t)time(0));

    *(bool*)(ImageBase + Sarah::Offsets::GIsClient) = false;

    UWorld::GetWorld()->OwningGameInstance->LocalPlayers.Remove(0);
    UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), L"open Athena_Terrain", nullptr);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ulReason, LPVOID lpReserved)
{
    if (ulReason == DLL_PROCESS_ATTACH)
    {
        std::thread(Main).detach();
    }
    return TRUE;
}

