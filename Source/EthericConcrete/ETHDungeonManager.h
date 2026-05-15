// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ETHDungeonDebugger.h"
#include "ETHDungeonManager.generated.h"

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

/** Mirrors E_RoomState Blueprint enum — values MUST stay in sync. */
UENUM(BlueprintType)
enum class EETHRoomState : uint8
{
	Undiscovered = 0,
	Discovered   = 1,
	Active       = 2,
	Combat       = 3,
	Cleared      = 4,
	Locked       = 5,
	Boss         = 6,
};

/** Broad room category — drives state machine rules. */
UENUM(BlueprintType)
enum class EETHRoomType : uint8
{
	Generic   = 0,
	Start     = 1,
	Enemy     = 2,
	Chest     = 3,
	Corridor  = 4,
	Boss      = 5,
	Locked    = 6,
};

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FETHOnRoomStateChanged, int32, RoomID, EETHRoomState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FETHOnRoomChanged, int32, NewRoomID, int32, PreviousRoomID);

// ---------------------------------------------------------------------------
// Internal door-connection data (plain C++ struct — not exposed to BP)
// ---------------------------------------------------------------------------

/** Models a bidirectional door link between two rooms with its actor reference. */
struct FETHDoorConnection
{
	int32 RoomID_A = -1;
	int32 RoomID_B = -1;
	TWeakObjectPtr<AActor> DoorActor;
	FVector ConnectorWorldPos = FVector::ZeroVector;
};

// ---------------------------------------------------------------------------
// Internal per-room data (plain C++ struct — not exposed to BP)
// ---------------------------------------------------------------------------

USTRUCT()
struct FETHRoomEntry
{
	GENERATED_BODY()

	EETHRoomState          State            = EETHRoomState::Undiscovered;
	EETHRoomType           Type             = EETHRoomType::Generic;
	TWeakObjectPtr<AActor> WrapperRef;
	TArray<int32>          AdjacentRoomIDs;
	FVector                WorldLocation    = FVector::ZeroVector;
	int32                  DoorCount        = 0;
	int32                  LivingEnemyCount = 0;
	bool                   bPlayerPresent   = false;
	TArray<bool>           DoorOpenStates;   // updated by BP via UpdateDoorStates()
};

// ---------------------------------------------------------------------------
// AETHDungeonManager
// ---------------------------------------------------------------------------

UCLASS()
class ETHERICCONCRETE_API AETHDungeonManager : public AActor
{
	GENERATED_BODY()

public:
	AETHDungeonManager();

	// -------------------------------------------------------
	// Delegates — external systems bind here
	// -------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Dungeon")
	FETHOnRoomStateChanged OnRoomStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Dungeon")
	FETHOnRoomChanged OnRoomChanged;

	// -------------------------------------------------------
	// Config
	// -------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Debug")
	bool bDebugMode = true;

	/** Optional: place AETHDungeonDebugger in the level and assign here. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Dungeon|Debug")
	TObjectPtr<AETHDungeonDebugger> DebuggerRef;

	// -------------------------------------------------------
	// Registration API (called by BP_RoomWrapper / Generator)
	// -------------------------------------------------------

	/** Call from BP_DungeonGeneratorNEW before spawning any rooms. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	void SetTotalExpectedRooms(int32 Total);

	/**
	 * Register a room once its sub-level is loaded and its wrapper actor is ready.
	 * @param RoomID         Unique ID assigned by the generator.
	 * @param WrapperActor   The BP_RoomWrapper actor.
	 * @param RoomType       Category (drives initial state and transition rules).
	 * @param AdjacentIDs    Neighbouring room IDs (for graph traversal).
	 * @param InWorldLocation  World-space centre of the room (for debugger).
	 * @param InDoorCount    Number of doors in the room (for debugger).
	 */
	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	void RegisterRoom(int32 RoomID,
	                  AActor* WrapperActor,
	                  EETHRoomType RoomType,
	                  const TArray<int32>& AdjacentIDs,
	                  FVector InWorldLocation,
	                  int32 InDoorCount);

	/** Update the living enemy count for a room (call after enemy spawn). */
	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	void SetRoomEnemyCount(int32 RoomID, int32 Count);

	/**
	 * Overwrite the adjacency list for an already-registered room.
	 * Call this from BP_DungeonGeneratorNEW after LinkConnectorTransforms completes,
	 * once each wrapper's AdjacentRoomIDs array has been populated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	void SetAdjacentRooms(int32 RoomID, const TArray<int32>& AdjacentIDs);

	/**
	 * Record a single bidirectional adjacency link between two rooms.
	 * Call this from LinkConnectorTransforms the moment a matching connector pair is found,
	 * instead of reading back BP arrays after the fact.
	 * Safe to call before both rooms are registered — link is stored and applied on registration.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	void AddAdjacentRoom(int32 RoomID_A, int32 RoomID_B);

	/**
	 * Push the current open/closed state of each door in the room.
	 * Call this at the end of BP_RoomWrapper's UpdateDoors() so PrintAll can show door states.
	 * @param OpenStates  One bool per door, true = open. Order must match RegisteredDoors array.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	void UpdateDoorStates(int32 RoomID, const TArray<bool>& OpenStates);

	// -------------------------------------------------------
	// Runtime events (called by BP_RoomWrapper overlaps / enemies)
	// -------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	void OnPlayerEnterRoom(int32 RoomID);

	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	void OnPlayerExitRoom(int32 RoomID);

	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	void OnEnemyDied(int32 RoomID);

	/** Unlock a Locked room (key/puzzle systems). */
	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	void UnlockRoom(int32 RoomID);

	// -------------------------------------------------------
	// Query API (BlueprintPure)
	// -------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Dungeon")
	EETHRoomState GetRoomState(int32 RoomID) const;

	UFUNCTION(BlueprintPure, Category = "Dungeon")
	bool IsPlayerInRoom(int32 RoomID) const;

	/** Returns a snapshot of all registered rooms for minimap / HUD rendering. */
	UFUNCTION(BlueprintPure, Category = "Dungeon|Minimap")
	TArray<FETHRoomDebugData> GetMinimapData() const;

	UFUNCTION(BlueprintPure, Category = "Dungeon")
	int32 GetCurrentRoomID() const { return CurrentRoomID; }

	UFUNCTION(BlueprintPure, Category = "Dungeon")
	int32 GetPreviousRoomID() const { return PreviousRoomID; }

	UFUNCTION(BlueprintPure, Category = "Dungeon")
	int32 GetReadyRoomsCount() const { return ReadyRoomsCount; }

	// -------------------------------------------------------
	// Cheat / Debug API
	// -------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Dungeon|Debug")
	void ForceRoomState(int32 RoomID, EETHRoomState State);

	UFUNCTION(BlueprintCallable, Category = "Dungeon|Debug")
	void PrintAllRooms();

	UFUNCTION(BlueprintCallable, Category = "Dungeon|Debug")
	void ClearAllRooms();

	UFUNCTION(BlueprintCallable, Category = "Dungeon|Debug")
	void UnlockAllRooms();

	UFUNCTION(BlueprintCallable, Category = "Dungeon|Debug")
	void DiscoverAllRooms();

	// -------------------------------------------------------
	// BlueprintImplementableEvents (implement in BP_DungeonManager)
	// -------------------------------------------------------

	/** Called after any room state change — wire to UpdateDoors(), minimap, HUD. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Dungeon")
	void BP_OnRoomStateChanged(int32 RoomID, EETHRoomState NewState);

	/** Called when the player moves to a different room — camera, music, minimap. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Dungeon")
	void BP_OnRoomChanged(int32 NewRoomID, int32 PrevRoomID);

	/** Called once when ReadyRoomsCount == TotalExpectedRooms — open start door, begin ambience. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Dungeon")
	void BP_OnAllRoomsReady();

	/**
	 * Called on every room adjacent to the one that just changed state.
	 * RoomID       = the room that needs to re-evaluate its shared door with NeighborID.
	 * NeighborID   = the room whose state just changed.
	 * NeighborState = the new state of that neighbor.
	 * Implement in BP_DungeonManager to call UpdateSharedDoor on the wrapper.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Dungeon|Doors")
	void BP_OnNeighborStateChanged(int32 RoomID, int32 NeighborID, EETHRoomState NeighborState);

	// -------------------------------------------------------
	// Door Connection API (new centralised door system)
	// -------------------------------------------------------

	/**
	 * Enregistre une connexion logique entre deux salles (sans acteur porte).
	 * Appelé par BP_DungeonGeneratorNEW quand deux connecteurs matchent.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Doors")
	void RegisterPendingDoorConnection(int32 RoomID_A, int32 RoomID_B, FVector ConnectorWorldPos);

	/**
	 * Associe un acteur porte à une connexion existante par proximité.
	 * Appelé par BP_RoomWrapper après le chargement du sublevel.
	 * Déclenche immédiatement l'évaluation de l'état de la porte.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Doors")
	void AssignDoorActorToConnection(int32 RoomID, AActor* DoorActor,
	                                  FVector DoorWorldPos, float Tolerance = 500.f);

	/** Retourne true si la porte entre A et B doit être ouverte (AND des deux états). */
	UFUNCTION(BlueprintPure, Category = "Dungeon|Doors")
	bool ShouldDoorBeOpen(int32 RoomID_A, int32 RoomID_B) const;

	/**
	 * Implémenté dans BP_DungeonManager.
	 * Applique l'animation d'ouverture/fermeture sur l'acteur porte.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Dungeon|Doors")
	void BP_SetDoorOpen(AActor* DoorActor, bool bOpen);

protected:
	// -------------------------------------------------------
	// Tracked state (BlueprintReadOnly so BP can read them)
	// -------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	int32 CurrentRoomID = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	int32 PreviousRoomID = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	int32 ReadyRoomsCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	int32 TotalExpectedRooms = 0;

private:
	// -------------------------------------------------------
	// Data store
	// -------------------------------------------------------

	TMap<int32, FETHRoomEntry> Rooms;

	/** Adjacency links added before a room is registered — flushed on RegisterRoom. */
	TMap<int32, TArray<int32>> PendingAdjacency;

	/** All registered door connections (bidirectional). Source of truth for door states. */
	TArray<FETHDoorConnection> DoorConnections;

	bool bAllRoomsReadyFired = false;

	// -------------------------------------------------------
	// Internal helpers
	// -------------------------------------------------------

	/** Single choke point for all state transitions. */
	void SetRoomState(int32 RoomID, EETHRoomState NewState);

	/** Discover adjacent rooms that are currently Undiscovered (and not Locked type). */
	void DiscoverAdjacentRooms(int32 RoomID);

	/** Rebuild debugger data and push to DebuggerRef if assigned. */
	void PushDebuggerUpdate();

	/** Check whether ReadyRoomsCount reached TotalExpectedRooms and fire BP_OnAllRoomsReady once. */
	void CheckAllRoomsReady();

	/** Returns true for states where a door should be open (Discovered, Active, Cleared). */
	static bool IsOpenState(EETHRoomState State);

	/** Re-evaluate and apply open/close for all door connections involving RoomID. */
	void RefreshDoorsForRoom(int32 RoomID);
};
