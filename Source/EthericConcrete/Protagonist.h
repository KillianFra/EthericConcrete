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


//Ali Tuto Part 8 : Ajouter une fonction qui permet de grant des ability
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilitySystem")
	TArray<TSubclassOf<UGameplayAbility>> StartingAbility;

	
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

	//Ali Tuto Part 8 : Ajouter une fonction qui permet de grant des ability depuis un array
	UFUNCTION(BlueprintCallable,Category = "AbilitySystem")
	TArray <FGameplayAbilitySpecHandle> GrantAbilities(TArray<TSubclassOf<UGameplayAbility>> AbilitiesToGrant);

	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	void RemoveAbilities(TArray<FGameplayAbilitySpecHandle> AbilitiesHandlesToRemove);

	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	void SendAbilitiesChangedEvent();


private:
	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	void HandleAnomalyChanged(const FOnAttributeChangeData& Data);
};
