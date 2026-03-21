// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ETHDungeonManager.h"
#include "ETHMinimapWidget.generated.h"

/**
 * Binding-of-Isaac style minimap.
 *
 * Each registered room is a solid colored square.
 * Rooms are laid out on a 2D integer grid via a BFS traversal of the
 * dungeon adjacency graph — direction (N/S/E/W) is inferred from
 * world-space position deltas, so branches spread in 2D naturally.
 * No connector lines are drawn; adjacent squares sit close together.
 *
 * Setup:
 *   1. Create WBP_Minimap as a Blueprint child of this class.
 *   2. In Protagonist BeginPlay (Is Locally Controlled):
 *        Create Widget → Add to Viewport
 *        → Set Desired Size In Viewport (e.g. 280, 280)
 *        → Set Position In Viewport (ViewportSize.X - 290, 10)
 */
UCLASS()
class ETHERICCONCRETE_API UETHMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Size of each room square in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
	float RoomBoxSize = 22.f;

	/** Gap between adjacent room squares in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
	float CellGap = 5.f;

	/** Padding around the entire grid in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
	float MapPadding = 14.f;

	/** If false, Undiscovered rooms are hidden entirely. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
	bool bShowUndiscovered = true;

	/** Explicit manager reference — auto-found via TObjectIterator if null. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Minimap")
	TObjectPtr<AETHDungeonManager> DungeonManager;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct()  override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual int32 NativePaint(
		const FPaintArgs&        Args,
		const FGeometry&         AllottedGeometry,
		const FSlateRect&        MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32                    LayerId,
		const FWidgetStyle&      InWidgetStyle,
		bool                     bParentEnabled) const override;

private:
	UFUNCTION() void HandleRoomStateChanged(int32 RoomID, EETHRoomState NewState);
	UFUNCTION() void HandleRoomChanged(int32 NewRoomID, int32 PreviousRoomID);

	void TryFindDungeonManager();
	static FLinearColor GetColorForState(EETHRoomState State);
};
