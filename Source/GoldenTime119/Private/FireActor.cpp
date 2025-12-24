// ============================ FireActor.cpp ============================
#include "FireActor.h"

#include "RoomActor.h"
#include "CombustibleComponent.h"
#include "Components/SceneComponent.h"
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
    FirePsc->bAutoActivate = true;
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

    if (FireTemplate) FirePsc->SetTemplate(FireTemplate);

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

    // (중요) 혹시 LinkedCombustible 누락이면 타겟에서 복구
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
    {
        LinkedCombustible->EnsureFuelInitialized();

        UE_LOG(LogFire, Warning, TEXT("[Fire] FuelAtBeginPlay Owner=%s Init=%.2f Cur=%.2f"),
            *GetNameSafe(LinkedCombustible->GetOwner()),
            LinkedCombustible->Fuel.FuelInitial,
            LinkedCombustible->Fuel.FuelCurrent);
    }

    LinkedRoom->RegisterFire(this);
    UpdateRuntimeFromRoom();
}

void AFireActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // VFX 업데이트는 상태와 상관없이 항상 수행
    UpdateVfx(DeltaSeconds);

    if (!bIsActive) return;

    if (ShouldExtinguish())
    {
        Extinguish();
        return;
    }

    SpawnAge += DeltaSeconds;
    UpdateRuntimeFromRoom();


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
    {
        UE_LOG(LogFire, Warning, TEXT("[Fire] ExtinguishReason: LinkedRoom invalid"));
        return true;
    }

    if (!LinkedRoom->CanSustainFire())
    {
        UE_LOG(LogFire, Warning, TEXT("[Fire] ExtinguishReason: Oxygen low (O2=%.2f, Th=%.2f)"),
            LinkedRoom->Oxygen, LinkedRoom->MinOxygenToSustain);
        return true;
    }

    if (IsValid(LinkedCombustible))
    {
        UE_LOG(LogFire, Warning, TEXT("[Fire] FuelCheck Owner=%s Init=%.2f Cur=%.2f"),
            *GetNameSafe(LinkedCombustible->GetOwner()),
            LinkedCombustible->Fuel.FuelInitial,
            LinkedCombustible->Fuel.FuelCurrent);

        if (LinkedCombustible->Fuel.FuelCurrent <= 0.f)
        {
            UE_LOG(LogFire, Warning, TEXT("[Fire] ExtinguishReason: Fuel empty (%.2f)"), LinkedCombustible->Fuel.FuelCurrent);
            return true;
        }
    }
    else
    {
        UE_LOG(LogFire, Warning, TEXT("[Fire] ExtinguishReason: LinkedCombustible invalid"));
        // 여기서 즉시 끄고 싶지 않으면 return false로 두는 것도 방법
    }

    return false;
}


void AFireActor::UpdateRuntimeFromRoom()
{
    if (!IsValid(LinkedRoom)) return;

    float FuelRatio01 = 1.f;
    float ExtinguishAlpha = 0.f;
    if (IsValid(LinkedCombustible))
    {
        FuelRatio01 = LinkedCombustible->Fuel.FuelRatio01_Cpp();
        ExtinguishAlpha = LinkedCombustible->ExtinguishAlpha01;
    }

    FFireRuntimeTuning T;
    if (!LinkedRoom->GetRuntimeTuning(CombustibleType, EffectiveIntensity, FuelRatio01, T))
        return;

    // 물에 의해 진압 중이라면 확산 반경을 줄임
    CurrentSpreadRadius = T.SpreadRadius * FMath::Lerp(1.f, 0.3f, ExtinguishAlpha);
    SpreadInterval = FMath::Max(0.05f, T.SpreadInterval);

    // 강도 역시 진압률에 따라 약화시켜 주변 가연물에 주는 압력을 줄임
    float SuppressionMul = FMath::Lerp(1.f, 0.2f, ExtinguishAlpha);
    EffectiveIntensity = BaseIntensity * (0.35f + 0.65f * FuelRatio01) * SuppressionMul;
    EffectiveIntensity = FMath::Max(0.f, EffectiveIntensity);
}

void AFireActor::ApplyToOwnerCombustible()
{
    if (!IsValid(LinkedCombustible) || !IsValid(LinkedRoom)) return;

    // 연료 소비량: 정책 + 런타임 튜닝 기반
    float FuelRatio01 = LinkedCombustible->Fuel.FuelRatio01_Cpp();

    FFireRuntimeTuning T;
    if (!LinkedRoom->GetRuntimeTuning(CombustibleType, EffectiveIntensity, FuelRatio01, T))
        return;

    const float Consume = T.ConsumePerSecond * EffectiveIntensity * InfluenceInterval * LinkedCombustible->Fuel.FuelConsumeMul;
    LinkedCombustible->ConsumeFuel(Consume);

    // 타겟 자신도 더 타게(열) -> 재점화 같은 건 필요 없으면 제거 가능
    LinkedCombustible->AddHeat(EffectiveIntensity * 0.5f);
}

void AFireActor::SubmitInfluenceToRoom()
{
    if (!IsValid(LinkedRoom) || !IsValid(LinkedCombustible)) return;

    const float FuelRatio01 = LinkedCombustible->Fuel.FuelRatio01_Cpp();

    FFireRuntimeTuning T;
    if (!LinkedRoom->GetRuntimeTuning(CombustibleType, EffectiveIntensity, FuelRatio01, T))
        return;

    LinkedRoom->AccumulateInfluence(CombustibleType, EffectiveIntensity, T.InfluenceScale);
}

void AFireActor::SpreadPressureToNeighbors()
{
    if (!IsValid(LinkedRoom)) return;

    TArray<UCombustibleComponent*> List;
    LinkedRoom->GetCombustiblesInRoom(List, /*exclude burning*/true);

    const FVector Origin = GetSpreadOrigin();
    const float Radius = CurrentSpreadRadius;

    UE_LOG(LogFire, Warning, TEXT("[Fire] SpreadStart Origin=%s Radius=%.1f Candidates=%d"),
        *Origin.ToString(), Radius, List.Num());

    int32 Applied = 0;

    for (UCombustibleComponent* C : List)
    {
        if (!IsValid(C) || C->IsBurning()) continue;

        const FVector P0 = GetOwnerCenterFromComb(C);
        const float Dist = FVector::Dist(P0, Origin);

        if (Dist > Radius)
        {
            UE_LOG(LogFire, VeryVerbose, TEXT("[Fire] SpreadSkip Dist>R Owner=%s Dist=%.1f"),
                *GetNameSafe(C->GetOwner()), Dist);
            continue;
        }

        float Pressure = ComputePressure(EffectiveIntensity, Dist, Radius);

        if (CombustibleType == ECombustibleType::Oil) Pressure *= 1.10f;
        if (CombustibleType == ECombustibleType::Explosive) Pressure *= 1.60f;

        // ★ 임시로 KINDA_SMALL_NUMBER 체크를 완화해서 “정말 0인지”부터 확인
        if (Pressure <= 0.f)
        {
            UE_LOG(LogFire, VeryVerbose, TEXT("[Fire] SpreadSkip Pressure<=0 Owner=%s Dist=%.1f"),
                *GetNameSafe(C->GetOwner()), Dist);
            continue;
        }

        // 이 줄은 2)에서 고칠 거지만 일단 로그 확인용으로 그대로 둬도 됨
        C->AddIgnitionPressure(FireID, Pressure);

        UE_LOG(LogFire, Warning, TEXT("[Fire] SpreadHit Owner=%s Dist=%.1f Pressure=%.4f"),
            *GetNameSafe(C->GetOwner()), Dist, Pressure);

        Applied++;
    }

    UE_LOG(LogFire, Warning, TEXT("[Fire] SpreadEnd Applied=%d"), Applied);
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
    if (!bIsActive) return;
    bIsActive = false;

    if (IsValid(LinkedRoom))
        LinkedRoom->UnregisterFire(FireID);

    if (IsValid(LinkedCombustible))
    {
        LinkedCombustible->bIsBurning = false;
        LinkedCombustible->ActiveFire = nullptr;
    }

    // Strength01 파라미터를 통해 나이아가라 내부에서 조절

    // 20초의 생명 주기
    SetLifeSpan(20.0f);
}

void AFireActor::UpdateVfx(float DeltaSeconds)
{
    if (!IsValid(FirePsc)) return;

    float TargetStrength01 = 0.f;

    // A. 살아있는 상태일 때의 강도 계산
    if (bIsActive)
    {
        float Fuel01 = 1.f;
        if (IsValid(LinkedCombustible))
            Fuel01 = LinkedCombustible->Fuel.FuelRatio01_Cpp();

        const float Int01 = FMath::Clamp(EffectiveIntensity / FMath::Max(0.01f, BaseIntensity), 0.f, 2.f);
        const float Raw01 = FMath::Clamp(0.65f * Fuel01 + 0.35f * (Int01 * 0.5f), 0.f, 1.f);
        const float Ramp01 = (IgniteRampSeconds <= 0.f) ? 1.f : FMath::Clamp(SpawnAge / IgniteRampSeconds, 0.f, 1.f);

        TargetStrength01 = Raw01 * Ramp01;
    }
    else
    {
        // B. 소멸 중일 때 (Extinguish 호출 이후)
        // 이미 생성된 Strength01을 0으로 빠르게 깎습니다.
        TargetStrength01 = 0.0f;
    }

    // 부드럽게 보간 
    Strength01 = FMath::FInterpTo(Strength01, TargetStrength01, DeltaSeconds, 1.0f);
    FirePsc->SetFloatParameter(TEXT("Strength01"), Strength01);
}
