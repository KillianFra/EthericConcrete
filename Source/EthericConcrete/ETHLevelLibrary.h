// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ETHLevelLibrary.generated.h"

UCLASS()
class ETHERICCONCRETE_API UETHLevelLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns all valid actors contained in the given level. Returns empty array if Level is null. */
	UFUNCTION(BlueprintCallable, Category = "ETH|Level")
	static TArray<AActor*> GetActorsInLevel(ULevel* Level);
};
