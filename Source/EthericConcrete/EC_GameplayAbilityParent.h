// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "EC_GameplayAbilityParent.generated.h"

/**
 * 
 */
UCLASS()
class ETHERICCONCRETE_API UEC_GameplayAbilityParent : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	bool ShouldShowInAbilitiesBar = false;
	
};
