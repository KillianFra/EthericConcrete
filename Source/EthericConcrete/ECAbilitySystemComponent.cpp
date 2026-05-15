// Fill out your copyright notice in the Description page of Project Settings.


#include "ECAbilitySystemComponent.h"
#include "Protagonist.h"


void UECAbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();
	// Custom logic to handle ability activation replication can be added here
	AProtagonist* Character = Cast<AProtagonist>(GetOwner());

	if (!Character) return;

	
	bool bAbilitiesChanged = false;
	if (LastActivatableAbilities.Num() != ActivatableAbilities.Items.Num())
	{
		Character->SendAbilitiesChangedEvent();
		LastActivatableAbilities = ActivatableAbilities.Items;
	}
	else
	{
		for (int32 i = 0; i < LastActivatableAbilities.Num(); ++i)
		{
			if (LastActivatableAbilities[i].Ability != ActivatableAbilities.Items[i].Ability)
			{
				bAbilitiesChanged = true;	
				break;
			}
		}
	}

	if (bAbilitiesChanged)
	{
		Character->SendAbilitiesChangedEvent();
		LastActivatableAbilities = ActivatableAbilities.Items;
	}	
	
}

