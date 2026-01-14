// ============================ FireActor.cpp ============================
#include "FireActor.h"

#include "RoomActor.h"
#include "CombustibleComponent.h"

#include "Components/SceneComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"

#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogFireActor, Log, All);

static FORCEINLINE FVector GetOwnerCenterFromComb(const UCombustibleComponent* Comb)
{
    const AActor* A = Comb ? Comb->GetOwner() : nullptr;
    return IsValid(A) ? A->GetComponentsBoundingBox(true).GetCenter() : FVector::ZeroVector;
}

static FORCEINLINE float ComputePressure(float EffIntensity, float Dist, float Radius)
{
    if (Radius <= 1.f) return 0.f;
    const float Alpha = FMath::Clamp(Dist / Radius, 0.f, 1.f);
    const float Near = FMath::Pow(1.f - Alpha, 1.5f);
    return EffIntensity * Near;
}

AFireActor::AFireActor()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    FirePsc = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("FirePSC"));
    FirePsc->SetupAttachment(Root);
    FirePsc->bAutoActivate = false;

    FireLoopAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("FireLoopAudio"));
    FireLoopAudio->SetupAttachment(Root);
    FireLoopAudio->bAutoActivate = false;
    FireLoopAudio->bStopWhenOwnerDestroyed = true;
}

void AFireActor::InitFire(ARoomActor* InRoom, ECombustibleType InType)
{
    FireID = FGuid::NewGuid();
    LinkedRoom = InRoom;
    CombustibleType = InType;

    bIsActive = true;
    bInitialized = true;

    EffectiveIntensity = BaseIntensity;
}

void AFireActor::BeginPlay()
{
    Super::BeginPlay();

    if (IsValid(FirePsc))
    {
        if (FireTemplate)
            FirePsc->SetTemplate(FireTemplate);

        FirePsc->ActivateSystem(true);
    }

    if (IsValid(FireLoopAudio) && HeavyFireLoopSound)
    {
        FireLoopAudio->SetSound(HeavyFireLoopSound);
    }

    if (!bInitialized)
    {
        InitFire(SpawnRoom, SpawnType);
    }

    if (!IsValid(LinkedRoom))
    {
        Destroy();
        return;
    }

    if (!IsValid(LinkedCombustible))
    {
        AActor* TargetActor = IgnitedTarget.Get();
        if (IsValid(TargetActor))
        {
            if (UCombustibleComponent* Found = TargetActor->FindComponentByClass<UCombustibleComponent>())
            {
                LinkedCombustible = Found;
                Found->SetOwningRoom(LinkedRoom);
                Found->ActiveFire = this;
                Found->bIsBurning = true;
            }
        }
    }

    if (IsValid(LinkedCombustible))
        LinkedCombustible->EnsureFuelInitialized();

    LinkedRoom->RegisterFire(this);

    UpdateRuntimeFromRoom(0.f);
}

void AFireActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateVfx(DeltaSeconds);
    UpdateAudio(DeltaSeconds);

    if (!bIsActive)
        return;

    if (ShouldExtinguish())
    {
        Extinguish();
        return;
    }

    SpawnAge += DeltaSeconds;

    UpdateRuntimeFromRoom(DeltaSeconds);

    InfluenceAcc += DeltaSeconds;
    if (InfluenceAcc >= InfluenceInterval)
    {
        InfluenceAcc = 0.f;
        ApplyToOwnerCombustible();
        SubmitInfluenceToRoom();
    }

    SpreadAcc += DeltaSeconds;
    if (SpreadAcc >= SpreadInterval)
    {
        SpreadAcc = 0.f;
        SpreadPressureToNeighbors();
    }
}

bool AFireActor::ShouldExtinguish() const
{
    if (!IsValid(LinkedRoom))
        return true;

    if (!LinkedRoom->CanSustainFire())
        return true;

    if (IsValid(LinkedCombustible))
    {
        if (LinkedCombustible->Fuel.FuelCurrent <= 0.f)
            return true;
    }

    return false;
}

float AFireActor::GetCombustionScaleFromRoom() const
{
    if (!IsValid(LinkedRoom))
        return 1.f;

    const float S = LinkedRoom->GetBackdraftCombustionScale01();
    return FMath::Clamp(S, 0.f, 1.f);
}

void AFireActor::UpdateRuntimeFromRoom(float /*DeltaSeconds*/)
{
    if (!IsValid(LinkedRoom))
        return;

    float FuelRatio01 = 1.f;
    float ExtinguishAlpha = 0.f;

    if (IsValid(LinkedCombustible))
    {
        FuelRatio01 = LinkedCombustible->Fuel.FuelRatio01_Cpp();
        ExtinguishAlpha = LinkedCombustible->ExtinguishAlpha01;
    }

    BackdraftScale01 = GetCombustionScaleFromRoom();

    FFireRuntimeTuning T;
    if (!LinkedRoom->GetRuntimeTuning(CombustibleType, EffectiveIntensity, FuelRatio01, T))
        return;

    const float SuppressByWater = FMath::Lerp(1.f, 0.3f, ExtinguishAlpha);
    const float SuppressByBackdraft = FMath::Lerp(1.f, 0.2f, 1.f - BackdraftScale01);
    CurrentSpreadRadius = T.SpreadRadius * SuppressByWater * SuppressByBackdraft;
    SpreadInterval = FMath::Max(0.05f, T.SpreadInterval);

    const float FuelMul = (0.35f + 0.65f * FuelRatio01);
    const float WaterMul = FMath::Lerp(1.f, 0.2f, ExtinguishAlpha);
    const float BackdraftMul = FMath::Lerp(1.f, 0.05f, 1.f - BackdraftScale01);

    EffectiveIntensity = BaseIntensity * FuelMul * WaterMul * BackdraftMul;
    EffectiveIntensity = FMath::Max(0.f, EffectiveIntensity);
}

void AFireActor::ApplyToOwnerCombustible()
{
    if (!IsValid(LinkedCombustible) || !IsValid(LinkedRoom))
        return;

    const float FuelRatio01 = LinkedCombustible->Fuel.FuelRatio01_Cpp();

    const float BackdraftMul = GetCombustionScaleFromRoom();
    if (BackdraftMul <= KINDA_SMALL_NUMBER)
        return;

    FFireRuntimeTuning T;
    if (!LinkedRoom->GetRuntimeTuning(CombustibleType, EffectiveIntensity, FuelRatio01, T))
        return;

    const float Consume =
        T.ConsumePerSecond *
        EffectiveIntensity *
        InfluenceInterval *
        LinkedCombustible->Fuel.FuelConsumeMul *
        BackdraftMul;

    LinkedCombustible->ConsumeFuel(Consume);
    LinkedCombustible->AddHeat(EffectiveIntensity * 0.5f * BackdraftMul);
}

void AFireActor::SubmitInfluenceToRoom()
{
    if (!IsValid(LinkedRoom) || !IsValid(LinkedCombustible))
        return;

    const float FuelRatio01 = LinkedCombustible->Fuel.FuelRatio01_Cpp();
    const float BackdraftMul = GetCombustionScaleFromRoom();
    if (BackdraftMul <= KINDA_SMALL_NUMBER)
        return;

    FFireRuntimeTuning T;
    if (!LinkedRoom->GetRuntimeTuning(CombustibleType, EffectiveIntensity, FuelRatio01, T))
        return;

    LinkedRoom->AccumulateInfluence(CombustibleType, EffectiveIntensity, T.InfluenceScale * BackdraftMul);
}

void AFireActor::SpreadPressureToNeighbors()
{
    if (!IsValid(LinkedRoom))
        return;

    const float BackdraftMul = GetCombustionScaleFromRoom();
    if (BackdraftMul <= 0.2f)
        return;

    TArray<UCombustibleComponent*> List;
    LinkedRoom->GetCombustiblesInRoom(List, /*exclude burning*/true);

    const FVector Origin = GetSpreadOrigin();
    const float Radius = CurrentSpreadRadius;

    for (UCombustibleComponent* C : List)
    {
        if (!IsValid(C) || C->IsBurning())
            continue;

        const FVector P0 = GetOwnerCenterFromComb(C);
        const float Dist = FVector::Dist(P0, Origin);
        if (Dist > Radius)
            continue;

        float Pressure = ComputePressure(EffectiveIntensity, Dist, Radius);

        if (CombustibleType == ECombustibleType::Oil)       Pressure *= 1.10f;
        if (CombustibleType == ECombustibleType::Explosive) Pressure *= 1.60f;

        Pressure *= BackdraftMul;

        if (Pressure <= 0.f)
            continue;

        C->AddIgnitionPressure(FireID, Pressure);
    }
}

FVector AFireActor::GetSpreadOrigin() const
{
    if (IgnitedTarget.IsValid())
        return IgnitedTarget->GetComponentsBoundingBox(true).GetCenter();

    if (IsValid(LinkedCombustible))
        return GetOwnerCenterFromComb(LinkedCombustible);

    return GetActorLocation();
}

void AFireActor::Extinguish()
{
    if (!bIsActive)
        return;

    bIsActive = false;

    // 3_Fire_Extinguish (OneShot) - FireActor´ç 1È¸¸¸
    if (bPlayExtinguishOneShot && !bPlayedExtinguishOneShot && FireExtinguishOneShotSound)
    {
        bPlayedExtinguishOneShot = true;
        UGameplayStatics::PlaySoundAtLocation(this, FireExtinguishOneShotSound, GetActorLocation());
    }

    if (IsValid(FireLoopAudio) && FireLoopAudio->IsPlaying())
    {
        FireLoopAudio->FadeOut(FireLoopFadeOut, 0.f);
    }

    if (IsValid(LinkedRoom))
        LinkedRoom->UnregisterFire(FireID);

    if (IsValid(LinkedCombustible))
    {
        LinkedCombustible->bIsBurning = false;
        LinkedCombustible->ActiveFire = nullptr;
    }

    SetLifeSpan(20.0f);
}

void AFireActor::UpdateVfx(float DeltaSeconds)
{
    if (!IsValid(FirePsc))
        return;

    float TargetStrength01 = 0.f;

    if (bIsActive)
    {
        float Fuel01 = 1.f;
        if (IsValid(LinkedCombustible))
            Fuel01 = LinkedCombustible->Fuel.FuelRatio01_Cpp();

        const float BackdraftMul = GetCombustionScaleFromRoom();

        const float Int01 = FMath::Clamp(EffectiveIntensity / FMath::Max(0.01f, BaseIntensity), 0.f, 2.f);
        const float Raw01 = FMath::Clamp(0.65f * Fuel01 + 0.35f * (Int01 * 0.5f), 0.f, 1.f);

        const float Ramp01 = (IgniteRampSeconds <= 0.f)
            ? 1.f
            : FMath::Clamp(SpawnAge / IgniteRampSeconds, 0.f, 1.f);

        TargetStrength01 = Raw01 * Ramp01 * FMath::Lerp(0.15f, 1.0f, BackdraftMul);
    }
    else
    {
        TargetStrength01 = 0.f;
    }

    Strength01 = FMath::FInterpTo(Strength01, TargetStrength01, DeltaSeconds, 1.0f);
    FirePsc->SetFloatParameter(TEXT("Strength01"), Strength01);
}

void AFireActor::UpdateAudio(float /*DeltaSeconds*/)
{
    if (!IsValid(FireLoopAudio) || !HeavyFireLoopSound)
        return;

    const bool bShouldPlay = bIsActive && (Strength01 > 0.03f);

    if (bShouldPlay)
    {
        if (!FireLoopAudio->IsPlaying())
        {
            FireLoopAudio->Play();
            FireLoopAudio->FadeIn(FireLoopFadeIn, 1.f);
        }

        const float Vol = FMath::Clamp(Strength01, 0.0f, 1.0f);
        FireLoopAudio->SetVolumeMultiplier(Vol);
    }
    else
    {
        if (FireLoopAudio->IsPlaying())
        {
            FireLoopAudio->FadeOut(FireLoopFadeOut, 0.f);
        }
    }
}
