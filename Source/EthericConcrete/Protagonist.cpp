// Fill out your copyright notice in the Description page of Project Settings.


#include "Protagonist.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "ETHAttributeSet.h"
#include "Net/UnrealNetwork.h"

// Sets default values
AProtagonist::AProtagonist()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
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
