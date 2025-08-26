// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "ETHAttributeSet.h"
#include "Protagonist.generated.h"

UCLASS()
class ETHERICCONCRETE_API AProtagonist : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AProtagonist();

	// Implement the IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnHealthChanged(float DeltaValue, const FGameplayTagContainer& EventTags);

	UFUNCTION(BlueprintImplementableEvent, Category = "Anomaly")
	void OnAnomalyChanged(int32 OldAnomaly, int32 NewAnomaly, const FGameplayTagContainer& EventTags);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// ASC that manages attributes and effects
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	UAbilitySystemComponent* AbilitySystemComponent;

	// Attribute Set that stores and manages health and other attributes, marked for replication
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	UETHAttributeSet* AttributeSet;

	void InitializeAttributes();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;


private:
	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	void HandleAnomalyChanged(const FOnAttributeChangeData& Data);
};
