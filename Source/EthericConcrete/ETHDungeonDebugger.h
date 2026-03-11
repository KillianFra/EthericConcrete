// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ETHDungeonDebugger.generated.h"

USTRUCT(BlueprintType)
struct FETHRoomDebugData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Debug")
	int32 RoomID = -1;

	/** E_RoomState cast to byte: 0=Undiscovered, 1=Discovered, 2=Active, 3=Combat, 4=Cleared, 5=Locked, 6=Boss */
	UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Debug")
	uint8 RoomState = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Debug")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Debug")
	TArray<int32> AdjacentRoomIDs;

	UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Debug")
	int32 DoorCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Debug")
	TArray<FVector> ConnectorWorldLocations;
};

UCLASS()
class ETHERICCONCRETE_API AETHDungeonDebugger : public AActor
{
	GENERATED_BODY()

public:
	AETHDungeonDebugger();

	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------
	// Blueprint-callable API (called by BP_DungeonManager)
	// -------------------------------------------------------

	/** Main update — call on BeginPlay, OnRoomStateChanged, and OnRoomChanged. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Debug")
	void UpdateRooms(const TArray<FETHRoomDebugData>& Rooms,
	                 int32 InCurrentRoomID,
	                 int32 InPreviousRoomID,
	                 int32 InReadyCount,
	                 int32 InTotalCount);

	/** Toggle the entire overlay at runtime. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Debug")
	void SetEnabled(bool bNewEnabled);

	// -------------------------------------------------------
	// Display toggles (configurable in-editor)
	// -------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Debug")
	bool bShowInWorldSpheres = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Debug")
	bool bShowStateLabels = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Debug")
	bool bShowAdjacencyLines = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Debug")
	bool bShowConnectorDots = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Debug")
	bool bShowHUDPanel = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Debug")
	float SphereRadius = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Debug")
	float LabelHeightOffset = 350.f;

protected:
	virtual void BeginPlay() override;

private:
	// -------------------------------------------------------
	// Cached state
	// -------------------------------------------------------

	TArray<FETHRoomDebugData> CachedRooms;
	int32 CurrentRoomID  = -1;
	int32 PreviousRoomID = -1;
	int32 ReadyCount     = 0;
	int32 TotalCount     = 0;
	bool  bEnabled       = true;

	// -------------------------------------------------------
	// Internal draw helpers
	// -------------------------------------------------------

	void DrawWorldDebug() const;
	void DrawHUDPanel()   const;

	static FColor GetColorForState(uint8 State);
	static FString GetNameForState(uint8 State);
};
