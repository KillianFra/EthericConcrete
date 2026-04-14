// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "ECAbilitySystemComponent.generated.h"

/**
 * 
 */
UCLASS()
class ETHERICCONCRETE_API UECAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

protected:
	TArray<FGameplayAbilitySpec> LastActivatableAbilities;



protected:

	void OnRep_ActivateAbilities() override;	
	
};
