// Fill out your copyright notice in the Description page of Project Settings.


#include "Protagonist.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "ETHAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "ECAbilitySystemComponent.h"

// Sets default values
AProtagonist::AProtagonist()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	AbilitySystemComponent = CreateDefaultSubobject<UECAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UETHAttributeSet>(TEXT("AttributeSet"));
}

// Return the ASC
UAbilitySystemComponent* AProtagonist::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

// Called when the game starts or when spawned
void AProtagonist::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystemComponent && AttributeSet)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute()).AddUObject(this, &AProtagonist::HandleHealthChanged);
		// Register anomaly change delegate
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetAnomalyAttribute()).AddUObject(this, &AProtagonist::HandleAnomalyChanged);
		//Pas du tout sur si j'dois mettre cette fonction là mais lui il la met sous un "PossessedBy" sauf qu'on a pas de possessedBy et lui il pense au multi avant tout
		//Mais on a pas de multi nous donc je sais pas mec j'commence à paniquer aarrrggghhh
		GrantAbilities(StartingAbility);
	}
	
}


void AProtagonist::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	float newHealth = Data.NewValue;
	float OldHealth = Data.OldValue;

	float DeltaValue = newHealth - OldHealth;

	OnHealthChanged(DeltaValue, FGameplayTagContainer());
}

void AProtagonist::HandleAnomalyChanged(const FOnAttributeChangeData& Data)
{
	int32 newAnomaly = static_cast<int32>(Data.NewValue);
	int32 OldAnomaly = static_cast<int32>(Data.OldValue);

	OnAnomalyChanged(OldAnomaly, newAnomaly, FGameplayTagContainer());
}

void AProtagonist::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// Optionally add custom tick logic here
}

void AProtagonist::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// Optionally bind input here
}


//Ali GAS Part 8 : Ajouter une fonction qui permet de grant des ability depuis un array

TArray<FGameplayAbilitySpecHandle> AProtagonist::GrantAbilities(
	TArray<TSubclassOf<UGameplayAbility>> AbilitiesToGrant)
{
	if (!AbilitySystemComponent || !HasAuthority())
	{
		return TArray<FGameplayAbilitySpecHandle>();
	}

	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	
	for (TSubclassOf<UGameplayAbility> Ability : AbilitiesToGrant)
	{

		FGameplayAbilitySpecHandle SpecHandle = AbilitySystemComponent->GiveAbility(
			FGameplayAbilitySpec(Ability, 1, -1, this
		));
		AbilityHandles.Add(SpecHandle); 

	}
	SendAbilitiesChangedEvent();
	return AbilityHandles;
}

void AProtagonist::RemoveAbilities(TArray<FGameplayAbilitySpecHandle> AbilityHandlesToRemove)
{
	if (!AbilitySystemComponent || !HasAuthority())
	{
		return;
	}

	for (FGameplayAbilitySpecHandle AbilityHandle : AbilityHandlesToRemove)
	{
		AbilitySystemComponent->ClearAbility(AbilityHandle);
	}

	SendAbilitiesChangedEvent();
}

void AProtagonist::SendAbilitiesChangedEvent()
{
	FGameplayEventData EventData;
	EventData.EventTag = FGameplayTag::RequestGameplayTag(FName("Event.AbilitiesChanged"));
	EventData.Instigator = this;
	EventData.Target = this;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, EventData.EventTag, EventData);

}