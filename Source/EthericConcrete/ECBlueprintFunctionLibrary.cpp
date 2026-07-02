#include "ECBlueprintFunctionLibrary.h"

void UECBlueprintFunctionLibrary::CancelAbilitiesByTag(UAbilitySystemComponent* ASC, FGameplayTagContainer WithTags)
{
	if (ASC)
	{
		// We pass the address of our Tag Container, leave WithoutTags null, and Ignore ability null.
		ASC->CancelAbilities(&WithTags, nullptr, nullptr);
	}
}