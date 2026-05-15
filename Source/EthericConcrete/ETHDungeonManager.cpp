// Fill out your copyright notice in the Description page of Project Settings.

#include "ETHDungeonManager.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogETHDungeon, Log, All);

// ---------------------------------------------------------------------------
// Console commands
// ---------------------------------------------------------------------------

static FAutoConsoleCommand CmdForceState(
	TEXT("ETH.Dungeon.ForceState"),
	TEXT("Force a room to a specific state. Usage: ETH.Dungeon.ForceState <RoomID> <State 0-6>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogETHDungeon, Warning, TEXT("Usage: ETH.Dungeon.ForceState <RoomID> <State 0-6>"));
			return;
		}

		const int32 RoomID = FCString::Atoi(*Args[0]);
		const uint8 StateVal = (uint8)FCString::Atoi(*Args[1]);

		for (TObjectIterator<AETHDungeonManager> It; It; ++It)
		{
			if (It->GetWorld() && It->GetWorld()->IsGameWorld())
			{
				It->ForceRoomState(RoomID, (EETHRoomState)StateVal);
				return;
			}
		}
		UE_LOG(LogETHDungeon, Warning, TEXT("ETH.Dungeon.ForceState: no AETHDungeonManager found in game world."));
	}));

static FAutoConsoleCommand CmdPrintAll(
	TEXT("ETH.Dungeon.PrintAll"),
	TEXT("Print all registered rooms and their states to the log."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		for (TObjectIterator<AETHDungeonManager> It; It; ++It)
		{
			if (It->GetWorld() && It->GetWorld()->IsGameWorld())
			{
				It->PrintAllRooms();
				return;
			}
		}
		UE_LOG(LogETHDungeon, Warning, TEXT("ETH.Dungeon.PrintAll: no AETHDungeonManager found in game world."));
	}));

static FAutoConsoleCommand CmdClearAll(
	TEXT("ETH.Dungeon.ClearAll"),
	TEXT("Force all rooms to Cleared state."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		for (TObjectIterator<AETHDungeonManager> It; It; ++It)
		{
			if (It->GetWorld() && It->GetWorld()->IsGameWorld())
			{
				It->ClearAllRooms();
				return;
			}
		}
		UE_LOG(LogETHDungeon, Warning, TEXT("ETH.Dungeon.ClearAll: no AETHDungeonManager found in game world."));
	}));

static FAutoConsoleCommand CmdUnlockAll(
	TEXT("ETH.Dungeon.UnlockAll"),
	TEXT("Unlock all Locked rooms (sets them to Discovered)."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		for (TObjectIterator<AETHDungeonManager> It; It; ++It)
		{
			if (It->GetWorld() && It->GetWorld()->IsGameWorld())
			{
				It->UnlockAllRooms();
				return;
			}
		}
		UE_LOG(LogETHDungeon, Warning, TEXT("ETH.Dungeon.UnlockAll: no AETHDungeonManager found in game world."));
	}));

static FAutoConsoleCommand CmdDiscoverAll(
	TEXT("ETH.Dungeon.DiscoverAll"),
	TEXT("Discover all Undiscovered rooms."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		for (TObjectIterator<AETHDungeonManager> It; It; ++It)
		{
			if (It->GetWorld() && It->GetWorld()->IsGameWorld())
			{
				It->DiscoverAllRooms();
				return;
			}
		}
		UE_LOG(LogETHDungeon, Warning, TEXT("ETH.Dungeon.DiscoverAll: no AETHDungeonManager found in game world."));
	}));

// ---------------------------------------------------------------------------
// Actor lifecycle
// ---------------------------------------------------------------------------

AETHDungeonManager::AETHDungeonManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void AETHDungeonManager::SetTotalExpectedRooms(int32 Total)
{
	TotalExpectedRooms    = Total;
	ReadyRoomsCount       = 0;
	bAllRoomsReadyFired   = false;
	Rooms.Empty();
	PendingAdjacency.Empty();
	DoorConnections.Empty();

	UE_LOG(LogETHDungeon, Log, TEXT("SetTotalExpectedRooms: expecting %d rooms"), Total);
}

void AETHDungeonManager::RegisterRoom(int32 RoomID,
                                       AActor* WrapperActor,
                                       EETHRoomType RoomType,
                                       const TArray<int32>& AdjacentIDs,
                                       FVector InWorldLocation,
                                       int32 InDoorCount)
{
	FETHRoomEntry& Entry = Rooms.FindOrAdd(RoomID);
	Entry.Type        = RoomType;
	Entry.WrapperRef  = WrapperActor;
	Entry.WorldLocation = InWorldLocation;
	Entry.DoorCount   = InDoorCount;

	// Merge caller-supplied list with any buffered links from AddAdjacentRoom
	Entry.AdjacentRoomIDs = AdjacentIDs;
	if (TArray<int32>* Pending = PendingAdjacency.Find(RoomID))
	{
		for (int32 AdjID : *Pending)
		{
			if (!Entry.AdjacentRoomIDs.Contains(AdjID))
			{
				Entry.AdjacentRoomIDs.Add(AdjID);
			}
		}
		PendingAdjacency.Remove(RoomID);
	}

	// Determine initial state from type
	EETHRoomState InitialState;
	switch (RoomType)
	{
	case EETHRoomType::Start:  InitialState = EETHRoomState::Active;       break;
	case EETHRoomType::Locked: InitialState = EETHRoomState::Locked;       break;
	default:                   InitialState = EETHRoomState::Undiscovered;  break;
	}
	Entry.State = InitialState;

	++ReadyRoomsCount;

	if (bDebugMode)
	{
		UE_LOG(LogETHDungeon, Log, TEXT("RegisterRoom: ID=%d Type=%d State=%d  (%d/%d ready)"),
		       RoomID, (int32)RoomType, (int32)InitialState, ReadyRoomsCount, TotalExpectedRooms);
	}

	// Broadcast initial state so doors/minimap are in sync from the start
	OnRoomStateChanged.Broadcast(RoomID, InitialState);
	BP_OnRoomStateChanged(RoomID, InitialState);

	PushDebuggerUpdate();
	CheckAllRoomsReady();
}

void AETHDungeonManager::SetRoomEnemyCount(int32 RoomID, int32 Count)
{
	FETHRoomEntry* Entry = Rooms.Find(RoomID);
	if (!Entry)
	{
		UE_LOG(LogETHDungeon, Warning, TEXT("SetRoomEnemyCount: unknown RoomID %d"), RoomID);
		return;
	}
	Entry->LivingEnemyCount = Count;

	if (bDebugMode)
	{
		UE_LOG(LogETHDungeon, Log, TEXT("SetRoomEnemyCount: Room %d = %d enemies"), RoomID, Count);
	}
}

void AETHDungeonManager::SetAdjacentRooms(int32 RoomID, const TArray<int32>& AdjacentIDs)
{
	FETHRoomEntry* Entry = Rooms.Find(RoomID);
	if (!Entry)
	{
		UE_LOG(LogETHDungeon, Warning, TEXT("SetAdjacentRooms: unknown RoomID %d"), RoomID);
		return;
	}
	Entry->AdjacentRoomIDs = AdjacentIDs;

	if (bDebugMode)
	{
		TArray<FString> Parts;
		for (int32 A : AdjacentIDs) { Parts.Add(FString::FromInt(A)); }
		UE_LOG(LogETHDungeon, Log, TEXT("SetAdjacentRooms: Room %d → [%s]"),
		       RoomID, *FString::Join(Parts, TEXT(",")));
	}

	PushDebuggerUpdate();
}

void AETHDungeonManager::AddAdjacentRoom(int32 RoomID_A, int32 RoomID_B)
{
	if (RoomID_A == RoomID_B) { return; }

	auto AddLink = [&](int32 From, int32 To)
	{
		FETHRoomEntry* Entry = Rooms.Find(From);
		if (Entry)
		{
			if (!Entry->AdjacentRoomIDs.Contains(To))
			{
				Entry->AdjacentRoomIDs.Add(To);
			}
		}
		else
		{
			// Room not registered yet — buffer it
			TArray<int32>& Pending = PendingAdjacency.FindOrAdd(From);
			if (!Pending.Contains(To)) { Pending.Add(To); }
		}
	};

	AddLink(RoomID_A, RoomID_B);
	AddLink(RoomID_B, RoomID_A);

	if (bDebugMode)
	{
		UE_LOG(LogETHDungeon, Log, TEXT("AddAdjacentRoom: %d ↔ %d"), RoomID_A, RoomID_B);
	}

	PushDebuggerUpdate();
}

void AETHDungeonManager::UpdateDoorStates(int32 RoomID, const TArray<bool>& OpenStates)
{
	FETHRoomEntry* Entry = Rooms.Find(RoomID);
	if (!Entry) { return; }
	Entry->DoorOpenStates = OpenStates;
}

// ---------------------------------------------------------------------------
// Runtime events
// ---------------------------------------------------------------------------

void AETHDungeonManager::OnPlayerEnterRoom(int32 RoomID)
{
	FETHRoomEntry* Entry = Rooms.Find(RoomID);
	if (!Entry)
	{
		UE_LOG(LogETHDungeon, Warning, TEXT("OnPlayerEnterRoom: unknown RoomID %d"), RoomID);
		return;
	}

	Entry->bPlayerPresent = true;

	// Locked rooms block entry
	if (Entry->State == EETHRoomState::Locked)
	{
		return;
	}

	// Discover adjacents on first physical visit (Undiscovered OR pre-discovered by a neighbour)
	const bool bFirstVisit = (Entry->State == EETHRoomState::Undiscovered ||
	                          Entry->State == EETHRoomState::Discovered);

	// Determine new state based on type and enemy count
	if (Entry->Type == EETHRoomType::Boss)
	{
		SetRoomState(RoomID, EETHRoomState::Boss);
	}
	else if (Entry->LivingEnemyCount > 0)
	{
		SetRoomState(RoomID, EETHRoomState::Combat);
	}
	else  if (Entry->State == EETHRoomState::Undiscovered || Entry->State == EETHRoomState::Discovered)
	{
		SetRoomState(RoomID, EETHRoomState::Active);
	}

	if (bFirstVisit)
	{
		DiscoverAdjacentRooms(RoomID);
	}

	// Track current/previous room
	if (CurrentRoomID != RoomID)
	{
		PreviousRoomID = CurrentRoomID;
		CurrentRoomID  = RoomID;

		OnRoomChanged.Broadcast(CurrentRoomID, PreviousRoomID);
		BP_OnRoomChanged(CurrentRoomID, PreviousRoomID);

		if (bDebugMode)
		{
			UE_LOG(LogETHDungeon, Log, TEXT("OnPlayerEnterRoom: moved to Room %d (prev: %d)"),
			       CurrentRoomID, PreviousRoomID);
		}
	}

	PushDebuggerUpdate();
}

void AETHDungeonManager::OnPlayerExitRoom(int32 RoomID)
{
	FETHRoomEntry* Entry = Rooms.Find(RoomID);
	if (!Entry)
	{
		UE_LOG(LogETHDungeon, Warning, TEXT("OnPlayerExitRoom: unknown RoomID %d"), RoomID);
		return;
	}

	Entry->bPlayerPresent = false;

	// Only transition if there are no enemies left
	if (Entry->LivingEnemyCount == 0)
	{
		if (Entry->State == EETHRoomState::Active || Entry->State == EETHRoomState::Discovered)
		{
			SetRoomState(RoomID, EETHRoomState::Discovered);
		}
	}
	// If enemies are still alive (Combat/Boss), leave state unchanged

	if (bDebugMode)
	{
		UE_LOG(LogETHDungeon, Log, TEXT("OnPlayerExitRoom: left Room %d"), RoomID);
	}

	PushDebuggerUpdate();
}

void AETHDungeonManager::OnEnemyDied(int32 RoomID)
{
	FETHRoomEntry* Entry = Rooms.Find(RoomID);
	if (!Entry)
	{
		UE_LOG(LogETHDungeon, Warning, TEXT("OnEnemyDied: unknown RoomID %d"), RoomID);
		return;
	}

	if (Entry->LivingEnemyCount > 0)
	{
		--Entry->LivingEnemyCount;
	}

	if (bDebugMode)
	{
		UE_LOG(LogETHDungeon, Log, TEXT("OnEnemyDied: Room %d — %d enemies remaining"),
		       RoomID, Entry->LivingEnemyCount);
	}

	if (Entry->LivingEnemyCount == 0)
	{
		if (Entry->bPlayerPresent)
		{
			SetRoomState(RoomID, EETHRoomState::Cleared);
		}
		else
		{
			SetRoomState(RoomID, EETHRoomState::Discovered);
		}
	}

	PushDebuggerUpdate();
}

void AETHDungeonManager::UnlockRoom(int32 RoomID)
{
	FETHRoomEntry* Entry = Rooms.Find(RoomID);
	if (!Entry)
	{
		UE_LOG(LogETHDungeon, Warning, TEXT("UnlockRoom: unknown RoomID %d"), RoomID);
		return;
	}

	if (Entry->State != EETHRoomState::Locked)
	{
		return;
	}

	if (Entry->bPlayerPresent)
	{
		SetRoomState(RoomID, EETHRoomState::Active);
	}
	else
	{
		SetRoomState(RoomID, EETHRoomState::Discovered);
	}

	if (bDebugMode)
	{
		UE_LOG(LogETHDungeon, Log, TEXT("UnlockRoom: Room %d unlocked"), RoomID);
	}

	PushDebuggerUpdate();
}

// ---------------------------------------------------------------------------
// Query
// ---------------------------------------------------------------------------

EETHRoomState AETHDungeonManager::GetRoomState(int32 RoomID) const
{
	const FETHRoomEntry* Entry = Rooms.Find(RoomID);
	return Entry ? Entry->State : EETHRoomState::Undiscovered;
}

bool AETHDungeonManager::IsPlayerInRoom(int32 RoomID) const
{
	const FETHRoomEntry* Entry = Rooms.Find(RoomID);
	return Entry ? Entry->bPlayerPresent : false;
}

TArray<FETHRoomDebugData> AETHDungeonManager::GetMinimapData() const
{
	TArray<FETHRoomDebugData> Result;
	Result.Reserve(Rooms.Num());

	for (const auto& Pair : Rooms)
	{
		FETHRoomDebugData Data;
		Data.RoomID          = Pair.Key;
		Data.RoomState       = (uint8)Pair.Value.State;
		Data.WorldLocation   = Pair.Value.WorldLocation;
		Data.AdjacentRoomIDs = Pair.Value.AdjacentRoomIDs;
		Data.DoorCount       = Pair.Value.DoorCount;
		Result.Add(Data);
	}

	return Result;
}

// ---------------------------------------------------------------------------
// Cheat / Debug
// ---------------------------------------------------------------------------

void AETHDungeonManager::ForceRoomState(int32 RoomID, EETHRoomState State)
{
	if (!Rooms.Contains(RoomID))
	{
		UE_LOG(LogETHDungeon, Warning, TEXT("ForceRoomState: unknown RoomID %d"), RoomID);
		return;
	}
	UE_LOG(LogETHDungeon, Log, TEXT("ForceRoomState: Room %d → State %d"), RoomID, (int32)State);
	SetRoomState(RoomID, State);
	PushDebuggerUpdate();
}

// ---------------------------------------------------------------------------
// State name helpers (mirrors AETHDungeonDebugger)
// ---------------------------------------------------------------------------

static const TCHAR* RoomStateName(EETHRoomState State)
{
	switch (State)
	{
	case EETHRoomState::Undiscovered: return TEXT("Undiscovered");
	case EETHRoomState::Discovered:   return TEXT("Discovered");
	case EETHRoomState::Active:       return TEXT("Active");
	case EETHRoomState::Combat:       return TEXT("Combat");
	case EETHRoomState::Cleared:      return TEXT("Cleared");
	case EETHRoomState::Locked:       return TEXT("Locked");
	case EETHRoomState::Boss:         return TEXT("Boss");
	default:                          return TEXT("Unknown");
	}
}

static FColor RoomStateColor(EETHRoomState State)
{
	switch (State)
	{
	case EETHRoomState::Undiscovered: return FColor(64,  64,  64);
	case EETHRoomState::Discovered:   return FColor(50,  100, 200);
	case EETHRoomState::Active:       return FColor(50,  200, 50);
	case EETHRoomState::Combat:       return FColor(220, 30,  30);
	case EETHRoomState::Cleared:      return FColor(0,   200, 220);
	case EETHRoomState::Locked:       return FColor(220, 200, 0);
	case EETHRoomState::Boss:         return FColor(140, 30,  200);
	default:                          return FColor::White;
	}
}

void AETHDungeonManager::PrintAllRooms()
{
	UE_LOG(LogETHDungeon, Log, TEXT("===== Dungeon Rooms (%d/%d ready) ====="),
	       ReadyRoomsCount, TotalExpectedRooms);
	UE_LOG(LogETHDungeon, Log, TEXT("CurrentRoom: %d  PreviousRoom: %d"),
	       CurrentRoomID, PreviousRoomID);

	TArray<int32> Keys;
	Rooms.GetKeys(Keys);
	Keys.Sort();

	UWorld* World = GetWorld();

	// Build location lookup for draw debug
	TMap<int32, FVector> LocByID;
	for (const auto& Pair : Rooms) { LocByID.Add(Pair.Key, Pair.Value.WorldLocation); }

	for (int32 ID : Keys)
	{
		const FETHRoomEntry& E = Rooms[ID];

		// Build adjacent room list with their states
		TArray<FString> AdjParts;
		for (int32 AdjID : E.AdjacentRoomIDs)
		{
			const FETHRoomEntry* Adj = Rooms.Find(AdjID);
			if (Adj)
			{
				AdjParts.Add(FString::Printf(TEXT("%d(%s)"), AdjID, RoomStateName(Adj->State)));
			}
			else
			{
				AdjParts.Add(FString::Printf(TEXT("%d(?)"), AdjID));
			}
		}

		// Build door states string: "O C O" (O=open, C=closed)
		TArray<FString> DoorParts;
		for (int32 d = 0; d < E.DoorCount; ++d)
		{
			const bool bOpen = E.DoorOpenStates.IsValidIndex(d) ? E.DoorOpenStates[d] : false;
			DoorParts.Add(bOpen ? TEXT("O") : TEXT("C"));
		}
		const FString DoorStr = DoorParts.Num() > 0
		    ? FString::Join(DoorParts, TEXT(" "))
		    : TEXT("?");

		UE_LOG(LogETHDungeon, Log,
		       TEXT("  Room %d | %-13s | Type=%d | Enemies=%d | Player=%d | Doors=[%s] | Adj=[%s]"),
		       ID, RoomStateName(E.State), (int32)E.Type, E.LivingEnemyCount,
		       (int32)E.bPlayerPresent, *DoorStr,
		       *FString::Join(AdjParts, TEXT(", ")));

		// --- Visual debug (15 seconds) ---
		if (World)
		{
			const FColor Col = RoomStateColor(E.State);

			// Sphere per room
			DrawDebugSphere(World, E.WorldLocation, 120.f, 12, Col, false, 15.f, 0, 3.f);

			// Room ID + state label
			const FString Label = FString::Printf(TEXT("Room %d\n%s"), ID, RoomStateName(E.State));
			DrawDebugString(World, E.WorldLocation + FVector(0, 0, 300.f), Label, nullptr, Col, 15.f, false, 1.2f);

			// Adjacency lines — each edge drawn once, coloured by the worse of the two states
			for (int32 AdjID : E.AdjacentRoomIDs)
			{
				if (AdjID <= ID) { continue; } // draw each edge once
				const FVector* AdjLoc = LocByID.Find(AdjID);
				if (!AdjLoc) { continue; }

				const FETHRoomEntry* Adj = Rooms.Find(AdjID);
				// Dim green if both sides discovered/active, red if one is combat/locked
				const bool bProblem = Adj &&
				    (Adj->State == EETHRoomState::Locked || E.State == EETHRoomState::Locked);
				const FColor LineCol = bProblem ? FColor(220, 30, 30) : FColor(0, 180, 80);
				DrawDebugLine(World, E.WorldLocation, *AdjLoc, LineCol, false, 15.f, 0, 2.f);

				// Mid-point label: "R0↔R1"
				const FVector Mid = (E.WorldLocation + *AdjLoc) * 0.5f;
				DrawDebugString(World, Mid + FVector(0, 0, 60.f),
				                FString::Printf(TEXT("R%d↔R%d"), ID, AdjID),
				                nullptr, FColor::White, 15.f, false, 0.9f);
			}

			// Highlight current room with a white ring
			if (ID == CurrentRoomID)
			{
				DrawDebugSphere(World, E.WorldLocation, 155.f, 12, FColor::White, false, 15.f, 0, 4.f);
			}
		}
	}

	// ----------------------------------------------------------------
	// DungeonTree — derived from adjacency (rooms placed in ID order)
	// Parent of room N = adjacent room with min ID that is < N
	// Children of room N = adjacent rooms with ID > N
	// Depth computed via BFS from the root (room with no parent)
	// ----------------------------------------------------------------
	{
		// 1. Derive parent & children maps
		TMap<int32, int32>         ParentOf;   // RoomID → ParentID (-1 = root)
		TMap<int32, TArray<int32>> ChildrenOf; // RoomID → child IDs

		for (int32 ID : Keys)
		{
			ParentOf.Add(ID, -1);
			ChildrenOf.Add(ID, TArray<int32>());
		}

		for (int32 ID : Keys)
		{
			const FETHRoomEntry& E = Rooms[ID];
			for (int32 AdjID : E.AdjacentRoomIDs)
			{
				if (AdjID < ID)
				{
					// AdjID is a candidate parent — take the minimum (closest ancestor)
					if (ParentOf[ID] == -1 || AdjID < ParentOf[ID])
					{
						ParentOf[ID] = AdjID;
					}
				}
			}
		}

		// Populate children from parent map
		for (int32 ID : Keys)
		{
			const int32 Par = ParentOf[ID];
			if (Par != -1 && ChildrenOf.Contains(Par))
			{
				ChildrenOf[Par].Add(ID);
			}
		}

		// 2. BFS depth from root
		TMap<int32, int32> DepthOf;
		TQueue<int32> Queue;

		// Find root(s) — rooms with no parent
		for (int32 ID : Keys)
		{
			if (ParentOf[ID] == -1)
			{
				DepthOf.Add(ID, 0);
				Queue.Enqueue(ID);
			}
		}
		while (!Queue.IsEmpty())
		{
			int32 Cur;
			Queue.Dequeue(Cur);
			const int32 CurDepth = DepthOf[Cur];
			for (int32 ChildID : ChildrenOf[Cur])
			{
				DepthOf.Add(ChildID, CurDepth + 1);
				Queue.Enqueue(ChildID);
			}
		}

		// 3. Log tree
		UE_LOG(LogETHDungeon, Log, TEXT("===== DungeonTree ====="));
		for (int32 ID : Keys)
		{
			const int32 Par   = ParentOf.FindRef(ID);
			const int32 Depth = DepthOf.FindRef(ID);
			const TArray<int32>& RoomChildren = ChildrenOf[ID];

			TArray<FString> ChildStrs;
			for (int32 C : RoomChildren) { ChildStrs.Add(FString::FromInt(C)); }

			const FString Indent = FString::ChrN(Depth * 2, TEXT(' '));
			UE_LOG(LogETHDungeon, Log,
			       TEXT("%s  Room %d | Parent=%d | Depth=%d | Children=[%s]"),
			       *Indent, ID, Par, Depth,
			       ChildStrs.Num() > 0 ? *FString::Join(ChildStrs, TEXT(", ")) : TEXT("none"));
		}

		// 4. Visual debug — orange lines parent → child (15 s)
		if (World)
		{
			const FColor OrangeCol(255, 140, 0);
			for (int32 ID : Keys)
			{
				const int32 Par = ParentOf[ID];
				if (Par == -1) { continue; }

				const FVector* ChildLoc  = LocByID.Find(ID);
				const FVector* ParentLoc = LocByID.Find(Par);
				if (!ChildLoc || !ParentLoc) { continue; }

				// Offset slightly above adjacency lines to avoid z-fighting
				const FVector Offset(0, 0, 80.f);
				DrawDebugLine(World, *ParentLoc + Offset, *ChildLoc + Offset,
				              OrangeCol, false, 15.f, 1, 4.f);

				// Depth label at child position
				DrawDebugString(World, *ChildLoc + FVector(0, 0, 420.f),
				                FString::Printf(TEXT("D%d"), DepthOf.FindRef(ID)),
				                nullptr, OrangeCol, 15.f, false, 1.0f);
			}
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Cyan,
			FString::Printf(TEXT("ETH.Dungeon.PrintAll — %d rooms drawn for 15s"), Rooms.Num()));
	}

	PushDebuggerUpdate();
}

void AETHDungeonManager::ClearAllRooms()
{
	UE_LOG(LogETHDungeon, Log, TEXT("ClearAllRooms: forcing all rooms to Cleared"));
	for (auto& Pair : Rooms)
	{
		SetRoomState(Pair.Key, EETHRoomState::Cleared);
	}
	PushDebuggerUpdate();
}

void AETHDungeonManager::UnlockAllRooms()
{
	UE_LOG(LogETHDungeon, Log, TEXT("UnlockAllRooms: unlocking all Locked rooms"));
	for (auto& Pair : Rooms)
	{
		if (Pair.Value.State == EETHRoomState::Locked)
		{
			SetRoomState(Pair.Key, EETHRoomState::Discovered);
		}
	}
	PushDebuggerUpdate();
}

void AETHDungeonManager::DiscoverAllRooms()
{
	UE_LOG(LogETHDungeon, Log, TEXT("DiscoverAllRooms: discovering all Undiscovered rooms"));
	for (auto& Pair : Rooms)
	{
		if (Pair.Value.State == EETHRoomState::Undiscovered)
		{
			SetRoomState(Pair.Key, EETHRoomState::Discovered);
		}
	}
	PushDebuggerUpdate();
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void AETHDungeonManager::SetRoomState(int32 RoomID, EETHRoomState NewState)
{
	FETHRoomEntry* Entry = Rooms.Find(RoomID);
	if (!Entry)
	{
		return;
	}

	// Guard against no-op transitions
	if (Entry->State == NewState)
	{
		return;
	}

	if (bDebugMode)
	{
		UE_LOG(LogETHDungeon, Log, TEXT("SetRoomState: Room %d  %d → %d"),
		       RoomID, (int32)Entry->State, (int32)NewState);
	}

	Entry->State = NewState;

	OnRoomStateChanged.Broadcast(RoomID, NewState);
	BP_OnRoomStateChanged(RoomID, NewState);

	// Refresh all doors connected to this room (centralized AND logic)
	RefreshDoorsForRoom(RoomID);

	// Notify adjacent rooms so they can re-evaluate their shared door with this room
	for (int32 AdjID : Entry->AdjacentRoomIDs)
	{
		BP_OnNeighborStateChanged(AdjID, RoomID, NewState);
	}
}

void AETHDungeonManager::DiscoverAdjacentRooms(int32 RoomID)
{
	const FETHRoomEntry* Entry = Rooms.Find(RoomID);
	if (!Entry)
	{
		return;
	}

	for (int32 AdjID : Entry->AdjacentRoomIDs)
	{
		FETHRoomEntry* Adj = Rooms.Find(AdjID);
		if (!Adj)
		{
			continue;
		}

		// Only discover Undiscovered rooms that are not Locked type
		if (Adj->State == EETHRoomState::Undiscovered && Adj->Type != EETHRoomType::Locked)
		{
			SetRoomState(AdjID, EETHRoomState::Discovered);
		}
	}
}

void AETHDungeonManager::PushDebuggerUpdate()
{
	if (!DebuggerRef)
	{
		return;
	}

	TArray<FETHRoomDebugData> DebugRooms;
	DebugRooms.Reserve(Rooms.Num());

	for (const auto& Pair : Rooms)
	{
		FETHRoomDebugData Data;
		Data.RoomID           = Pair.Key;
		Data.RoomState        = (uint8)Pair.Value.State;
		Data.WorldLocation    = Pair.Value.WorldLocation;
		Data.AdjacentRoomIDs  = Pair.Value.AdjacentRoomIDs;
		Data.DoorCount        = Pair.Value.DoorCount;
		// ConnectorWorldLocations left empty — populated by BP_RoomWrapper if needed
		DebugRooms.Add(Data);
	}

	DebuggerRef->UpdateRooms(DebugRooms, CurrentRoomID, PreviousRoomID, ReadyRoomsCount, TotalExpectedRooms);
}

void AETHDungeonManager::CheckAllRoomsReady()
{
	if (bAllRoomsReadyFired)
	{
		return;
	}

	if (TotalExpectedRooms > 0 && ReadyRoomsCount >= TotalExpectedRooms)
	{
		bAllRoomsReadyFired = true;
		UE_LOG(LogETHDungeon, Log, TEXT("CheckAllRoomsReady: all %d rooms registered — firing BP_OnAllRoomsReady"),
		       TotalExpectedRooms);
		BP_OnAllRoomsReady();
	}
}

// ---------------------------------------------------------------------------
// Door Connection System
// ---------------------------------------------------------------------------

bool AETHDungeonManager::IsOpenState(EETHRoomState State)
{
	return State == EETHRoomState::Cleared;
}

static bool IsBlockingState(EETHRoomState State)
{
	return State == EETHRoomState::Active
	    || State == EETHRoomState::Combat
	    || State == EETHRoomState::Boss
	    || State == EETHRoomState::Locked;
}

void AETHDungeonManager::RegisterPendingDoorConnection(int32 RoomID_A, int32 RoomID_B, FVector ConnectorWorldPos)
{
	if (RoomID_A == RoomID_B) { return; }

	// Avoid duplicate connections
	for (const FETHDoorConnection& Conn : DoorConnections)
	{
		if ((Conn.RoomID_A == RoomID_A && Conn.RoomID_B == RoomID_B) ||
		    (Conn.RoomID_A == RoomID_B && Conn.RoomID_B == RoomID_A))
		{
			return;
		}
	}

	FETHDoorConnection NewConn;
	NewConn.RoomID_A           = RoomID_A;
	NewConn.RoomID_B           = RoomID_B;
	NewConn.ConnectorWorldPos  = ConnectorWorldPos;
	DoorConnections.Add(NewConn);

	if (bDebugMode)
	{
		UE_LOG(LogETHDungeon, Log, TEXT("RegisterPendingDoorConnection: Room %d ↔ Room %d at %s"),
		       RoomID_A, RoomID_B, *ConnectorWorldPos.ToString());
	}
}

void AETHDungeonManager::AssignDoorActorToConnection(int32 RoomID, AActor* DoorActor,
                                                      FVector DoorWorldPos, float Tolerance)
{
	if (!DoorActor) { return; }

	UE_LOG(LogETHDungeon, Log,
	       TEXT("AssignDoorActorToConnection: called for Room %d | Door='%s' | Pos=%s | Tolerance=%.0f | DoorConnections.Num()=%d"),
	       RoomID, *DoorActor->GetName(), *DoorWorldPos.ToString(), Tolerance, DoorConnections.Num());

	const float ToleranceSq = Tolerance * Tolerance;

	for (FETHDoorConnection& Conn : DoorConnections)
	{
		const float DistSq = FVector::DistSquared(Conn.ConnectorWorldPos, DoorWorldPos);
		UE_LOG(LogETHDungeon, Log,
		       TEXT("  Checking conn Room %d↔%d | HasActor=%d | MatchesRoom=%d | DistSq=%.0f (need < %.0f)"),
		       Conn.RoomID_A, Conn.RoomID_B,
		       Conn.DoorActor.IsValid() ? 1 : 0,
		       (Conn.RoomID_A == RoomID || Conn.RoomID_B == RoomID) ? 1 : 0,
		       DistSq, ToleranceSq);

		if (Conn.DoorActor.IsValid()) { continue; }
		if (Conn.RoomID_A != RoomID && Conn.RoomID_B != RoomID) { continue; }

		if (DistSq < ToleranceSq)
		{
			Conn.DoorActor = DoorActor;

			UE_LOG(LogETHDungeon, Log, TEXT("AssignDoorActorToConnection: MATCHED Door '%s' → Room %d ↔ Room %d"),
			       *DoorActor->GetName(), Conn.RoomID_A, Conn.RoomID_B);

			RefreshDoorsForRoom(RoomID);
			return;
		}
	}

	UE_LOG(LogETHDungeon, Warning,
	       TEXT("AssignDoorActorToConnection: NO MATCH for Room %d near %s (tol=%.0f) — %d connections checked"),
	       RoomID, *DoorWorldPos.ToString(), Tolerance, DoorConnections.Num());
}

bool AETHDungeonManager::ShouldDoorBeOpen(int32 RoomID_A, int32 RoomID_B) const
{
	const EETHRoomState StateA = GetRoomState(RoomID_A);
	const EETHRoomState StateB = GetRoomState(RoomID_B);
	return (IsOpenState(StateA) || IsOpenState(StateB))
	    && !IsBlockingState(StateA) && !IsBlockingState(StateB);
}

void AETHDungeonManager::RefreshDoorsForRoom(int32 RoomID)
{
	UE_LOG(LogETHDungeon, Log, TEXT("RefreshDoorsForRoom: Room %d | DoorConnections.Num()=%d"),
	       RoomID, DoorConnections.Num());

	for (const FETHDoorConnection& Conn : DoorConnections)
	{
		if (Conn.RoomID_A != RoomID && Conn.RoomID_B != RoomID) { continue; }
		if (!Conn.DoorActor.IsValid()) { continue; }

		const EETHRoomState StateA = GetRoomState(Conn.RoomID_A);
		const EETHRoomState StateB = GetRoomState(Conn.RoomID_B);
		const bool bOpenA = IsOpenState(StateA);
		const bool bOpenB = IsOpenState(StateB);
		const bool bOpen  = (bOpenA || bOpenB) && !IsBlockingState(StateA) && !IsBlockingState(StateB);

		UE_LOG(LogETHDungeon, Log,
		       TEXT("  Door '%s' Room%d(%d)=%d Room%d(%d)=%d → %s"),
		       *Conn.DoorActor.Get()->GetName(),
		       Conn.RoomID_A, (int32)GetRoomState(Conn.RoomID_A), bOpenA ? 1 : 0,
		       Conn.RoomID_B, (int32)GetRoomState(Conn.RoomID_B), bOpenB ? 1 : 0,
		       bOpen ? TEXT("OPEN") : TEXT("CLOSE"));

		BP_SetDoorOpen(Conn.DoorActor.Get(), bOpen);
	}
}
