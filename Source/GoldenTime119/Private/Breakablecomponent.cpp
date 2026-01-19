// ============================ BreakableComponent.cpp ==========================
#include "BreakableComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogBreakable, Log, All);

UBreakableComponent::UBreakableComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UBreakableComponent::BeginPlay()
{
    Super::BeginPlay();

    CurrentHP = MaxHP;
    BreakState = EBreakableState::Intact;
    bHasBroken = false;

    UE_LOG(LogBreakable, Log, TEXT("[Breakable] %s initialized - HP:%.1f Material:%d RequiredTool:%d"),
        *GetNameSafe(GetOwner()), MaxHP, (int32)Material, (int32)RequiredTool);
}

float UBreakableComponent::ApplyDamage(float BaseDamage, EBreakToolType ToolUsed, FVector HitLocation, FVector HitNormal)
{
    if (IsBroken() || bHasBroken)
        return 0.f;

    // 도구 적합성에 따른 데미지 계산
    float DamageMultiplier = 1.0f;

    if (!IsCorrectTool(ToolUsed))
    {
        DamageMultiplier = WrongToolDamageMultiplier;
        UE_LOG(LogBreakable, Log, TEXT("[Breakable] Wrong tool used! Damage reduced to %.0f%%"),
            WrongToolDamageMultiplier * 100.f);
    }
    else
    {
        // 재질별 도구 효율
        DamageMultiplier = GetToolEffectiveness(ToolUsed);
    }

    const float FinalDamage = BaseDamage * DamageMultiplier;
    CurrentHP = FMath::Max(0.f, CurrentHP - FinalDamage);

    UE_LOG(LogBreakable, Warning, TEXT("[Breakable] %s hit! Damage:%.1f (Base:%.1f x %.2f) HP:%.1f/%.1f"),
        *GetNameSafe(GetOwner()), FinalDamage, BaseDamage, DamageMultiplier, CurrentHP, MaxHP);

    // 이펙트 재생 (도구 타입 전달)
    PlayHitEffects(ToolUsed, HitLocation, HitNormal);

    // 델리게이트
    OnDamageReceived.Broadcast(FinalDamage, CurrentHP);

    // 상태 업데이트
    UpdateBreakState();

    // 파괴 확인
    if (CurrentHP <= 0.f && !bHasBroken)
    {
        ExecuteBreak(HitLocation);
    }

    return FinalDamage;
}

bool UBreakableComponent::IsCorrectTool(EBreakToolType ToolUsed) const
{
    if (RequiredTool == EBreakToolType::Any)
        return true;

    return ToolUsed == RequiredTool;
}

float UBreakableComponent::GetToolEffectiveness(EBreakToolType ToolUsed) const
{
    // 재질 + 도구 조합별 효율
    switch (Material)
    {
    case EBreakableMaterial::Wood:
        if (ToolUsed == EBreakToolType::Axe) return 1.5f;
        if (ToolUsed == EBreakToolType::Saw) return 1.2f;
        return 1.0f;

    case EBreakableMaterial::Metal:
        if (ToolUsed == EBreakToolType::Axe) return 0.5f;
        if (ToolUsed == EBreakToolType::Crowbar) return 1.3f;
        if (ToolUsed == EBreakToolType::Hammer) return 1.0f;
        return 0.7f;

    case EBreakableMaterial::Glass:
        // 유리는 모든 도구에 취약
        return 2.0f;

    case EBreakableMaterial::Drywall:
        // 석고보드는 모든 도구에 잘 부서짐
        if (ToolUsed == EBreakToolType::Axe) return 1.8f;
        if (ToolUsed == EBreakToolType::Hammer) return 1.5f;
        return 1.3f;

    case EBreakableMaterial::Concrete:
        if (ToolUsed == EBreakToolType::Hammer) return 1.0f;
        if (ToolUsed == EBreakToolType::Axe) return 0.3f;
        return 0.5f;

    default:
        return 1.0f;
    }
}

void UBreakableComponent::UpdateBreakState()
{
    const float HPRatio = GetHPRatio();
    EBreakableState NewState = BreakState;

    if (HPRatio <= 0.f)
    {
        NewState = EBreakableState::Broken;
    }
    else if (HPRatio <= HeavyDamagedThreshold)
    {
        NewState = EBreakableState::HeavyDamaged;
    }
    else if (HPRatio <= DamagedThreshold)
    {
        NewState = EBreakableState::Damaged;
    }
    else
    {
        NewState = EBreakableState::Intact;
    }

    if (NewState != BreakState)
    {
        SetBreakState(NewState);
    }
}

void UBreakableComponent::SetBreakState(EBreakableState NewState)
{
    const EBreakableState OldState = BreakState;
    BreakState = NewState;

    UE_LOG(LogBreakable, Warning, TEXT("[Breakable] %s State: %d -> %d (HP: %.1f%%)"),
        *GetNameSafe(GetOwner()), (int32)OldState, (int32)NewState, GetHPRatio() * 100.f);

    // 균열 소리 (Broken으로 가는 순간은 ExecuteBreak에서 BreakSound 재생하므로 여기선 제외)
    if (NewState == EBreakableState::Damaged || NewState == EBreakableState::HeavyDamaged)
    {
        if (CrackSound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, CrackSound, GetOwner()->GetActorLocation());
        }
    }

    OnBreakableStateChanged.Broadcast(NewState);
}

void UBreakableComponent::ExecuteBreak(FVector HitLocation)
{
    if (bHasBroken)
        return;

    bHasBroken = true;

    SetBreakState(EBreakableState::Broken);

    UE_LOG(LogBreakable, Error, TEXT("[Breakable] ====== %s BROKEN ======"), *GetNameSafe(GetOwner()));

    // 파괴 이펙트
    PlayBreakEffects(HitLocation);

    // 통과 가능하게 콜리전 변경
    if (bAllowPassthroughWhenBroken)
    {
        AActor* Owner = GetOwner();
        if (IsValid(Owner))
        {
            TArray<UPrimitiveComponent*> PrimitiveComps;
            Owner->GetComponents<UPrimitiveComponent>(PrimitiveComps);

            for (UPrimitiveComponent* Comp : PrimitiveComps)
            {
                if (IsValid(Comp))
                {
                    Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                }
            }
        }
    }

    // 델리게이트
    OnBroken.Broadcast();

    // 자동 제거
    if (DestroyAfterBrokenDelay > 0.f)
    {
        GetOwner()->SetLifeSpan(DestroyAfterBrokenDelay);
    }
}

float UBreakableComponent::GetRandomPitch(float MinPitch, float MaxPitch) const
{
    const float SafeMin = FMath::Max(0.01f, MinPitch);
    const float SafeMax = FMath::Max(SafeMin, MaxPitch);
    return FMath::FRandRange(SafeMin, SafeMax);
}

void UBreakableComponent::PlayHitEffects(EBreakToolType ToolUsed, FVector Location, FVector Normal)
{
    // ===== Hit one-shot (Axe random pitch) =====
    if (HitSound)
    {
        float Pitch = 1.0f;

        const bool bIsAxe = (ToolUsed == EBreakToolType::Axe);
        if (bIsAxe && bAxeHitOneShotRandomPitch)
        {
            Pitch = GetRandomPitch(AxeHitPitchMin, AxeHitPitchMax);
        }

        // one-shot: SoundWave/SoundCue 자체가 Loop가 아니어야 함
        UGameplayStatics::PlaySoundAtLocation(this, HitSound, Location, 1.0f, Pitch);
    }

    if (HitParticle)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), HitParticle, Location, Normal.Rotation());
    }
}

void UBreakableComponent::PlayBreakEffects(FVector Location)
{
    // ===== Break one-shot =====
    if (BreakSound)
    {
        float Pitch = 1.0f;
        if (bBreakOneShotRandomPitch)
        {
            Pitch = GetRandomPitch(BreakPitchMin, BreakPitchMax);
        }

        UGameplayStatics::PlaySoundAtLocation(this, BreakSound, Location, 1.0f, Pitch);
    }

    if (BreakParticle)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BreakParticle, Location);
    }
}

void UBreakableComponent::ForceBreak()
{
    if (bHasBroken)
        return;

    CurrentHP = 0.f;
    ExecuteBreak(GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector);
}

float UBreakableComponent::GetHPRatio() const
{
    if (MaxHP <= 0.f)
        return 0.f;

    return FMath::Clamp(CurrentHP / MaxHP, 0.f, 1.f);
}

bool UBreakableComponent::IsBroken() const
{
    return BreakState == EBreakableState::Broken;
}
