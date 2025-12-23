#include "RoomActor.h"

#include "FireActor.h"
#include "CombustibleComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogFire, Log, All);

ARoomActor::ARoomActor()
{
    PrimaryActorTick.bCanEverTick = true;

    RoomBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("RoomBounds"));
    SetRootComponent(RoomBounds);

    RoomBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    RoomBounds->SetGenerateOverlapEvents(true);
    RoomBounds->SetCollisionObjectType(ECC_WorldDynamic);
    RoomBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    RoomBounds->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    RoomBounds->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
    RoomBounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    FireClass = AFireActor::StaticClass();

    // 기본 정책(원하면 디테일에서 조정)
    PolicyNormal.InitialFuel = 12.0f;
    PolicyNormal.ConsumePerSecond_Min = 0.7f;
    PolicyNormal.ConsumePerSecond_Max = 1.3f;
    PolicyNormal.SpreadRadius_Min = 280.0f;
    PolicyNormal.SpreadRadius_Max = 900.0f;
    PolicyNormal.SpreadInterval_Min = 0.55f;
    PolicyNormal.SpreadInterval_Max = 1.30f;

    PolicyOil.InitialFuel = 16.0f;
    PolicyOil.ConsumePerSecond_Min = 0.9f;
    PolicyOil.ConsumePerSecond_Max = 1.8f;
    PolicyOil.SpreadRadius_Min = 350.0f;
    PolicyOil.SpreadRadius_Max = 1100.0f;
    PolicyOil.SpreadInterval_Min = 0.50f;
    PolicyOil.SpreadInterval_Max = 1.15f;
    PolicyOil.HeatMul = 1.15f;
    PolicyOil.SmokeMul = 1.25f;

    PolicyElectric.InitialFuel = 10.0f;
    PolicyElectric.ConsumePerSecond_Min = 0.6f;
    PolicyElectric.ConsumePerSecond_Max = 1.2f;
    PolicyElectric.SpreadRadius_Min = 260.0f;
    PolicyElectric.SpreadRadius_Max = 850.0f;
    PolicyElectric.SpreadInterval_Min = 0.55f;
    PolicyElectric.SpreadInterval_Max = 1.25f;

    PolicyExplosive.InitialFuel = 3.0f;
    PolicyExplosive.ConsumePerSecond_Min = 1.2f;
    PolicyExplosive.ConsumePerSecond_Max = 2.4f;
    PolicyExplosive.SpreadRadius_Min = 450.0f;
    PolicyExplosive.SpreadRadius_Max = 1600.0f;
    PolicyExplosive.SpreadInterval_Min = 0.35f;
    PolicyExplosive.SpreadInterval_Max = 0.90f;
    PolicyExplosive.HeatMul = 1.4f;
    PolicyExplosive.SmokeMul = 1.2f;

    // ===== Spread Delay 기본값 (원하면 BP/인스턴스에서 조정) =====
    // (헤더에 UPROPERTY로 선언돼있다고 가정)
    SpreadDelayNearSec = 5.0f;
    SpreadDelayFarSec = 40.0f;
    SpreadDelayJitterSec = 1.5f;
}

void ARoomActor::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogFire, Log, TEXT("[Room] BeginPlay Room=%s"), *GetName());
}

void ARoomActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // 예약 점화 처리(먼저 처리해도 되고 마지막에 처리해도 되는데, “확산 예약”을 같은 프레임에 처리하고 싶으면 Tick 앞쪽)
    ProcessPendingIgnitions();

    ApplyAccumulators(DeltaSeconds);
    UpdateRoomState();
    ResetAccumulators();
}

const FFireFuelPolicy& ARoomActor::GetFuelPolicy(ECombustibleType Type) const
{
    switch (Type)
    {
    case ECombustibleType::Normal:    return PolicyNormal;
    case ECombustibleType::Oil:       return PolicyOil;
    case ECombustibleType::Electric:  return PolicyElectric;
    case ECombustibleType::Explosive: return PolicyExplosive;
    default:                          return PolicyNormal;
    }
}

void ARoomActor::RegisterFire(AFireActor* Fire)
{
    if (!IsValid(Fire)) return;

    ActiveFires.Add(Fire->FireID, Fire);

    const ECombustibleType Type = Fire->CombustibleType;
    const FFireFuelPolicy& P = GetFuelPolicy(Type);

    FireFuelCurrent.Add(Fire->FireID, P.InitialFuel);
    FireFuelInitial.Add(Fire->FireID, P.InitialFuel);
    FireTypeById.Add(Fire->FireID, Type);

    UE_LOG(LogFire, Warning,
        TEXT("[Room] RegisterFire Room=%s Fire=%s Id=%s Type=%d InitFuel=%.2f ActiveFires=%d"),
        *GetName(),
        *Fire->GetName(),
        *Fire->FireID.ToString(),
        (int32)Type,
        P.InitialFuel,
        ActiveFires.Num()
    );

    OnFireStarted.Broadcast(Fire);
}

void ARoomActor::UnregisterFire(const FGuid& FireID)
{
    AFireActor* Fire = nullptr;
    if (TObjectPtr<AFireActor>* Found = ActiveFires.Find(FireID))
        Fire = Found->Get();

    UE_LOG(LogFire, Warning,
        TEXT("[Room] UnregisterFire Room=%s Fire=%s Id=%s ActiveFiresBefore=%d"),
        *GetName(),
        IsValid(Fire) ? *Fire->GetName() : TEXT("None"),
        *FireID.ToString(),
        ActiveFires.Num()
    );

    ActiveFires.Remove(FireID);

    // BurningTargets 정리
    for (auto It = BurningTargets.CreateIterator(); It; ++It)
    {
        if (It.Value() == FireID)
        {
            UE_LOG(LogFire, Log, TEXT("[Room] BurningTarget cleared Actor=%s (FireId=%s)"),
                IsValid(It.Key().Get()) ? *It.Key().Get()->GetName() : TEXT("None"),
                *FireID.ToString());
            It.RemoveCurrent();
        }
    }

    // 예약도 정리(이미 불이 꺼진 타겟은 더 이상 예약 유지 필요 없음)
    for (auto It = PendingIgnitions.CreateIterator(); It; ++It)
    {
        const FPendingIgnition& P = It.Value();
        if (P.SourceFireId == FireID)
        {
            ReservedTargets.Remove(It.Key());
            It.RemoveCurrent();
        }
    }

    FireFuelCurrent.Remove(FireID);
    FireFuelInitial.Remove(FireID);
    FireTypeById.Remove(FireID);

    if (IsValid(Fire))
        OnFireExtinguished.Broadcast(Fire);

    UE_LOG(LogFire, Warning, TEXT("[Room] Unregister done Room=%s ActiveFiresNow=%d"),
        *GetName(), ActiveFires.Num());
}

bool ARoomActor::ShouldExtinguishFire(const FGuid& FireID) const
{
    if (Oxygen <= MinOxygenToSustain)
        return true;

    if (const float* Fuel = FireFuelCurrent.Find(FireID))
    {
        if (*Fuel <= 0.0f)
            return true;
    }
    else
    {
        return true;
    }

    return false;
}

float ARoomActor::GetIntensityScale(const FGuid& FireID) const
{
    const float* Fuel = FireFuelCurrent.Find(FireID);
    const float* Init = FireFuelInitial.Find(FireID);
    const ECombustibleType* Type = FireTypeById.Find(FireID);

    if (!Fuel || !Init || !Type || *Init <= 0.0f)
        return 0.0f;

    const FFireFuelPolicy& P = GetFuelPolicy(*Type);

    const float Ratio = FMath::Clamp((*Fuel) / (*Init), 0.0f, 1.0f);
    return FMath::Pow(Ratio, FMath::Max(0.1f, P.ScalePow));
}

bool ARoomActor::GetRuntimeTuning(const FGuid& FireID, ECombustibleType Type, float EffectiveIntensity, FFireRuntimeTuning& Out) const
{
    const float* Fuel = FireFuelCurrent.Find(FireID);
    const float* Init = FireFuelInitial.Find(FireID);
    if (!Fuel || !Init || *Init <= 0.0f)
        return false;

    const FFireFuelPolicy& P = GetFuelPolicy(Type);

    // 연료 비율
    const float FuelRatio01 = FMath::Clamp((*Fuel) / (*Init), 0.0f, 1.0f);

    // 강도 비율(EffectiveIntensity 기반)
    const float RawIntensity01 = (P.IntensityRef > 0.0f) ? (EffectiveIntensity / P.IntensityRef) : 1.0f;
    const float Intensity01 = FMath::Clamp(
        FMath::Pow(FMath::Clamp(RawIntensity01, 0.0f, 1.0f), FMath::Max(0.1f, P.IntensityPow)),
        0.0f, 1.0f
    );

    // 현재 SpreadRadius: 연료가 떨어지면 줄고, 강하면 늘어남
    const float SpreadAlpha = FMath::Clamp(FuelRatio01 * (0.35f + 0.65f * Intensity01), 0.0f, 1.0f);

    Out.FuelRatio01 = FuelRatio01;
    Out.Intensity01 = Intensity01;
    Out.SpreadRadius = FMath::Lerp(P.SpreadRadius_Min, P.SpreadRadius_Max, SpreadAlpha);

    // 확산 주기: 강할수록 더 자주 (Intensity가 1이면 Min)
    Out.SpreadInterval = FMath::Lerp(P.SpreadInterval_Max, P.SpreadInterval_Min, Intensity01);

    // 연소 속도: 강할수록 Consume 증가
    Out.ConsumePerSecond = FMath::Lerp(P.ConsumePerSecond_Min, P.ConsumePerSecond_Max, Intensity01);

    // 룸 영향 스케일
    Out.InfluenceScale = FMath::Clamp((0.5f + 0.5f * Intensity01) * (0.3f + 0.7f * FuelRatio01), 0.0f, 3.0f);

    return true;
}

void ARoomActor::SubmitInfluence(const FGuid& FireID, ECombustibleType Type, float EffectiveIntensity)
{
    float* Fuel = FireFuelCurrent.Find(FireID);
    float* Init = FireFuelInitial.Find(FireID);
    if (!Fuel || !Init) return;

    const FFireFuelPolicy& P = GetFuelPolicy(Type);

    FFireRuntimeTuning Tuning;
    if (!GetRuntimeTuning(FireID, Type, EffectiveIntensity, Tuning))
        return;

    const float StepSeconds = 0.5f; // Fire InfluenceInterval 전제
    const float BeforeFuel = *Fuel;

    *Fuel = FMath::Max(0.0f, *Fuel - (Tuning.ConsumePerSecond * EffectiveIntensity * StepSeconds));

    FRoomInfluence I = CalcInfluence(Type, EffectiveIntensity);

    I.HeatAdd *= (P.HeatMul * Tuning.InfluenceScale);
    I.SmokeAdd *= (P.SmokeMul * Tuning.InfluenceScale);
    I.OxygenSub *= (P.OxygenMul * Tuning.InfluenceScale);
    I.FireValueAdd *= (P.FireValueMul * Tuning.InfluenceScale);

    AccHeat += I.HeatAdd;
    AccSmoke += I.SmokeAdd;
    AccOxygenSub += I.OxygenSub;
    AccFireValue += I.FireValueAdd;

    UE_LOG(LogFire, VeryVerbose,
        TEXT("[Room] Influence Room=%s FireId=%s Type=%d Eff=%.2f Fuel %.2f->%.2f (Consume=%.2f) Acc(H=%.2f S=%.2f O2=%.3f FV=%.2f)"),
        *GetName(), *FireID.ToString(), (int32)Type, EffectiveIntensity,
        BeforeFuel, *Fuel, Tuning.ConsumePerSecond,
        AccHeat, AccSmoke, AccOxygenSub, AccFireValue
    );
}

void ARoomActor::ResetAccumulators()
{
    AccHeat = AccSmoke = AccOxygenSub = AccFireValue = 0.0f;
}

void ARoomActor::ApplyAccumulators(float DeltaSeconds)
{
    Heat += AccHeat * DeltaSeconds;
    Smoke += AccSmoke * DeltaSeconds;
    FireValue += AccFireValue * DeltaSeconds;

    Oxygen = FMath::Clamp(Oxygen - (AccOxygenSub * DeltaSeconds), 0.0f, 1.0f);
}

void ARoomActor::UpdateRoomState()
{
    const int32 FireCount = ActiveFires.Num();
    const ERoomState Prev = State;

    if (FireCount <= 0)
    {
        State = (Heat >= RiskHeatThreshold || Smoke > 0.2f) ? ERoomState::Risk : ERoomState::Idle;
    }
    else
    {
        State = ERoomState::Fire;
    }

    if (Prev != State)
    {
        UE_LOG(LogFire, Warning,
            TEXT("[Room] StateChange Room=%s %d -> %d (Heat=%.1f Smoke=%.2f O2=%.2f Fires=%d)"),
            *GetName(), (int32)Prev, (int32)State, Heat, Smoke, Oxygen, FireCount
        );
    }
}

FRoomInfluence ARoomActor::CalcInfluence(ECombustibleType Type, float EffectiveIntensity) const
{
    FRoomInfluence R;

    switch (Type)
    {
    case ECombustibleType::Normal:
        R.HeatAdd = 12.0f * EffectiveIntensity;
        R.SmokeAdd = 8.0f * EffectiveIntensity;
        R.OxygenSub = 0.03f * EffectiveIntensity;
        R.FireValueAdd = 1.0f * EffectiveIntensity;
        break;

    case ECombustibleType::Oil:
        R.HeatAdd = 18.0f * EffectiveIntensity;
        R.SmokeAdd = 12.0f * EffectiveIntensity;
        R.OxygenSub = 0.05f * EffectiveIntensity;
        R.FireValueAdd = 1.6f * EffectiveIntensity;
        break;

    case ECombustibleType::Electric:
        R.HeatAdd = 10.0f * EffectiveIntensity;
        R.SmokeAdd = 6.0f * EffectiveIntensity;
        R.OxygenSub = 0.02f * EffectiveIntensity;
        R.FireValueAdd = 1.2f * EffectiveIntensity;
        break;

    case ECombustibleType::Explosive:
        R.HeatAdd = 25.0f * EffectiveIntensity;
        R.SmokeAdd = 15.0f * EffectiveIntensity;
        R.OxygenSub = 0.07f * EffectiveIntensity;
        R.FireValueAdd = 2.5f * EffectiveIntensity;
        break;
    }

    return R;
}

bool ARoomActor::IsActorAlreadyOnFire(AActor* Target) const
{
    return Target && BurningTargets.Contains(Target);
}

// Box 스케일링 반영(중요)
bool ARoomActor::IsInsideRoomBox(const UBoxComponent* Box, const FVector& WorldPos)
{
    if (!Box) return false;

    const FVector Local = Box->GetComponentTransform().InverseTransformPosition(WorldPos);
    const FVector Extent = Box->GetScaledBoxExtent(); // ✅ scaled extent 사용

    return FMath::Abs(Local.X) <= Extent.X
        && FMath::Abs(Local.Y) <= Extent.Y
        && FMath::Abs(Local.Z) <= Extent.Z;
}

// 후보/거리/룸판정 모두 “바운즈 중심” 기준으로 통일
static FORCEINLINE FVector GetActorCheckPos(AActor* A)
{
    return IsValid(A) ? A->GetComponentsBoundingBox(true).GetCenter() : FVector::ZeroVector;
}

bool ARoomActor::CollectCandidates(const FVector& Origin, float Radius, ARoomActor* InRoom, TArray<AActor*>& OutActors) const
{
    if (!GetWorld() || !IsValid(InRoom) || !IsValid(InRoom->RoomBounds)) return false;

    OutActors.Reset();
    const float RadiusSq = Radius * Radius;

    int32 InBoxFail = 0, NoComb = 0, Burning = 0, TooFar = 0, Added = 0;

    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* A = *It;
        if (!IsValid(A)) continue;

        const FVector Pos = GetActorCheckPos(A);

        // 룸 박스 내부
        if (!IsInsideRoomBox(InRoom->RoomBounds, Pos))
        {
            InBoxFail++;
            continue;
        }

        UCombustibleComponent* Comb = A->FindComponentByClass<UCombustibleComponent>();
        if (!Comb)
        {
            NoComb++;
            continue;
        }

        if (InRoom->IsActorAlreadyOnFire(A))
        {
            Burning++;
            continue;
        }

        // 예약 중이면 제외(중복 예약 방지)
        if (InRoom->ReservedTargets.Contains(A))
        {
            continue;
        }

        const float D = FVector::DistSquared(Pos, Origin);
        if (D > RadiusSq)
        {
            TooFar++;
            continue;
        }

        OutActors.Add(A);
        Added++;
    }

    UE_LOG(LogFire, Log, TEXT("[Room] CollectCandidates Room=%s Origin=%s R=%.1f -> %d"),
        *GetNameSafe(InRoom), *Origin.ToString(), Radius, OutActors.Num());

    UE_LOG(LogFire, VeryVerbose, TEXT("[Room] CC dbg Added=%d InBoxFail=%d NoComb=%d Burning=%d TooFar=%d"),
        Added, InBoxFail, NoComb, Burning, TooFar);

    return OutActors.Num() > 0;
}

AActor* ARoomActor::PickNearestCandidate(const FVector& Origin, const TArray<AActor*>& Candidates) const
{
    float BestD = TNumericLimits<float>::Max();
    AActor* Best = nullptr;

    for (AActor* A : Candidates)
    {
        if (!IsValid(A)) continue;
        const float D = FVector::DistSquared(GetActorCheckPos(A), Origin);
        if (D < BestD)
        {
            BestD = D;
            Best = A;
        }
    }
    return Best;
}

AFireActor* ARoomActor::SpawnFireInternal(ARoomActor* TargetRoom, AActor* TargetActor, ECombustibleType Type)
{
    if (!IsValid(TargetRoom) || !IsValid(TargetActor) || !GetWorld()) return nullptr;
    if (!FireClass) FireClass = AFireActor::StaticClass();

    const FVector TargetCenter = TargetActor->GetComponentsBoundingBox(true).GetCenter();

    UE_LOG(LogFire, Warning,
        TEXT("[Room] SpawnFireInternal Room=%s Target=%s Type=%d Class=%s TargetCenter=%s"),
        *TargetRoom->GetName(), *TargetActor->GetName(), (int32)Type, *GetNameSafe(FireClass),
        *TargetCenter.ToString());

    const FTransform SpawnTM(FRotator::ZeroRotator, TargetCenter);

    AFireActor* NewFire = GetWorld()->SpawnActorDeferred<AFireActor>(
        FireClass, SpawnTM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    if (!IsValid(NewFire))
    {
        UE_LOG(LogFire, Error, TEXT("[Room] SpawnActorDeferred failed"));
        return nullptr;
    }

    NewFire->SpawnRoom = TargetRoom;
    NewFire->SpawnType = Type;

    // FireActor에 IgnitedTarget(TWeakObjectPtr) 넣어둔 경우에만 사용
    NewFire->IgnitedTarget = TargetActor;

    // BeginPlay 전에 초기화
    NewFire->InitFire(TargetRoom, Type);

    // BurningTargets 선등록
    TargetRoom->BurningTargets.Add(TargetActor, NewFire->FireID);

    // FinishSpawning이 BeginPlay를 바로 호출할 수 있음
    NewFire->FinishSpawning(SpawnTM);

    // (중요) BeginPlay/InitFire에서 위치가 0으로 리셋돼도 다시 강제
    NewFire->SetActorLocation(TargetCenter, false, nullptr, ETeleportType::TeleportPhysics);

    // 어태치: 스냅해서 타겟 기준으로 고정 (프롭이 움직이든 말든 안정)
    NewFire->AttachToActor(TargetActor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    NewFire->SetActorRelativeLocation(FVector::ZeroVector);
    NewFire->SetActorRelativeRotation(FRotator::ZeroRotator);

    // 최종 한번 더 강제(안전빵)
    NewFire->SetActorLocation(TargetCenter, false, nullptr, ETeleportType::TeleportPhysics);

    UE_LOG(LogFire, Warning,
        TEXT("[Room] FireSpawned Fire=%s Id=%s Target=%s FireLoc=%s (TargetCenter=%s)"),
        *NewFire->GetName(),
        *NewFire->FireID.ToString(),
        *TargetActor->GetName(),
        *NewFire->GetActorLocation().ToString(),
        *TargetCenter.ToString());

    return NewFire;
}

AFireActor* ARoomActor::IgniteTarget(AActor* TargetActor)
{
    if (!IsValid(TargetActor))
    {
        UE_LOG(LogFire, Warning, TEXT("[Room] IgniteTarget 실패: Target invalid"));
        return nullptr;
    }

    if (IsActorAlreadyOnFire(TargetActor))
    {
        UE_LOG(LogFire, Log, TEXT("[Room] IgniteTarget 무시: 이미 불붙음 Target=%s"), *TargetActor->GetName());
        return nullptr;
    }

    // 예약중이면 무시
    if (ReservedTargets.Contains(TargetActor))
    {
        UE_LOG(LogFire, Log, TEXT("[Room] IgniteTarget 무시: 예약 점화 중 Target=%s"), *GetNameSafe(TargetActor));
        return nullptr;
    }

    UCombustibleComponent* Comb = TargetActor->FindComponentByClass<UCombustibleComponent>();
    if (!Comb)
    {
        UE_LOG(LogFire, Warning, TEXT("[Room] IgniteTarget 실패: CombustibleComponent 없음 Target=%s"), *TargetActor->GetName());
        return nullptr;
    }

    if (Comb->CombustibleType == ECombustibleType::Electric && !Comb->bElectricIgnitionTriggered)
    {
        UE_LOG(LogFire, Log, TEXT("[Room] IgniteTarget 실패: Electric 트리거 없음 Target=%s"), *TargetActor->GetName());
        return nullptr;
    }

    UE_LOG(LogFire, Warning, TEXT("[Room] IGNITE Target=%s Room=%s Type=%d"),
        *TargetActor->GetName(), *GetName(), (int32)Comb->CombustibleType);

    return SpawnFireInternal(this, TargetActor, Comb->CombustibleType);
}

bool ARoomActor::TrySpawnFireFromSpread(AFireActor* SourceFire)
{
    if (!IsValid(SourceFire) || !IsValid(SourceFire->LinkedRoom)) return false;

    ARoomActor* SourceRoom = SourceFire->LinkedRoom;
    if (!IsValid(SourceRoom)) return false;

    UE_LOG(LogFire, Log,
        TEXT("[Room] SpreadRequest FromFire=%s Id=%s Type=%d Room=%s"),
        *SourceFire->GetName(),
        *SourceFire->FireID.ToString(),
        (int32)SourceFire->CombustibleType,
        *SourceRoom->GetName()
    );

    switch (SourceFire->CombustibleType)
    {
    case ECombustibleType::Normal:    return SourceRoom->Spread_Normal(SourceFire);
    case ECombustibleType::Oil:       return SourceRoom->Spread_Oil(SourceFire);
    case ECombustibleType::Electric:  return SourceRoom->Spread_Electric(SourceFire);
    case ECombustibleType::Explosive: return SourceRoom->Spread_Explosive(SourceFire);
    default:                          return false;
    }
}

// 확산 원점: FireActor 위치가 아니라 “점화 타겟 중심”을 우선 사용
static FORCEINLINE FVector ResolveSpreadOrigin(AFireActor* SourceFire)
{
    if (!IsValid(SourceFire)) return FVector::ZeroVector;

    if (SourceFire->IgnitedTarget.IsValid())
        return SourceFire->IgnitedTarget->GetComponentsBoundingBox(true).GetCenter();

    return SourceFire->GetActorLocation();
}

// ===== 예약 점화 구현 =====

bool ARoomActor::ScheduleIgnitionFromSpread(
    AFireActor* SourceFire,
    ARoomActor* TargetRoom,
    AActor* TargetActor,
    float Radius)
{
    if (!IsValid(SourceFire) || !IsValid(TargetRoom) || !IsValid(TargetActor) || !GetWorld())
        return false;

    // 이미 불 / 이미 예약이면 스킵
    if (TargetRoom->IsActorAlreadyOnFire(TargetActor)) return false;
    if (TargetRoom->ReservedTargets.Contains(TargetActor)) return false;

    UCombustibleComponent* Comb = TargetActor->FindComponentByClass<UCombustibleComponent>();
    if (!Comb) return false;

    // 전기 조건
    if (Comb->CombustibleType == ECombustibleType::Electric && !Comb->bElectricIgnitionTriggered)
        return false;

    const FVector Origin = ResolveSpreadOrigin(SourceFire);
    const FVector TargetPos = GetActorCheckPos(TargetActor);

    const float Dist = FVector::Dist(TargetPos, Origin);
    const float Alpha = (Radius > 1.f) ? FMath::Clamp(Dist / Radius, 0.f, 1.f) : 1.f;

    float Delay = FMath::Lerp(SpreadDelayNearSec, SpreadDelayFarSec, Alpha);
    Delay += FMath::FRandRange(-SpreadDelayJitterSec, SpreadDelayJitterSec);
    Delay = FMath::Max(0.1f, Delay);

    const float Now = GetWorld()->GetTimeSeconds();

    FPendingIgnition P;
    P.Target = TargetActor;
    P.Type = Comb->CombustibleType;
    P.IgniteAtTime = Now + Delay;
    P.SourceFireId = SourceFire->FireID;
    P.SourceOrigin = Origin;

    TargetRoom->PendingIgnitions.Add(TargetActor, P);
    TargetRoom->ReservedTargets.Add(TargetActor);

    UE_LOG(LogFire, Warning,
        TEXT("[Room] ScheduleIgnition Room=%s Target=%s Dist=%.1f R=%.1f Alpha=%.2f Delay=%.2fs IgniteAt=%.2f Type=%d"),
        *GetNameSafe(TargetRoom),
        *GetNameSafe(TargetActor),
        Dist, Radius, Alpha, Delay, P.IgniteAtTime,
        (int32)P.Type
    );

    return true;
}

void ARoomActor::ProcessPendingIgnitions()
{
    if (!GetWorld()) return;

    const float Now = GetWorld()->GetTimeSeconds();

    for (auto It = PendingIgnitions.CreateIterator(); It; ++It)
    {
        const TWeakObjectPtr<AActor> Key = It.Key();
        FPendingIgnition& P = It.Value();

        AActor* Target = P.Target.Get();
        if (!IsValid(Target))
        {
            ReservedTargets.Remove(Key);
            It.RemoveCurrent();
            continue;
        }

        // 이미 불붙었으면 예약 해제
        if (IsActorAlreadyOnFire(Target))
        {
            ReservedTargets.Remove(Key);
            It.RemoveCurrent();
            continue;
        }

        if (Now < P.IgniteAtTime)
            continue;

        // 예약 시간이 됐다 -> 실제 점화
        UCombustibleComponent* Comb = Target->FindComponentByClass<UCombustibleComponent>();
        if (!Comb)
        {
            UE_LOG(LogFire, Log, TEXT("[Room] PendingIgnition cancel: no Comb Target=%s"), *GetNameSafe(Target));
            ReservedTargets.Remove(Key);
            It.RemoveCurrent();
            continue;
        }

        if (Comb->CombustibleType == ECombustibleType::Electric && !Comb->bElectricIgnitionTriggered)
        {
            UE_LOG(LogFire, Log, TEXT("[Room] PendingIgnition waiting: Electric not triggered Target=%s"), *GetNameSafe(Target));
            // 여기서는 “취소”가 아니라 계속 대기시키고 싶으면 return하지 말고 continue.
            // 다만 전기는 영원히 안 켜질 수도 있으니 타임아웃 정책을 따로 두는 것도 가능.
            ReservedTargets.Remove(Key);
            It.RemoveCurrent();
            continue;
        }

        UE_LOG(LogFire, Warning,
            TEXT("[Room] PendingIgnition FIRE! Room=%s Target=%s Type=%d (SrcFire=%s)"),
            *GetNameSafe(this),
            *GetNameSafe(Target),
            (int32)P.Type,
            *P.SourceFireId.ToString()
        );

        AFireActor* NewFire = SpawnFireInternal(this, Target, P.Type);
        if (IsValid(NewFire))
        {
            OnFireSpread.Broadcast(NewFire);
        }

        ReservedTargets.Remove(Key);
        It.RemoveCurrent();
    }
}

// ===== 타입 기반 확산 =====

bool ARoomActor::Spread_Normal(AFireActor* SourceFire)
{
    const FVector Origin = ResolveSpreadOrigin(SourceFire);
    const float Radius = SourceFire->CurrentSpreadRadius;

    UE_LOG(LogFire, Log, TEXT("[Room] Spread_Normal Fire=%s Origin=%s Radius=%.1f"),
        *SourceFire->GetName(), *Origin.ToString(), Radius);

    // 같은 룸 1개 우선
    {
        TArray<AActor*> Candidates;
        if (CollectCandidates(Origin, Radius, this, Candidates))
        {
            AActor* Pick = PickNearestCandidate(Origin, Candidates);
            if (IsValid(Pick))
            {
                // ✅ 즉시 점화 대신 예약 점화
                if (ScheduleIgnitionFromSpread(SourceFire, this, Pick, Radius))
                {
                    UE_LOG(LogFire, Warning, TEXT("[Room] Spread_Normal SAME-ROOM 예약 Target=%s"), *GetNameSafe(Pick));
                    return true;
                }
            }
        }
    }

    // 인접 룸 1개
    for (ARoomActor* N : AdjacentRooms)
    {
        if (!IsValid(N)) continue;

        TArray<AActor*> Candidates;
        if (CollectCandidates(Origin, Radius, N, Candidates))
        {
            AActor* Pick = N->PickNearestCandidate(Origin, Candidates);
            if (IsValid(Pick))
            {
                if (ScheduleIgnitionFromSpread(SourceFire, N, Pick, Radius))
                {
                    UE_LOG(LogFire, Warning, TEXT("[Room] Spread_Normal ADJ-ROOM=%s 예약 Target=%s"), *N->GetName(), *GetNameSafe(Pick));
                    return true;
                }
            }
        }
    }

    UE_LOG(LogFire, Log, TEXT("[Room] Spread_Normal 결과: 실패"));
    return false;
}

bool ARoomActor::Spread_Oil(AFireActor* SourceFire)
{
    const FVector Origin = ResolveSpreadOrigin(SourceFire);
    const float Radius = SourceFire->CurrentSpreadRadius * 1.1f;

    UE_LOG(LogFire, Log, TEXT("[Room] Spread_Oil Fire=%s Origin=%s Radius=%.1f"),
        *SourceFire->GetName(), *Origin.ToString(), Radius);

    int32 Scheduled = 0;
    TArray<AActor*> Candidates;

    if (CollectCandidates(Origin, Radius, this, Candidates))
    {
        Candidates.Sort([&](const AActor& L, const AActor& R)
            {
                return FVector::DistSquared(GetActorCheckPos(const_cast<AActor*>(&L)), Origin) <
                    FVector::DistSquared(GetActorCheckPos(const_cast<AActor*>(&R)), Origin);
            });

        for (AActor* Pick : Candidates)
        {
            if (!IsValid(Pick)) continue;
            if (ScheduleIgnitionFromSpread(SourceFire, this, Pick, Radius))
            {
                UE_LOG(LogFire, Warning, TEXT("[Room] Spread_Oil 예약 Target=%s"), *GetNameSafe(Pick));
                Scheduled++;
                if (Scheduled >= 2) break;
            }
        }
    }

    if (Scheduled > 0)
    {
        UE_LOG(LogFire, Warning, TEXT("[Room] Spread_Oil 결과: 성공 Scheduled=%d"), Scheduled);
        return true;
    }

    UE_LOG(LogFire, Log, TEXT("[Room] Spread_Oil fallback -> Spread_Normal"));
    return Spread_Normal(SourceFire);
}

bool ARoomActor::Spread_Electric(AFireActor* SourceFire)
{
    const FVector Origin = ResolveSpreadOrigin(SourceFire);
    const float Radius = SourceFire->CurrentSpreadRadius;

    UE_LOG(LogFire, Log, TEXT("[Room] Spread_Electric Fire=%s Origin=%s Radius=%.1f NetHint=%s"),
        *SourceFire->GetName(), *Origin.ToString(), Radius, *SourceFire->ElectricNetIdHint.ToString());

    auto TryRoom = [&](ARoomActor* R) -> bool
        {
            TArray<AActor*> Candidates;
            if (!CollectCandidates(Origin, Radius, R, Candidates)) return false;

            TArray<AActor*> SameNet;
            TArray<AActor*> OtherNet;

            for (AActor* A : Candidates)
            {
                UCombustibleComponent* Comb = A ? A->FindComponentByClass<UCombustibleComponent>() : nullptr;
                if (!Comb) continue;
                if (Comb->CombustibleType != ECombustibleType::Electric) continue;
                if (!Comb->bElectricIgnitionTriggered) continue;

                if (Comb->ElectricNetId != NAME_None && Comb->ElectricNetId == SourceFire->ElectricNetIdHint)
                    SameNet.Add(A);
                else
                    OtherNet.Add(A);
            }

            AActor* Pick = nullptr;
            if (SameNet.Num() > 0) Pick = R->PickNearestCandidate(Origin, SameNet);
            else if (OtherNet.Num() > 0) Pick = R->PickNearestCandidate(Origin, OtherNet);
            if (!IsValid(Pick)) return false;

            // ✅ 예약 점화
            if (ScheduleIgnitionFromSpread(SourceFire, R, Pick, Radius))
            {
                UE_LOG(LogFire, Warning, TEXT("[Room] ➜ Spread_Electric 예약 Room=%s Target=%s"),
                    *R->GetName(), *GetNameSafe(Pick));
                return true;
            }
            return false;
        };

    if (TryRoom(this)) return true;
    for (ARoomActor* N : AdjacentRooms)
        if (IsValid(N) && TryRoom(N)) return true;

    UE_LOG(LogFire, Log, TEXT("[Room] Spread_Electric 결과: 실패"));
    return false;
}

bool ARoomActor::Spread_Explosive(AFireActor* SourceFire)
{
    const FVector Origin = ResolveSpreadOrigin(SourceFire);
    const float Radius = SourceFire->CurrentSpreadRadius * 1.8f;
    const int32 MaxMulti = 4;

    UE_LOG(LogFire, Warning, TEXT("[Room] Spread_Explosive Fire=%s Origin=%s Radius=%.1f Max=%d"),
        *SourceFire->GetName(), *Origin.ToString(), Radius, MaxMulti);

    TArray<AActor*> Candidates;
    if (!CollectCandidates(Origin, Radius, this, Candidates))
    {
        UE_LOG(LogFire, Log, TEXT("[Room] Spread_Explosive 후보 없음"));
        return false;
    }

    Candidates.Sort([&](const AActor& L, const AActor& R)
        {
            return FVector::DistSquared(GetActorCheckPos(const_cast<AActor*>(&L)), Origin) <
                FVector::DistSquared(GetActorCheckPos(const_cast<AActor*>(&R)), Origin);
        });

    int32 Scheduled = 0;
    for (AActor* Pick : Candidates)
    {
        if (!IsValid(Pick)) continue;

        // ✅ 예약 점화
        if (ScheduleIgnitionFromSpread(SourceFire, this, Pick, Radius))
        {
            UE_LOG(LogFire, Warning, TEXT("[Room] ➜ ExplosiveSpread 예약 Target=%s"), *GetNameSafe(Pick));
            Scheduled++;
            if (Scheduled >= MaxMulti) break;
        }
    }

    UE_LOG(LogFire, Warning, TEXT("[Room] Spread_Explosive 결과: Scheduled=%d"), Scheduled);
    return Scheduled > 0;
}

void ARoomActor::GetCombustibleActorsInRoom(TArray<AActor*>& OutActors, bool bExcludeBurning) const
{
    OutActors.Reset();

    if (!GetWorld() || !IsValid(RoomBounds))
        return;

    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* A = *It;
        if (!IsValid(A)) continue;

        const FVector Pos = GetActorCheckPos(A);

        if (!IsInsideRoomBox(RoomBounds, Pos))
            continue;

        UCombustibleComponent* Comb = A->FindComponentByClass<UCombustibleComponent>();
        if (!Comb) continue;

        if (bExcludeBurning && BurningTargets.Contains(A))
            continue;

        // 예약 중 제외
        if (ReservedTargets.Contains(A))
            continue;

        OutActors.Add(A);
    }

    UE_LOG(LogFire, Log,
        TEXT("[Room] GetCombustibleActorsInRoom Room=%s Count=%d ExcludeBurning=%d"),
        *GetName(), OutActors.Num(), bExcludeBurning ? 1 : 0);
}

AFireActor* ARoomActor::IgniteRandomCombustibleInRoom(bool bAllowElectric)
{
    TArray<AActor*> Candidates;
    GetCombustibleActorsInRoom(Candidates, true);

    UE_LOG(LogFire, Warning,
        TEXT("[Room] RandomIgnite Room=%s Candidates=%d AllowElectric=%d"),
        *GetName(), Candidates.Num(), bAllowElectric ? 1 : 0);

    if (Candidates.Num() == 0)
    {
        UE_LOG(LogFire, Warning, TEXT("[Room] RandomIgnite 실패: 후보 0"));
        return nullptr;
    }

    Candidates.RemoveAll([&](AActor* A)
        {
            UCombustibleComponent* Comb = A ? A->FindComponentByClass<UCombustibleComponent>() : nullptr;
            if (!Comb) return true;

            if (Comb->CombustibleType == ECombustibleType::Electric)
            {
                if (!bAllowElectric) return true;
                if (!Comb->bElectricIgnitionTriggered) return true;
            }
            return false;
        });

    if (Candidates.Num() == 0)
    {
        UE_LOG(LogFire, Warning, TEXT("[Room] RandomIgnite 실패: 필터 후 후보 0"));
        return nullptr;
    }

    const int32 Index = FMath::RandRange(0, Candidates.Num() - 1);
    AActor* Target = Candidates[Index];

    UE_LOG(LogFire, Warning, TEXT("[Room] RandomIgnite PICK=%s"), *GetNameSafe(Target));

    return IgniteTarget(Target);
}
