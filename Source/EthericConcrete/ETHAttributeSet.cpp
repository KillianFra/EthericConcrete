// ETHAttributeSet.cpp

#include "ETHAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UETHAttributeSet::UETHAttributeSet()
{
    Health = 100.0f;
    MaxHealth = 100.0f;

    MoveSpeed = 600.0f;
    MaxMoveSpeed = 1200.0f;

    WeaponAttackSpeed = 1.0f;
    MaxWeaponAttackSpeed = 5.0f;

    Armor = 0.0f;
    WeaponDamage = 10.0f;

    Anomaly = 1;
    DistanceWeaponAccuracy = 1.0f;
    CriticalChance = 0.1f; // 10%
    CriticalDamage = 1.5f; // 150%

    SpellDamage = 10.0f;
    SpellCooldownReduction = 0.0f;
    SpellCooldownReduction_Echo = 0.0f;
}

/** ================= Replication ================= */
void UETHAttributeSet::OnRep_Anomaly(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UETHAttributeSet, Anomaly, OldValue);
}

void UETHAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UETHAttributeSet, Health, OldValue);
}

void UETHAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UETHAttributeSet, MaxHealth, OldValue);
}

void UETHAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UETHAttributeSet, Armor, OldValue);
}

void UETHAttributeSet::OnRep_WeaponDamage(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UETHAttributeSet, WeaponDamage, OldValue);
}

void UETHAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UETHAttributeSet, MoveSpeed, OldValue);
}

void UETHAttributeSet::OnRep_MaxMoveSpeed(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UETHAttributeSet, MaxMoveSpeed, OldValue);
}

void UETHAttributeSet::OnRep_WeaponAttackSpeed(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UETHAttributeSet, WeaponAttackSpeed, OldValue);
}

void UETHAttributeSet::OnRep_MaxWeaponAttackSpeed(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UETHAttributeSet, MaxWeaponAttackSpeed, OldValue);
}

void UETHAttributeSet::OnRep_DistanceWeaponAccuracy(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UETHAttributeSet, DistanceWeaponAccuracy, OldValue);
}

void UETHAttributeSet::OnRep_CriticalChance(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UETHAttributeSet, CriticalChance, OldValue);
}

void UETHAttributeSet::OnRep_SpellDamage(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UETHAttributeSet, SpellDamage, OldValue);
}

void UETHAttributeSet::OnRep_SpellCooldownReduction(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UETHAttributeSet, SpellCooldownReduction, OldValue);
}

void UETHAttributeSet::OnRep_CriticalDamage(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UETHAttributeSet, CriticalDamage, OldValue);
}

void UETHAttributeSet::OnRep_SpellCooldownReduction_Echo(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UETHAttributeSet, SpellCooldownReduction_Echo, OldValue);
}

/** ================= Clamping & Attribute Hooks ================= */
void UETHAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetHealthAttribute())
    {
        // Clamp Health to [0, MaxHealth]
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
    }
    else if (Attribute == GetMoveSpeedAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxMoveSpeed());
    }
    else if (Attribute == GetWeaponAttackSpeedAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxWeaponAttackSpeed());
    }
    else if (Attribute == GetCriticalChanceAttribute())
    {
        // Clamp Crit chance between 0–1 (0%–100%)
        NewValue = FMath::Clamp(NewValue, 0.0f, 1.0f);
    }
    else if (Attribute == GetSpellCooldownReductionAttribute() || Attribute == GetSpellCooldownReduction_EchoAttribute())
    {
        // Clamp Cooldown Reduction between 0–1 (0%–100%)
        NewValue = FMath::Clamp(NewValue, 0.0f, 1.0f);
    }
}

void UETHAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetHealthAttribute())
    {
        SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
    }
    else if (Data.EvaluatedData.Attribute == GetMoveSpeedAttribute())
    {
        SetMoveSpeed(FMath::Clamp(GetMoveSpeed(), 0.0f, GetMaxMoveSpeed()));
    }
    else if (Data.EvaluatedData.Attribute == GetWeaponAttackSpeedAttribute())
    {
        SetWeaponAttackSpeed(FMath::Clamp(GetWeaponAttackSpeed(), 0.0f, GetMaxWeaponAttackSpeed()));
    }
}

void UETHAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION_NOTIFY(UETHAttributeSet, Anomaly, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UETHAttributeSet, Health, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UETHAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UETHAttributeSet, Armor, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UETHAttributeSet, WeaponDamage, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UETHAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UETHAttributeSet, MaxMoveSpeed, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UETHAttributeSet, WeaponAttackSpeed, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UETHAttributeSet, MaxWeaponAttackSpeed, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UETHAttributeSet, DistanceWeaponAccuracy, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UETHAttributeSet, CriticalChance, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UETHAttributeSet, SpellDamage, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UETHAttributeSet, SpellCooldownReduction, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UETHAttributeSet, CriticalDamage, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UETHAttributeSet, SpellCooldownReduction_Echo, COND_None, REPNOTIFY_Always);
}
