// Fill out your copyright notice in the Description page of Project Settings.

#include "ETHLevelLibrary.h"
#include "Engine/Level.h"

TArray<AActor*> UETHLevelLibrary::GetActorsInLevel(ULevel* Level)
{
	TArray<AActor*> Result;

	if (!Level)
	{
		return Result;
	}

	// ULevel::Actors is the authoritative actor list for the level at runtime.
	for (AActor* Actor : Level->Actors)
	{
		if (Actor)
		{
			Result.Add(Actor);
		}
	}

	return Result;
}
