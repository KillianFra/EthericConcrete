// Fill out your copyright notice in the Description page of Project Settings.

#include "ETHMinimapWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UETHMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	TryFindDungeonManager();
}

void UETHMinimapWidget::NativeDestruct()
{
	if (DungeonManager)
	{
		DungeonManager->OnRoomStateChanged.RemoveDynamic(this, &UETHMinimapWidget::HandleRoomStateChanged);
		DungeonManager->OnRoomChanged.RemoveDynamic(this, &UETHMinimapWidget::HandleRoomChanged);
	}
	Super::NativeDestruct();
}

void UETHMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!DungeonManager)
	{
		TryFindDungeonManager();
	}
}

void UETHMinimapWidget::TryFindDungeonManager()
{
	if (DungeonManager) { return; }
	UWorld* World = GetWorld();
	if (!World)         { return; }

	for (TObjectIterator<AETHDungeonManager> It; It; ++It)
	{
		if (It->GetWorld() == World)
		{
			DungeonManager = *It;
			DungeonManager->OnRoomStateChanged.AddDynamic(this, &UETHMinimapWidget::HandleRoomStateChanged);
			DungeonManager->OnRoomChanged.AddDynamic(this, &UETHMinimapWidget::HandleRoomChanged);
			return;
		}
	}
}

void UETHMinimapWidget::HandleRoomStateChanged(int32 RoomID, EETHRoomState NewState)
{
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UETHMinimapWidget::HandleRoomChanged(int32 NewRoomID, int32 PreviousRoomID)
{
	Invalidate(EInvalidateWidgetReason::Paint);
}

// ---------------------------------------------------------------------------
// Color palette
// ---------------------------------------------------------------------------

FLinearColor UETHMinimapWidget::GetColorForState(EETHRoomState State)
{
	switch (State)
	{
	case EETHRoomState::Undiscovered: return FLinearColor(0.15f, 0.15f, 0.18f, 1.f);
	case EETHRoomState::Discovered:   return FLinearColor(0.20f, 0.40f, 0.80f, 1.f);
	case EETHRoomState::Active:       return FLinearColor(0.20f, 0.80f, 0.20f, 1.f);
	case EETHRoomState::Combat:       return FLinearColor(0.85f, 0.10f, 0.10f, 1.f);
	case EETHRoomState::Cleared:      return FLinearColor(0.00f, 0.75f, 0.85f, 1.f);
	case EETHRoomState::Locked:       return FLinearColor(0.85f, 0.78f, 0.00f, 1.f);
	case EETHRoomState::Boss:         return FLinearColor(0.55f, 0.10f, 0.80f, 1.f);
	default:                          return FLinearColor::White;
	}
}

// ---------------------------------------------------------------------------
// Internal draw helper — uses FCoreStyle WhiteTexture (works in game runtime)
// ---------------------------------------------------------------------------

static void PaintBox(
	FSlateWindowElementList& OutDrawElements,
	int32                    LayerId,
	const FGeometry&         Geo,
	FVector2D                Pos,
	FVector2D                Size,
	FLinearColor             Color)
{
	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("WhiteTexture");
	if (!Brush) { return; }

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		Geo.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)),
		Brush,
		ESlateDrawEffect::None,
		Color);
}

// ---------------------------------------------------------------------------
// NativePaint
// ---------------------------------------------------------------------------

int32 UETHMinimapWidget::NativePaint(
	const FPaintArgs&        Args,
	const FGeometry&         AllottedGeometry,
	const FSlateRect&        MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32                    LayerId,
	const FWidgetStyle&      InWidgetStyle,
	bool                     bParentEnabled) const
{
	if (!DungeonManager) { return LayerId; }

	const TArray<FETHRoomDebugData> Rooms = DungeonManager->GetMinimapData();
	if (Rooms.Num() == 0) { return LayerId; }

	const int32     CurrentRoomID = DungeonManager->GetCurrentRoomID();
	const FVector2D LocalSize     = AllottedGeometry.GetLocalSize();

	// -------------------------------------------------------
	// 1. Build lookup by ID
	// -------------------------------------------------------
	TMap<int32, const FETHRoomDebugData*> RoomByID;
	for (const FETHRoomDebugData& R : Rooms)
	{
		RoomByID.Add(R.RoomID, &R);
	}

	// -------------------------------------------------------
	// 2. BFS tree layout — gives each room a 2D integer grid cell
	//    Direction is derived from world-space delta so the layout
	//    matches the actual dungeon topology (N/S/E/W branches).
	// -------------------------------------------------------
	TMap<int32, FIntPoint> GridCoords;
	TSet<FIntPoint>        Occupied;
	TQueue<int32>          Queue;
	TSet<int32>            Visited;

	// Root = lowest room ID (Start room is always registered first)
	int32 RootID = Rooms[0].RoomID;
	for (const FETHRoomDebugData& R : Rooms)
	{
		if (R.RoomID < RootID) { RootID = R.RoomID; }
	}

	GridCoords.Add(RootID, FIntPoint(0, 0));
	Occupied.Add(FIntPoint(0, 0));
	Queue.Enqueue(RootID);
	Visited.Add(RootID);

	while (!Queue.IsEmpty())
	{
		int32 CurID;
		Queue.Dequeue(CurID);

		const FETHRoomDebugData* Cur = RoomByID[CurID];
		const FIntPoint          CG  = GridCoords[CurID];

		for (int32 AdjID : Cur->AdjacentRoomIDs)
		{
			if (Visited.Contains(AdjID)) { continue; }
			const FETHRoomDebugData** AdjPtr = RoomByID.Find(AdjID);
			if (!AdjPtr)               { continue; }

			Visited.Add(AdjID);

			// Snap world delta to cardinal direction
			const float      DX  = (*AdjPtr)->WorldLocation.X - Cur->WorldLocation.X;
			const float      DY  = (*AdjPtr)->WorldLocation.Y - Cur->WorldLocation.Y;
			FIntPoint        Dir = (FMath::Abs(DX) >= FMath::Abs(DY))
			                          ? FIntPoint((DX >= 0.f) ? 1 : -1, 0)
			                          : FIntPoint(0, (DY >= 0.f) ? 1 : -1);

			// Find first free cell starting from the preferred one,
			// spiralling outward along the perpendicular if needed
			FIntPoint        Target = CG + Dir;
			const FIntPoint  Perp(Dir.Y, Dir.X);

			for (int32 Spread = 0; Spread <= 10 && Occupied.Contains(Target); ++Spread)
			{
				if (Spread == 0) { Target = CG + Dir * 2; continue; }
				Target = CG + Dir + Perp *  Spread; if (!Occupied.Contains(Target)) break;
				Target = CG + Dir + Perp * -Spread; if (!Occupied.Contains(Target)) break;
			}

			GridCoords.Add(AdjID, Target);
			Occupied.Add(Target);
			Queue.Enqueue(AdjID);
		}
	}

	// -------------------------------------------------------
	// 3. Pixel metrics — current room is always at widget center
	// -------------------------------------------------------

	// Fixed cell size: room square + gap. No scaling needed since
	// rooms outside the widget bounds are simply clipped.
	const float CellSize = RoomBoxSize + CellGap;
	const float RoomPx   = RoomBoxSize;
	const float HalfPx   = RoomPx * 0.5f;

	// Anchor: the current room sits at the exact center of the widget.
	// Fall back to root if the player hasn't entered any room yet.
	const FIntPoint* AnchorGrid = GridCoords.Find(CurrentRoomID);
	if (!AnchorGrid) { AnchorGrid = GridCoords.Find(RootID); }
	if (!AnchorGrid) { return LayerId; }

	const FVector2D WidgetCenter = LocalSize * 0.5f;

	auto ToScreen = [&](const FIntPoint& G) -> FVector2D
	{
		return WidgetCenter + FVector2D(
			(G.X - AnchorGrid->X) * CellSize,
			(G.Y - AnchorGrid->Y) * CellSize);
	};

	// Visible range: only draw rooms whose center falls inside the widget
	// (with a one-cell margin so partially visible rooms still render)
	auto IsVisible = [&](const FVector2D& Center) -> bool
	{
		return Center.X > -CellSize && Center.X < LocalSize.X + CellSize
			&& Center.Y > -CellSize && Center.Y < LocalSize.Y + CellSize;
	};

	// -------------------------------------------------------
	// 4. Draw background
	// -------------------------------------------------------
	PaintBox(OutDrawElements, LayerId, AllottedGeometry,
		FVector2D::ZeroVector, LocalSize,
		FLinearColor(0.04f, 0.04f, 0.08f, 0.92f));
	++LayerId;

	// -------------------------------------------------------
	// 5. Draw rooms — colored squares, no connector lines
	// -------------------------------------------------------
	for (const FETHRoomDebugData& R : Rooms)
	{
		const EETHRoomState  State = (EETHRoomState)R.RoomState;
		if (!bShowUndiscovered && State == EETHRoomState::Undiscovered) { continue; }

		const FIntPoint* G = GridCoords.Find(R.RoomID);
		if (!G) { continue; }

		const FVector2D Center = ToScreen(*G);
		if (!IsVisible(Center)) { continue; }
		const FVector2D Pos(Center.X - HalfPx, Center.Y - HalfPx);
		const FVector2D Sz(RoomPx, RoomPx);

		// White border on the current room
		if (R.RoomID == CurrentRoomID)
		{
			const float B = 3.f;
			PaintBox(OutDrawElements, LayerId, AllottedGeometry,
				Pos - FVector2D(B, B), Sz + FVector2D(B * 2.f, B * 2.f),
				FLinearColor::White);
			++LayerId;
		}

		PaintBox(OutDrawElements, LayerId, AllottedGeometry,
			Pos, Sz, GetColorForState(State));
	}
	++LayerId;

	return LayerId;
}
