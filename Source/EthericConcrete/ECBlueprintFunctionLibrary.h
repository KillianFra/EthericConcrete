#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemComponent.h"
#include "ECBlueprintFunctionLibrary.generated.h"

/**
 * Custom Blueprint toolbox for Etheric Concrete
 */
UCLASS()
class ETHERICCONCRETE_API UECBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Exposes the CancelAbilities function to Blueprints
	UFUNCTION(BlueprintCallable, Category = "EC|Abilities")
	static void CancelAbilitiesByTag(UAbilitySystemComponent* ASC, FGameplayTagContainer WithTags);
};