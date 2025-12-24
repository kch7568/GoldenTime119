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

    if (!bIsActive) return;

    if (ShouldExtinguish())
    {
        UE_LOG(LogFire, Warning, TEXT("[Fire] Extinguish Fire=%s Id=%s"), *GetName(), *FireID.ToString());
        Extinguish();
        return;
    }

    UpdateRuntimeFromRoom();


    // 2) 룸에 영향
    InfluenceAcc += DeltaSeconds;
    if (InfluenceAcc >= InfluenceInterval)
    {
        InfluenceAcc = 0.f;
        ApplyToOwnerCombustible();
        SubmitInfluenceToRoom();
    }

    // 3) 확산(압력 전달)
    SpreadAcc += DeltaSeconds;
    if (SpreadAcc >= SpreadInterval)
    {
        SpreadAcc = 0.f;
        SpreadPressureToNeighbors();
        UE_LOG(LogFire, Log, TEXT("[Fire] SpreadApplied SourceFire=%s Id=%s"), *GetName(), *FireID.ToString());
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

    // FuelRatio는 “가연물” 권위
    float FuelRatio01 = 1.f;
    if (IsValid(LinkedCombustible))
        FuelRatio01 = LinkedCombustible->Fuel.FuelRatio01_Cpp();

    FFireRuntimeTuning T;
    if (!LinkedRoom->GetRuntimeTuning(CombustibleType, EffectiveIntensity, FuelRatio01, T))
        return;

    CurrentSpreadRadius = T.SpreadRadius;
    SpreadInterval = FMath::Max(0.05f, T.SpreadInterval);

    // 강도는 BaseIntensity * (연료비율 기반 약화) 정도만 반영(취향)
    EffectiveIntensity = BaseIntensity * (0.35f + 0.65f * FuelRatio01);
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

    // 가연물 쪽 상태 내려주기
    if (IsValid(LinkedCombustible))
    {
        LinkedCombustible->bIsBurning = false;
        LinkedCombustible->ActiveFire = nullptr;
    }

    Destroy();
}
