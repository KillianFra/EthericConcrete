// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ETHAttributeSet.generated.h"

// Macro to easily declare attributes
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class ETHERICCONCRETE_API UETHAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	// Constructor
	UETHAttributeSet();

	/** ============= Anomaly (ANM) ============= */
	UPROPERTY(BlueprintReadOnly, Category = "Anomaly", ReplicatedUsing = OnRep_Anomaly)
	FGameplayAttributeData Anomaly;
	ATTRIBUTE_ACCESSORS(UETHAttributeSet, Anomaly);

	/** ============= Corpus (COR) ============= */
	// Health is bounded, so it has Current + Max
	UPROPERTY(BlueprintReadOnly, Category = "Corpus", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UETHAttributeSet, Health);

	UPROPERTY(BlueprintReadOnly, Category = "Corpus", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UETHAttributeSet, MaxHealth);

	UPROPERTY(BlueprintReadOnly, Category = "Corpus", ReplicatedUsing = OnRep_Armor)
	FGameplayAttributeData Armor;
	ATTRIBUTE_ACCESSORS(UETHAttributeSet, Armor);

	UPROPERTY(BlueprintReadOnly, Category = "Corpus", ReplicatedUsing = OnRep_WeaponDamage)
	FGameplayAttributeData WeaponDamage;
	ATTRIBUTE_ACCESSORS(UETHAttributeSet, WeaponDamage);

	/** ============= Kinetic (KIN) ============= */
	// Move Speed has Base + Max (for clamping buffs/debuffs)
	UPROPERTY(BlueprintReadOnly, Category = "Kinetic", ReplicatedUsing = OnRep_MoveSpeed)
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(UETHAttributeSet, MoveSpeed);

	UPROPERTY(BlueprintReadOnly, Category = "Kinetic", ReplicatedUsing = OnRep_MaxMoveSpeed)
	FGameplayAttributeData MaxMoveSpeed;
	ATTRIBUTE_ACCESSORS(UETHAttributeSet, MaxMoveSpeed);

	UPROPERTY(BlueprintReadOnly, Category = "Kinetic", ReplicatedUsing = OnRep_WeaponAttackSpeed)
	FGameplayAttributeData WeaponAttackSpeed;
	ATTRIBUTE_ACCESSORS(UETHAttributeSet, WeaponAttackSpeed);

	UPROPERTY(BlueprintReadOnly, Category = "Kinetic", ReplicatedUsing = OnRep_MaxWeaponAttackSpeed)
	FGameplayAttributeData MaxWeaponAttackSpeed;
	ATTRIBUTE_ACCESSORS(UETHAttributeSet, MaxWeaponAttackSpeed);

	/** ============= Focus (FCS) ============= */
	UPROPERTY(BlueprintReadOnly, Category = "Focus", ReplicatedUsing = OnRep_DistanceWeaponAccuracy)
	FGameplayAttributeData DistanceWeaponAccuracy;
	ATTRIBUTE_ACCESSORS(UETHAttributeSet, DistanceWeaponAccuracy);

	UPROPERTY(BlueprintReadOnly, Category = "Focus", ReplicatedUsing = OnRep_CriticalChance)
	FGameplayAttributeData CriticalChance;
	ATTRIBUTE_ACCESSORS(UETHAttributeSet, CriticalChance);

	/** ============= Synapse (SYN) ============= */
	UPROPERTY(BlueprintReadOnly, Category = "Synapse", ReplicatedUsing = OnRep_SpellDamage)
	FGameplayAttributeData SpellDamage;
	ATTRIBUTE_ACCESSORS(UETHAttributeSet, SpellDamage);

	UPROPERTY(BlueprintReadOnly, Category = "Synapse", ReplicatedUsing = OnRep_SpellCooldownReduction)
	FGameplayAttributeData SpellCooldownReduction;
	ATTRIBUTE_ACCESSORS(UETHAttributeSet, SpellCooldownReduction);

	/** ============= Echo (ECH) ============= */
	UPROPERTY(BlueprintReadOnly, Category = "Echo", ReplicatedUsing = OnRep_CriticalDamage)
	FGameplayAttributeData CriticalDamage;
	ATTRIBUTE_ACCESSORS(UETHAttributeSet, CriticalDamage);

	UPROPERTY(BlueprintReadOnly, Category = "Echo", ReplicatedUsing = OnRep_SpellCooldownReduction_Echo)
	FGameplayAttributeData SpellCooldownReduction_Echo;
	ATTRIBUTE_ACCESSORS(UETHAttributeSet, SpellCooldownReduction_Echo);

	/** ============= Replication ============= */
protected:
	UFUNCTION() virtual void OnRep_Anomaly(const FGameplayAttributeData& OldValue);

	UFUNCTION() virtual void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION() virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION() virtual void OnRep_Armor(const FGameplayAttributeData& OldValue);
	UFUNCTION() virtual void OnRep_WeaponDamage(const FGameplayAttributeData& OldValue);

	UFUNCTION() virtual void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION() virtual void OnRep_MaxMoveSpeed(const FGameplayAttributeData& OldValue);

	UFUNCTION() virtual void OnRep_WeaponAttackSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION() virtual void OnRep_MaxWeaponAttackSpeed(const FGameplayAttributeData& OldValue);

	UFUNCTION() virtual void OnRep_DistanceWeaponAccuracy(const FGameplayAttributeData& OldValue);
	UFUNCTION() virtual void OnRep_CriticalChance(const FGameplayAttributeData& OldValue);

	UFUNCTION() virtual void OnRep_SpellDamage(const FGameplayAttributeData& OldValue);
	UFUNCTION() virtual void OnRep_SpellCooldownReduction(const FGameplayAttributeData& OldValue);

	UFUNCTION() virtual void OnRep_CriticalDamage(const FGameplayAttributeData& OldValue);
	UFUNCTION() virtual void OnRep_SpellCooldownReduction_Echo(const FGameplayAttributeData& OldValue);

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
