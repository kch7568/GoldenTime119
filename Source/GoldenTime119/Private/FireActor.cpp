// ============================ FireActor.cpp ============================
#include "FireActor.h"

#include "RoomActor.h"
#include "CombustibleComponent.h"

#include "Components/SceneComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogFire, Log, All);

static FORCEINLINE FVector GetOwnerCenterFromComb(const UCombustibleComponent* Comb)
{
    const AActor* A = Comb ? Comb->GetOwner() : nullptr;
    return IsValid(A) ? A->GetComponentsBoundingBox(true).GetCenter() : FVector::ZeroVector;
}

// 거리 가중 압력: 가까울수록 강하게
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
}

void AFireActor::InitFire(ARoomActor* InRoom, ECombustibleType InType)
{
    FireID = FGuid::NewGuid();
    LinkedRoom = InRoom;
    CombustibleType = InType;

    bIsActive = true;
    bInitialized = true;

    EffectiveIntensity = BaseIntensity;

    UE_LOG(LogFire, Warning, TEXT("[Fire] InitFire Fire=%s Id=%s Room=%s Type=%d BaseInt=%.2f Loc=%s"),
        *GetName(), *FireID.ToString(),
        *GetNameSafe(LinkedRoom),
        (int32)CombustibleType,
        BaseIntensity,
        *GetActorLocation().ToString());
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

    if (!bInitialized)
    {
        UE_LOG(LogFire, Warning, TEXT("[Fire] BeginPlay: not initialized -> InitFire(SpawnRoom, SpawnType)"));
        InitFire(SpawnRoom, SpawnType);
    }

    if (!IsValid(LinkedRoom))
    {
        UE_LOG(LogFire, Error, TEXT("[Fire] BeginPlay: LinkedRoom is None -> Destroy"));
        Destroy();
        return;
    }

    // (중요) LinkedCombustible 누락이면 타겟에서 복구
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

                UE_LOG(LogFire, Warning, TEXT("[Fire] LinkedCombustible recovered Owner=%s"),
                    *GetNameSafe(TargetActor));
            }
        }
    }

    if (IsValid(LinkedCombustible))
        LinkedCombustible->EnsureFuelInitialized();

    LinkedRoom->RegisterFire(this);

    // 초기 런타임 세팅
    UpdateRuntimeFromRoom(0.f);
}

void AFireActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // VFX는 항상 갱신
    UpdateVfx(DeltaSeconds);

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

    // RoomActor.h에 이미 제공: GetBackdraftCombustionScale01()
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

    // Backdraft ready면 연소/확산/영향 모두 스케일 다운
    BackdraftScale01 = GetCombustionScaleFromRoom();

    FFireRuntimeTuning T;
    if (!LinkedRoom->GetRuntimeTuning(CombustibleType, EffectiveIntensity, FuelRatio01, T))
        return;

    // Spread
    const float SuppressByWater = FMath::Lerp(1.f, 0.3f, ExtinguishAlpha);
    const float SuppressByBackdraft = FMath::Lerp(1.f, 0.2f, 1.f - BackdraftScale01); // Ready면 더 줄이기
    CurrentSpreadRadius = T.SpreadRadius * SuppressByWater * SuppressByBackdraft;
    SpreadInterval = FMath::Max(0.05f, T.SpreadInterval);

    // Effective intensity
    const float FuelMul = (0.35f + 0.65f * FuelRatio01);
    const float WaterMul = FMath::Lerp(1.f, 0.2f, ExtinguishAlpha);
    const float BackdraftMul = FMath::Lerp(1.f, 0.05f, 1.f - BackdraftScale01); // Ready면 거의 최소

    EffectiveIntensity = BaseIntensity * FuelMul * WaterMul * BackdraftMul;
    EffectiveIntensity = FMath::Max(0.f, EffectiveIntensity);
}

void AFireActor::ApplyToOwnerCombustible()
{
    if (!IsValid(LinkedCombustible) || !IsValid(LinkedRoom))
        return;

    float FuelRatio01 = LinkedCombustible->Fuel.FuelRatio01_Cpp();

    // Backdraft Ready 스케일
    const float BackdraftMul = GetCombustionScaleFromRoom();
    if (BackdraftMul <= KINDA_SMALL_NUMBER)
        return;

    FFireRuntimeTuning T;
    if (!LinkedRoom->GetRuntimeTuning(CombustibleType, EffectiveIntensity, FuelRatio01, T))
        return;

    // 연료 소비 (BackdraftMul로 억제)
    const float Consume =
        T.ConsumePerSecond *
        EffectiveIntensity *
        InfluenceInterval *
        LinkedCombustible->Fuel.FuelConsumeMul *
        BackdraftMul;

    LinkedCombustible->ConsumeFuel(Consume);

    // 열 축적도 Ready면 거의 멈춤
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

    // 방 영향도 Ready면 급감
    LinkedRoom->AccumulateInfluence(CombustibleType, EffectiveIntensity, T.InfluenceScale * BackdraftMul);
}

void AFireActor::SpreadPressureToNeighbors()
{
    if (!IsValid(LinkedRoom))
        return;

    // Backdraft Ready면 확산 압력 거의 차단
    const float BackdraftMul = GetCombustionScaleFromRoom();
    if (BackdraftMul <= 0.2f) // 완전 차단 기준은 취향
        return;

    TArray<UCombustibleComponent*> List;
    LinkedRoom->GetCombustiblesInRoom(List, /*exclude burning*/true);

    const FVector Origin = GetSpreadOrigin();
    const float Radius = CurrentSpreadRadius;

    int32 Applied = 0;

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

        // Ready면 압력도 더 줄임
        Pressure *= BackdraftMul;

        if (Pressure <= 0.f)
            continue;

        C->AddIgnitionPressure(FireID, Pressure);
        Applied++;
    }

    UE_LOG(LogFire, VeryVerbose, TEXT("[Fire] SpreadEnd Applied=%d"), Applied);
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

    if (IsValid(LinkedRoom))
        LinkedRoom->UnregisterFire(FireID);

    if (IsValid(LinkedCombustible))
    {
        LinkedCombustible->bIsBurning = false;
        LinkedCombustible->ActiveFire = nullptr;
    }

    // VFX는 Strength01로 내려가며 사라지게 하고, 생명주기 부여
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

        // Backdraft Ready면 VFX도 거의 꺼진 것처럼
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
