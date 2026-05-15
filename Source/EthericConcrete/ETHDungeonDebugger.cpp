// Fill out your copyright notice in the Description page of Project Settings.

#include "ETHDungeonDebugger.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"

// ---------------------------------------------------------------------------
// Console variable — ETH.Debug.Dungeon 1 (enabled by default)
// ---------------------------------------------------------------------------
static TAutoConsoleVariable<int32> CVarDungeonDebug(
	TEXT("ETH.Debug.Dungeon"),
	1,
	TEXT("Show dungeon debug overlay: 1 = enabled, 0 = disabled"),
	ECVF_Cheat);

// ---------------------------------------------------------------------------
// State color / name helpers
// ---------------------------------------------------------------------------

FColor AETHDungeonDebugger::GetColorForState(uint8 State)
{
	switch (State)
	{
	case 0:  return FColor(64,  64,  64);   // Undiscovered — Dark Gray
	case 1:  return FColor(50,  100, 200);  // Discovered   — Blue
	case 2:  return FColor(50,  200, 50);   // Active       — Green
	case 3:  return FColor(220, 30,  30);   // Combat       — Red
	case 4:  return FColor(0,   200, 220);  // Cleared      — Cyan
	case 5:  return FColor(220, 200, 0);    // Locked       — Yellow
	case 6:  return FColor(140, 30,  200);  // Boss         — Purple
	default: return FColor::White;
	}
}

FString AETHDungeonDebugger::GetNameForState(uint8 State)
{
	switch (State)
	{
	case 0:  return TEXT("Undiscovered");
	case 1:  return TEXT("Discovered");
	case 2:  return TEXT("Active");
	case 3:  return TEXT("Combat");
	case 4:  return TEXT("Cleared");
	case 5:  return TEXT("Locked");
	case 6:  return TEXT("Boss");
	default: return TEXT("Unknown");
	}
}

// ---------------------------------------------------------------------------
// Actor lifecycle
// ---------------------------------------------------------------------------

AETHDungeonDebugger::AETHDungeonDebugger()
{
	PrimaryActorTick.bCanEverTick = true;
	// Runs even when the game is paused so the overlay stays visible in debug
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void AETHDungeonDebugger::BeginPlay()
{
	Super::BeginPlay();
}

void AETHDungeonDebugger::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bEnabled || CVarDungeonDebug.GetValueOnGameThread() == 0)
	{
		return;
	}

	if (bShowInWorldSpheres || bShowStateLabels || bShowAdjacencyLines || bShowConnectorDots)
	{
		DrawWorldDebug();
	}

	if (bShowHUDPanel)
	{
		DrawHUDPanel();
	}
}

// ---------------------------------------------------------------------------
// Blueprint-callable API
// ---------------------------------------------------------------------------

void AETHDungeonDebugger::UpdateRooms(const TArray<FETHRoomDebugData>& Rooms,
                                       int32 InCurrentRoomID,
                                       int32 InPreviousRoomID,
                                       int32 InReadyCount,
                                       int32 InTotalCount)
{
	CachedRooms    = Rooms;
	CurrentRoomID  = InCurrentRoomID;
	PreviousRoomID = InPreviousRoomID;
	ReadyCount     = InReadyCount;
	TotalCount     = InTotalCount;
}

void AETHDungeonDebugger::SetEnabled(bool bNewEnabled)
{
	bEnabled = bNewEnabled;

	// Clear any lingering HUD messages when disabled
	if (!bNewEnabled && GEngine)
	{
		GEngine->ClearOnScreenDebugMessages();
	}
}

// ---------------------------------------------------------------------------
// World-space debug geometry
// ---------------------------------------------------------------------------

void AETHDungeonDebugger::DrawWorldDebug() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Build a quick lookup: RoomID -> WorldLocation for adjacency lines
	TMap<int32, FVector> LocationByID;
	LocationByID.Reserve(CachedRooms.Num());
	for (const FETHRoomDebugData& Room : CachedRooms)
	{
		LocationByID.Add(Room.RoomID, Room.WorldLocation);
	}

	for (const FETHRoomDebugData& Room : CachedRooms)
	{
		const FColor RoomColor = GetColorForState(Room.RoomState);
		const FVector& Loc     = Room.WorldLocation;

		// --- Sphere ---
		if (bShowInWorldSpheres)
		{
			DrawDebugSphere(World, Loc, SphereRadius, 12, RoomColor, false, -1.f, 0, 2.f);

			// White outline for the currently active room
			if (Room.RoomID == CurrentRoomID)
			{
				DrawDebugSphere(World, Loc, SphereRadius + 30.f, 12, FColor::White, false, -1.f, 0, 3.f);
			}
		}

		// --- State label ---
		if (bShowStateLabels)
		{
			const FString Label = FString::Printf(TEXT("Room %d\n%s\nDoors: %d"),
			                                      Room.RoomID,
			                                      *GetNameForState(Room.RoomState),
			                                      Room.DoorCount);
			DrawDebugString(World,
			                Loc + FVector(0.f, 0.f, LabelHeightOffset),
			                Label,
			                nullptr,
			                RoomColor,
			                -1.f,
			                false,
			                1.2f);
		}

		// --- Connector dots ---
		if (bShowConnectorDots)
		{
			for (const FVector& ConnLoc : Room.ConnectorWorldLocations)
			{
				DrawDebugSphere(World, ConnLoc, 20.f, 6, FColor::Orange, false, -1.f, 0, 1.5f);
			}
		}

		// --- Adjacency lines ---
		if (bShowAdjacencyLines)
		{
			for (const int32 AdjID : Room.AdjacentRoomIDs)
			{
				// Only draw each edge once (lower ID draws to higher ID)
				if (AdjID <= Room.RoomID)
				{
					continue;
				}

				const FVector* AdjLoc = LocationByID.Find(AdjID);
				if (AdjLoc)
				{
					DrawDebugLine(World, Loc, *AdjLoc, FColor(0, 120, 0), false, -1.f, 0, 1.5f);
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------
// On-screen HUD panel
// ---------------------------------------------------------------------------

void AETHDungeonDebugger::DrawHUDPanel() const
{
	if (!GEngine)
	{
		return;
	}

	// Negative keys are permanent-until-overwritten — each call replaces the
	// previous frame's message at that key, so messages don't stack.
	int32 Key = -1;

	GEngine->AddOnScreenDebugMessage(Key--, 0.f, FColor::White,
		TEXT("==========================="));
	GEngine->AddOnScreenDebugMessage(Key--, 0.f, FColor::White,
		TEXT("      DUNGEON DEBUG        "));
	GEngine->AddOnScreenDebugMessage(Key--, 0.f, FColor::White,
		TEXT("==========================="));

	GEngine->AddOnScreenDebugMessage(Key--, 0.f, FColor::White,
		FString::Printf(TEXT("Rooms ready : %d / %d"), ReadyCount, TotalCount));

	GEngine->AddOnScreenDebugMessage(Key--, 0.f, FColor::White,
		FString::Printf(TEXT("Current     : Room %d  |  Prev: Room %d"),
		                CurrentRoomID, PreviousRoomID));

	GEngine->AddOnScreenDebugMessage(Key--, 0.f, FColor(180, 180, 180), TEXT(" "));

	// Per-room lines, sorted by RoomID for stable display
	TArray<FETHRoomDebugData> Sorted = CachedRooms;
	Sorted.Sort([](const FETHRoomDebugData& A, const FETHRoomDebugData& B)
	{
		return A.RoomID < B.RoomID;
	});

	for (const FETHRoomDebugData& Room : Sorted)
	{
		const FColor RoomColor  = GetColorForState(Room.RoomState);
		const FString RoomLine  = FString::Printf(TEXT("  [%d] %-13s  Doors: %d%s"),
		                                          Room.RoomID,
		                                          *GetNameForState(Room.RoomState),
		                                          Room.DoorCount,
		                                          Room.RoomID == CurrentRoomID ? TEXT("  <") : TEXT(""));
		GEngine->AddOnScreenDebugMessage(Key--, 0.f, RoomColor, RoomLine);
	}
}
