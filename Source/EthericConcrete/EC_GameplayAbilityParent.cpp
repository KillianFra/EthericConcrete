// Fill out your copyright notice in the Description page of Project Settings.

#include "EC_GameplayAbilityParent.h"

UEC_GameplayAbilityParent::UEC_GameplayAbilityParent()
{
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("GA.Active")));
}