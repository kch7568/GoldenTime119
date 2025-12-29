// ============================ RoomActor.cpp ============================
#include "RoomActor.h"

#include "FireActor.h"
#include "CombustibleComponent.h"
#include "DoorActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogFire, Log, All);

static FORCEINLINE FVector GetActorCenter(AActor* A)
{
    return IsValid(A) ? A->GetComponentsBoundingBox(true).GetCenter() : FVector::ZeroVector;
}

// 여러 Vent(0..1)를 “확률 합성”으로 결합: 1 - Π(1 - Vi)
static float CombineVents_Prob(const TArray<float>& Vents)
{
    float Inv = 1.f;
    for (float V : Vents)
        Inv *= (1.f - FMath::Clamp(V, 0.f, 1.f));
    return 1.f - Inv;
}

// ============================ ctor ============================
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

    RoomBounds->OnComponentBeginOverlap.AddDynamic(this, &ARoomActor::OnRoomBeginOverlap);
    RoomBounds->OnComponentEndOverlap.AddDynamic(this, &ARoomActor::OnRoomEndOverlap);

    FireClass = AFireActor::StaticClass();

    // ===== Default Policies =====
    PolicyNormal.InitialFuel = 12.f;
    PolicyNormal.ConsumePerSecond_Min = 0.7f;
    PolicyNormal.ConsumePerSecond_Max = 1.3f;
    PolicyNormal.SpreadRadius_Min = 280.f;
    PolicyNormal.SpreadRadius_Max = 900.f;
    PolicyNormal.SpreadInterval_Min = 0.55f;
    PolicyNormal.SpreadInterval_Max = 1.30f;

    PolicyOil = PolicyNormal;
    PolicyOil.InitialFuel = 16.f;
    PolicyOil.ConsumePerSecond_Min = 0.9f;
    PolicyOil.ConsumePerSecond_Max = 1.8f;
    PolicyOil.SpreadRadius_Min = 350.f;
    PolicyOil.SpreadRadius_Max = 1100.f;
    PolicyOil.SpreadInterval_Min = 0.50f;
    PolicyOil.SpreadInterval_Max = 1.15f;
    PolicyOil.HeatMul = 1.15f;
    PolicyOil.SmokeMul = 1.25f;

    PolicyElectric = PolicyNormal;
    PolicyElectric.InitialFuel = 10.f;
    PolicyElectric.SpreadRadius_Min = 260.f;
    PolicyElectric.SpreadRadius_Max = 850.f;

    PolicyExplosive = PolicyNormal;
    PolicyExplosive.InitialFuel = 3.f;
    PolicyExplosive.ConsumePerSecond_Min = 1.2f;
    PolicyExplosive.ConsumePerSecond_Max = 2.4f;
    PolicyExplosive.SpreadRadius_Min = 450.f;
    PolicyExplosive.SpreadRadius_Max = 1600.f;
    PolicyExplosive.SpreadInterval_Min = 0.35f;
    PolicyExplosive.SpreadInterval_Max = 0.90f;
    PolicyExplosive.HeatMul = 1.4f;
    PolicyExplosive.SmokeMul = 1.2f;
}

// ============================ BeginPlay / Tick ============================
void ARoomActor::BeginPlay()
{
    Super::BeginPlay();

    Debug_RescanCombustibles();
    UpdateRoomGeometryFromBounds();

    // ===== NeutralPlane init =====
    NP.NeutralPlaneZ = CeilingZ - MaxNeutralPlaneFromCeiling;
    NP.UpperSmoke01 = 0.f;
    NP.UpperTempC = 25.f;
    NP.Vent01 = 0.f;

    // ===== Smoke init (권위: NP/Lower에서 재구성) =====
    LowerSmoke01 = 0.f;
    Smoke = 0.f;

    // ===== Smoke volumes =====
    if (bEnableSmokeVolume)
        EnsureSmokeVolumesSpawned();

    // ===== Backdraft init =====
    SealedTime = 0.f;
    bBackdraftArmed = false;
    LastBackdraftTime = -1000.f;

    UE_LOG(LogFire, Warning,
        TEXT("[Room] BeginPlay Room=%s Combustibles=%d Doors=%d FloorZ=%.1f CeilingZ=%.1f NPZ=%.1f"),
        *GetName(), Combustibles.Num(), Doors.Num(), FloorZ, CeilingZ, NP.NeutralPlaneZ);
}

void ARoomActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // 1) Fire → Room 영향 반영 (주의: Smoke(권위)는 여기서 직접 누적하지 않음)
    ApplyAccumulators(DeltaSeconds);

    // 2) Door → Vent 합성 (멀티 도어)
    if (bEnableDoorVentAggregation)
        UpdateVentFromDoors();
    else
        NP.Vent01 = FMath::Clamp(NP.Vent01, 0.f, 1.f);

    // 3) Door 기반 방-방 / 방-밖 교환 (UpperSmoke/O2 중심)
    ApplyDoorExchange(DeltaSeconds);

    // 4) Neutral Plane 업데이트 (UpperSmoke01, NPZ, UpperTempC)
    if (bEnableNeutralPlane)
        UpdateNeutralPlane(DeltaSeconds);

    // 5) (핵심) NP -> LowerSmoke/Smoke 최종 합성
    RebuildSmokeFromNP(DeltaSeconds);
    ApplyOxygenCapBySmoke(DeltaSeconds);

    // 6) 화재 꺼짐/환기 시 env 회복 (연기 소실은 "문 환기"로만)
    RelaxEnv(DeltaSeconds);

    // 7) Backdraft 장전 평가 (sealed는 Vent 기반)
    if (bEnableBackdraft)
        EvaluateBackdraftArming(DeltaSeconds);

    // 8) Smoke Volumes
    if (bEnableSmokeVolume)
    {
        EnsureSmokeVolumesSpawned();
        UpdateSmokeVolumesTransform();
        PushSmokeMaterialParams();
    }

    // 9) Room State
    UpdateRoomState();

    // 10) 누적치 초기화
    ResetAccumulators();
}

// ============================ Policy / Influence ============================
const FFirePolicy& ARoomActor::GetPolicy(ECombustibleType Type) const
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

FRoomInfluence ARoomActor::BaseInfluence(ECombustibleType Type, float EffectiveIntensity) const
{
    FRoomInfluence R;
    switch (Type)
    {
    case ECombustibleType::Normal:
        R.HeatAdd = 12.f * EffectiveIntensity;
        R.SmokeAdd = 8.f * EffectiveIntensity;
        R.OxygenSub = 0.03f * EffectiveIntensity;
        R.FireValueAdd = 1.0f * EffectiveIntensity;
        break;
    case ECombustibleType::Oil:
        R.HeatAdd = 18.f * EffectiveIntensity;
        R.SmokeAdd = 12.f * EffectiveIntensity;
        R.OxygenSub = 0.05f * EffectiveIntensity;
        R.FireValueAdd = 1.6f * EffectiveIntensity;
        break;
    case ECombustibleType::Electric:
        R.HeatAdd = 10.f * EffectiveIntensity;
        R.SmokeAdd = 6.f * EffectiveIntensity;
        R.OxygenSub = 0.02f * EffectiveIntensity;
        R.FireValueAdd = 1.2f * EffectiveIntensity;
        break;
    case ECombustibleType::Explosive:
        R.HeatAdd = 25.f * EffectiveIntensity;
        R.SmokeAdd = 15.f * EffectiveIntensity;
        R.OxygenSub = 0.07f * EffectiveIntensity;
        R.FireValueAdd = 2.5f * EffectiveIntensity;
        break;
    }
    return R;
}

void ARoomActor::AccumulateInfluence(ECombustibleType Type, float EffectiveIntensity, float InfluenceScale)
{
    const FFirePolicy& P = GetPolicy(Type);
    FRoomInfluence I = BaseInfluence(Type, EffectiveIntensity);

    I.HeatAdd *= (P.HeatMul * InfluenceScale);
    I.SmokeAdd *= (P.SmokeMul * InfluenceScale);
    I.OxygenSub *= (P.OxygenMul * InfluenceScale);
    I.FireValueAdd *= (P.FireValueMul * InfluenceScale);

    AccHeat += I.HeatAdd;
    AccSmoke += I.SmokeAdd;
    AccOxygenSub += I.OxygenSub;
    AccFireValue += I.FireValueAdd;
}

bool ARoomActor::GetRuntimeTuning(ECombustibleType Type, float EffectiveIntensity, float FuelRatio01, FFireRuntimeTuning& Out) const
{
    const FFirePolicy& P = GetPolicy(Type);

    const float RawIntensity01 = (P.IntensityRef > 0.f) ? (EffectiveIntensity / P.IntensityRef) : 1.f;
    const float Intensity01 = FMath::Clamp(
        FMath::Pow(FMath::Clamp(RawIntensity01, 0.f, 1.f), FMath::Max(0.1f, P.IntensityPow)),
        0.f, 1.f
    );

    const float Fuel01 = FMath::Clamp(FuelRatio01, 0.f, 1.f);
    const float SpreadAlpha = FMath::Clamp(Fuel01 * (0.35f + 0.65f * Intensity01), 0.f, 1.f);

    Out.FuelRatio01 = Fuel01;
    Out.Intensity01 = Intensity01;

    Out.SpreadRadius = FMath::Lerp(P.SpreadRadius_Min, P.SpreadRadius_Max, SpreadAlpha);
    Out.SpreadInterval = FMath::Lerp(P.SpreadInterval_Max, P.SpreadInterval_Min, Intensity01);
    Out.ConsumePerSecond = FMath::Lerp(P.ConsumePerSecond_Min, P.ConsumePerSecond_Max, Intensity01);

    Out.InfluenceScale = FMath::Clamp((0.5f + 0.5f * Intensity01) * (0.3f + 0.7f * Fuel01), 0.f, 3.f);
    return true;
}

// ============================ Door registry ============================
void ARoomActor::RegisterDoor(ADoorActor* Door)
{
    if (!IsValid(Door)) return;
    Doors.AddUnique(Door);
}

void ARoomActor::UnregisterDoor(ADoorActor* Door)
{
    if (!Door) return;
    Doors.Remove(Door);
}

// ============================ Vent aggregation ============================
void ARoomActor::UpdateVentFromDoors()
{
    Doors.RemoveAll([](const TWeakObjectPtr<ADoorActor>& W) { return !W.IsValid(); });

    TArray<float> Vents;
    Vents.Reserve(Doors.Num());

    for (const TWeakObjectPtr<ADoorActor>& W : Doors)
    {
        const ADoorActor* D = W.Get();
        if (!IsValid(D)) continue;

        Vents.Add(D->ComputeVent01());
    }

    const float Agg = CombineVents_Prob(Vents);
    NP.Vent01 = FMath::Clamp(Agg, 0.f, VentAggregateClampMax);
}

// ============================ Door exchange (UpperSmoke/O2 권위) ============================
// - UpperSmoke01: 방<->방은 차이만큼 이동, 방<->밖은 제거(=문 환기)
// - Oxygen: 방<->방은 평형화, 방<->밖은 1.0으로 회복
void ARoomActor::ApplyDoorExchange(float DeltaSeconds)
{
    if (Doors.Num() <= 0) return;

    Doors.RemoveAll([](const TWeakObjectPtr<ADoorActor>& W) { return !W.IsValid(); });

    for (const TWeakObjectPtr<ADoorActor>& W : Doors)
    {
        ADoorActor* Door = W.Get();
        if (!IsValid(Door)) continue;

        const float Vent = FMath::Clamp(Door->ComputeVent01(), 0.f, 1.f);
        if (Vent <= KINDA_SMALL_NUMBER) continue;

        ARoomActor* Other = Door->GetOtherRoom(this);
        const bool bOutside = Door->IsOutsideConnectionFor(this);

        // 1) UpperSmoke01 교환/배출
        if (bOutside || !IsValid(Other))
        {
            // 밖으로 배출 (문 환기)
            const float Remove = DoorSmokeExchangeRate * Vent * DeltaSeconds;
            NP.UpperSmoke01 = FMath::Clamp(NP.UpperSmoke01 - Remove, 0.f, 1.f);

            // 하층도 약하게 같이 빠짐(연출)
            LowerSmoke01 = FMath::Clamp(LowerSmoke01 - Remove * 0.25f, 0.f, 1.f);
        }
        else
        {
            // 방-방: 높은 쪽 -> 낮은 쪽 이동
            const float A = NP.UpperSmoke01;
            const float B = Other->NP.UpperSmoke01;
            const float Diff = (A - B);

            if (FMath::Abs(Diff) > KINDA_SMALL_NUMBER)
            {
                const float Move = FMath::Clamp(Diff * DoorSmokeExchangeRate * Vent * DeltaSeconds, -1.f, 1.f);

                NP.UpperSmoke01 = FMath::Clamp(NP.UpperSmoke01 - Move, 0.f, 1.f);
                Other->NP.UpperSmoke01 = FMath::Clamp(Other->NP.UpperSmoke01 + Move, 0.f, 1.f);

                // 하층도 약하게 따라감(연출)
                LowerSmoke01 = FMath::Clamp(LowerSmoke01 - Move * 0.25f, 0.f, 1.f);
                Other->LowerSmoke01 = FMath::Clamp(Other->LowerSmoke01 + Move * 0.25f, 0.f, 1.f);
            }
        }

        // 2) Oxygen 평형화
        const float TargetO2 = (bOutside || !IsValid(Other)) ? 1.f : Other->Oxygen;
        const float O2New = FMath::FInterpTo(Oxygen, TargetO2, DeltaSeconds, DoorOxygenExchangeRate * Vent);
        Oxygen = FMath::Clamp(O2New, 0.f, 1.f);
    }
}

// ============================ Smoke 연동(핵심) ============================
void ARoomActor::RebuildSmokeFromNP(float DeltaSeconds)
{
    const float Upper = FMath::Clamp(NP.UpperSmoke01, 0.f, 1.f);

    // 하층은 상층의 1/4 (요구사항)
    const float TargetLower = FMath::Clamp(Upper * LowerSmokeTargetRatio, 0.f, 1.f);
    LowerSmoke01 = FMath::FInterpTo(LowerSmoke01, TargetLower, DeltaSeconds, FMath::Max(0.01f, LowerSmokeFollowSpeed));

    // 최종 Smoke (권위: UI/상태/백드래프트 판정)
    Smoke = FMath::Clamp(Upper * UpperSmokeWeight + LowerSmoke01 * LowerSmokeWeight, 0.f, 1.f);
}

// ============================ Env relax (화재 꺼짐/환기) ============================
// 중요: "자연 vent 없음" 모드
// - Heat/FireValue/Oxygen은 서서히 회복 가능
// - Smoke는 "문 환기"로만 감소(=Vent>0일 때만)
void ARoomActor::RelaxEnv(float DeltaSeconds)
{
    const bool bNoFire = (ActiveFires.Num() <= 0);
    const float Vent = FMath::Clamp(NP.Vent01, 0.f, 1.f);

    // 환기가 있거나(문 열림) 불이 없으면 회복 가속
    const float VentBoost = (0.5f + 1.5f * Vent);
    const float NoFireBoost = bNoFire ? 1.0f : 0.35f;

    Heat = FMath::FInterpTo(Heat, 0.f, DeltaSeconds, HeatCoolToAmbientPerSec * VentBoost * NoFireBoost);
    FireValue = FMath::FInterpTo(FireValue, 0.f, DeltaSeconds, FireValueDecayPerSec * VentBoost);
    Oxygen = FMath::FInterpTo(Oxygen, 1.f, DeltaSeconds, OxygenRecoverPerSec * VentBoost);

    // ✅ Smoke는 "문 환기"로만 소실 (Vent=0이면 여기서 손대지 않음)
    if (Vent > KINDA_SMALL_NUMBER)
    {
        const float Dissip = SmokeNaturalDissipatePerSec * Vent * DeltaSeconds;
        NP.UpperSmoke01 = FMath::Clamp(NP.UpperSmoke01 - Dissip, 0.f, 1.f);
        LowerSmoke01 = FMath::Clamp(LowerSmoke01 - Dissip * 0.5f, 0.f, 1.f);
    }
}

// ============================ Combustible / Fire registry ============================
void ARoomActor::RegisterCombustible(UCombustibleComponent* Comb)
{
    if (!IsValid(Comb)) return;
    Combustibles.Add(Comb);
    Comb->SetOwningRoom(this);
}

void ARoomActor::UnregisterCombustible(UCombustibleComponent* Comb)
{
    if (!IsValid(Comb)) return;

    Combustibles.Remove(Comb);
    if (Comb->GetOwningRoom() == this)
        Comb->SetOwningRoom(nullptr);
}

void ARoomActor::RegisterFire(AFireActor* Fire)
{
    if (!IsValid(Fire)) return;
    ActiveFires.Add(Fire->FireID, Fire);
    OnFireStarted.Broadcast(Fire);
}

void ARoomActor::UnregisterFire(const FGuid& FireId)
{
    AFireActor* Fire = nullptr;
    if (TObjectPtr<AFireActor>* Found = ActiveFires.Find(FireId))
        Fire = Found->Get();

    ActiveFires.Remove(FireId);

    if (IsValid(Fire))
        OnFireExtinguished.Broadcast(Fire);
}

FRoomEnvSnapshot ARoomActor::GetEnvSnapshot() const
{
    FRoomEnvSnapshot S;
    S.Heat = Heat;
    S.Smoke = Smoke;
    S.Oxygen = Oxygen;
    S.FireValue = FireValue;
    S.State = State;
    return S;
}

// ============================ Fire spawning ============================
AFireActor* ARoomActor::SpawnFireForCombustible(UCombustibleComponent* Comb, ECombustibleType Type)
{
    if (!IsValid(Comb) || !GetWorld()) return nullptr;

    AActor* OwnerActor = Comb->GetOwner();
    if (!IsValid(OwnerActor)) return nullptr;

    Comb->EnsureFuelInitialized();

    if (!FireClass)
        FireClass = AFireActor::StaticClass();

    const FVector TargetCenter = OwnerActor->GetActorLocation();
    const FTransform SpawnTM(FRotator::ZeroRotator, TargetCenter);

    AFireActor* NewFire = GetWorld()->SpawnActorDeferred<AFireActor>(
        FireClass, SpawnTM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    if (!IsValid(NewFire))
    {
        UE_LOG(LogFire, Error, TEXT("[Room] SpawnFireForCombustible failed: SpawnActorDeferred"));
        return nullptr;
    }

    NewFire->SpawnRoom = this;
    NewFire->SpawnType = Type;
    NewFire->IgnitedTarget = OwnerActor;
    NewFire->LinkedCombustible = Comb;

    NewFire->InitFire(this, Type);
    NewFire->FinishSpawning(SpawnTM);

    NewFire->SetActorLocation(TargetCenter, false, nullptr, ETeleportType::TeleportPhysics);
    NewFire->AttachToActor(OwnerActor, FAttachmentTransformRules::KeepWorldTransform);
    NewFire->SetActorRelativeLocation(FVector::ZeroVector);
    NewFire->SetActorRelativeRotation(FRotator::ZeroRotator);

    UE_LOG(LogFire, Warning, TEXT("[Room] FireSpawned Fire=%s Id=%s Target=%s Loc=%s"),
        *GetNameSafe(NewFire),
        *NewFire->FireID.ToString(),
        *GetNameSafe(OwnerActor),
        *NewFire->GetActorLocation().ToString());

    Comb->ActiveFire = NewFire;
    Comb->bIsBurning = true;
    Comb->Ignition.IgnitionProgress01 = 0.f;

    OnFireSpawned.Broadcast(NewFire);
    return NewFire;
}

// ============================ Combustible listing / rescan ============================
void ARoomActor::GetCombustiblesInRoom(TArray<UCombustibleComponent*>& Out, bool bExcludeBurning) const
{
    Out.Reset();
    for (const TWeakObjectPtr<UCombustibleComponent>& W : Combustibles)
    {
        UCombustibleComponent* C = W.Get();
        if (!IsValid(C)) continue;
        if (bExcludeBurning && C->IsBurning()) continue;
        Out.Add(C);
    }
}

void ARoomActor::Debug_RescanCombustibles()
{
    if (!GetWorld() || !IsValid(RoomBounds)) return;

    int32 Added = 0;

    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* A = *It;
        if (!IsValid(A)) continue;

        const FVector Pos = GetActorCenter(A);
        if (!IsInsideRoomBox(RoomBounds, Pos))
            continue;

        UCombustibleComponent* Comb = A->FindComponentByClass<UCombustibleComponent>();
        if (!Comb) continue;

        if (!Combustibles.Contains(Comb))
        {
            RegisterCombustible(Comb);
            Added++;
        }
    }

    UE_LOG(LogFire, Warning, TEXT("[Room] Rescan Room=%s Added=%d Total=%d"), *GetName(), Added, Combustibles.Num());
}

// ============================ Env apply/reset/state ============================
void ARoomActor::ApplyAccumulators(float DeltaSeconds)
{
    Heat += AccHeat * DeltaSeconds;
    FireValue += AccFireValue * DeltaSeconds;

    Oxygen = FMath::Clamp(Oxygen - (AccOxygenSub * DeltaSeconds), 0.f, 1.f);

    // 중요:
    // - Smoke(권위)는 NP.UpperSmoke01 + LowerSmoke01(=Upper의 1/4)로 RebuildSmokeFromNP()에서 결정.
    // - 즉, Smoke를 여기서 직접 누적하지 않습니다.
}

void ARoomActor::ResetAccumulators()
{
    AccHeat = AccSmoke = AccOxygenSub = AccFireValue = 0.f;
}

void ARoomActor::UpdateRoomState()
{
    const int32 FireCount = ActiveFires.Num();
    const ERoomState Prev = State;

    const bool bSmokeDanger = (Smoke >= 0.85f);      // 임계는 튜닝
    const bool bO2Danger = (Oxygen <= 0.22f);     // “0.2 즈음”
    const bool bHeatDanger = (Heat >= RiskHeatThreshold);

    if (FireCount > 0)
    {
        State = ERoomState::Fire;
    }
    else
    {
        State = (bSmokeDanger || bO2Danger || bHeatDanger) ? ERoomState::Risk : ERoomState::Idle;
    }

    if (Prev != State)
    {
        UE_LOG(LogFire, Warning, TEXT("[Room] StateChange Room=%s %d -> %d (Heat=%.1f Smoke=%.2f O2=%.2f Fires=%d)"),
            *GetName(), (int32)Prev, (int32)State, Heat, Smoke, Oxygen, FireCount);
    }
}


// ============================ Geometry / NP ============================
bool ARoomActor::IsInsideRoomBox(const UBoxComponent* Box, const FVector& WorldPos)
{
    if (!Box) return false;
    const FVector Local = Box->GetComponentTransform().InverseTransformPosition(WorldPos);
    const FVector Extent = Box->GetScaledBoxExtent();
    return FMath::Abs(Local.X) <= Extent.X
        && FMath::Abs(Local.Y) <= Extent.Y
        && FMath::Abs(Local.Z) <= Extent.Z;
}

void ARoomActor::UpdateRoomGeometryFromBounds()
{
    if (!IsValid(RoomBounds)) return;

    const FVector Center = RoomBounds->GetComponentLocation();
    const FVector Extent = RoomBounds->GetScaledBoxExtent();

    FloorZ = Center.Z - Extent.Z;
    CeilingZ = Center.Z + Extent.Z;
}

void ARoomActor::UpdateNeutralPlane(float DeltaSeconds)
{
    UpdateRoomGeometryFromBounds();

    // 1) UpperSmoke01 accumulate (from AccSmoke)
    const float FillSlowdown = (1.f - NP.UpperSmoke01);
    const float Add = AccSmoke * SmokeToUpperFillRate * FillSlowdown * DeltaSeconds;
    NP.UpperSmoke01 = FMath::Clamp(NP.UpperSmoke01 + Add, 0.f, 1.f);

    // 2) Vent remove (문 환기)
    const float VentRemove = NP.Vent01 * VentSmokeRemoveRate * DeltaSeconds;
    NP.UpperSmoke01 = FMath::Clamp(NP.UpperSmoke01 - VentRemove, 0.f, 1.f);

    // ✅ 3) 자연 감쇠(=문 외 vent) 제거
    // 요구사항: "Door 외 자연 vent 요인 없어야 정상"
    // -> 여기서 UpperSmoke01을 곱으로 줄이는 처리는 넣지 않습니다.

    // 4) Target height
    const float MinZ = FloorZ + MinNeutralPlaneFromFloor;
    const float MaxZ = CeilingZ - MaxNeutralPlaneFromCeiling;

    const float T = FMath::Clamp(NP.UpperSmoke01, 0.f, 1.f);
    const float EaseT = FMath::Pow(T, 1.2f);
    const float TargetZ = FMath::Lerp(MaxZ, MinZ, EaseT);

    // 5) Speed
    const bool bGoingDown = (TargetZ < NP.NeutralPlaneZ);
    const float Speed = bGoingDown
        ? NeutralPlaneDropPerSec * (0.25f + 0.75f * T)
        : NeutralPlaneRisePerSec * (0.25f + 0.75f * NP.Vent01);

    NP.NeutralPlaneZ = FMath::FInterpConstantTo(NP.NeutralPlaneZ, TargetZ, DeltaSeconds, Speed);
    NP.NeutralPlaneZ = FMath::Clamp(NP.NeutralPlaneZ, MinZ, MaxZ);

    // 6) Upper temperature
    const float Heat01 = FMath::Clamp(Heat / 600.f, 0.f, 1.f);
    const float TempTarget = FMath::Lerp(25.f, 650.f, Heat01);
    NP.UpperTempC = FMath::FInterpTo(NP.UpperTempC, TempTarget, DeltaSeconds, 0.25f);
}

// ============================ Overlap ============================
void ARoomActor::OnRoomBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!IsValid(OtherActor) || OtherActor == this) return;

    UCombustibleComponent* Comb = OtherActor->FindComponentByClass<UCombustibleComponent>();
    if (Comb)
    {
        RegisterCombustible(Comb);
        UE_LOG(LogFire, VeryVerbose, TEXT("[Room] Overlap+ Combustible=%s"), *GetNameSafe(OtherActor));
    }
}

void ARoomActor::OnRoomEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!IsValid(OtherActor)) return;

    UCombustibleComponent* Comb = OtherActor->FindComponentByClass<UCombustibleComponent>();
    if (Comb)
    {
        UnregisterCombustible(Comb);
        UE_LOG(LogFire, VeryVerbose, TEXT("[Room] Overlap- Combustible=%s"), *GetNameSafe(OtherActor));
    }
}

// ============================ Ignite helpers ============================
AFireActor* ARoomActor::IgniteActor(AActor* TargetActor)
{
    if (!IsValid(TargetActor) || !IsValid(GetWorld())) return nullptr;

    UCombustibleComponent* Comb = TargetActor->FindComponentByClass<UCombustibleComponent>();
    if (!Comb) return nullptr;

    Comb->SetOwningRoom(this);

    if (Comb->IsBurning())
        return Comb->ActiveFire;

    if (Comb->CombustibleType == ECombustibleType::Electric && !Comb->bElectricIgnitionTriggered)
        return nullptr;

    return SpawnFireForCombustible(Comb, Comb->CombustibleType);
}

AFireActor* ARoomActor::IgniteRandomCombustibleInRoom(bool bAllowElectric)
{
    TArray<UCombustibleComponent*> List;
    GetCombustiblesInRoom(List, true);

    List.RemoveAll([&](UCombustibleComponent* C)
        {
            if (!IsValid(C) || !IsValid(C->GetOwner())) return true;
            if (C->CombustibleType == ECombustibleType::Electric)
            {
                if (!bAllowElectric) return true;
                if (!C->bElectricIgnitionTriggered) return true;
            }
            return false;
        });

    if (List.Num() <= 0) return nullptr;

    const int32 Index = FMath::RandRange(0, List.Num() - 1);
    UCombustibleComponent* Pick = List[Index];
    if (!IsValid(Pick) || !IsValid(Pick->GetOwner())) return nullptr;

    return IgniteActor(Pick->GetOwner());
}

// ============================ SmokeVolume (상층/하층 2개) ============================
void ARoomActor::EnsureSmokeVolumesSpawned()
{
    if (!bEnableSmokeVolume) return;
    if (!GetWorld() || !SmokeVolumeClass) return;

    auto SpawnOne = [&](TObjectPtr<AActor>& OutActor,
        TObjectPtr<UStaticMeshComponent>& OutMesh,
        TObjectPtr<UMaterialInstanceDynamic>& OutMID,
        const TCHAR* Tag)
        {
            if (IsValid(OutActor)) return;

            UpdateRoomGeometryFromBounds();

            const FVector SpawnLoc(GetActorLocation().X, GetActorLocation().Y, CeilingZ - SmokeCeilingAttachOffset);
            const FTransform TM(FRotator::ZeroRotator, SpawnLoc);

            FActorSpawnParameters SP;
            SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            OutActor = GetWorld()->SpawnActor<AActor>(SmokeVolumeClass, TM, SP);
            if (!IsValid(OutActor)) return;

            OutActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
            OutActor->Tags.Add(FName(Tag));

            OutMesh = OutActor->FindComponentByClass<UStaticMeshComponent>();
            if (!IsValid(OutMesh))
            {
                UE_LOG(LogFire, Error, TEXT("[Room] SmokeVolume(%s) has NO StaticMeshComponent. BP_Smoke에 StaticMeshComponent를 추가하세요."), Tag);
                return;
            }

            OutMID = OutMesh->CreateDynamicMaterialInstance(0);
            if (!IsValid(OutMID))
            {
                UE_LOG(LogFire, Error, TEXT("[Room] CreateDynamicMaterialInstance failed. Tag=%s Room=%s"), Tag, *GetName());
                return;
            }
        };

    SpawnOne(UpperSmokeActor, UpperSmokeMesh, UpperSmokeMID, TEXT("UpperSmoke"));
    SpawnOne(LowerSmokeActor, LowerSmokeMesh, LowerSmokeMID, TEXT("LowerSmoke"));
}

void ARoomActor::UpdateSmokeVolumesTransform()
{
    if (!IsValid(RoomBounds)) return;

    UpdateRoomGeometryFromBounds();

    const FVector Center = RoomBounds->GetComponentLocation();
    const FVector Extent = RoomBounds->GetScaledBoxExtent();

    const float FullX = (Extent.X * 2.f) * SmokeXYInset;
    const float FullY = (Extent.Y * 2.f) * SmokeXYInset;

    const float TopZ = CeilingZ - SmokeCeilingAttachOffset;
    const float NPZ = FMath::Clamp(NP.NeutralPlaneZ, FloorZ, TopZ);

    // -------- Upper (Ceiling -> NP) --------
    const float UpperHeight = FMath::Max(1.f, TopZ - NPZ);

    if (IsValid(UpperSmokeActor))
    {
        const float SX = FullX / FMath::Max(1.f, SmokeCubeBaseSize);
        const float SY = FullY / FMath::Max(1.f, SmokeCubeBaseSize);
        const float SZ = UpperHeight / FMath::Max(1.f, SmokeCubeBaseSize);

        const float ZMid = TopZ - (UpperHeight * 0.5f);

        UpperSmokeActor->SetActorLocation(FVector(Center.X, Center.Y, ZMid));
        UpperSmokeActor->SetActorRotation(FRotator::ZeroRotator);
        UpperSmokeActor->SetActorScale3D(FVector(SX, SY, SZ));
    }

    // -------- Lower (바닥 근처 얇은 층: 상층 높이의 1/4) --------
    if (IsValid(LowerSmokeActor))
    {
        const float DesiredLowerH = FMath::Max(1.f, UpperHeight * FMath::Clamp(LowerVolumeHeightRatioToUpper, 0.05f, 0.5f));
        const float RoomH = FMath::Max(1.f, TopZ - FloorZ);
        const float LowerHeight = FMath::Min(DesiredLowerH, RoomH);

        const float SX = FullX / FMath::Max(1.f, SmokeCubeBaseSize);
        const float SY = FullY / FMath::Max(1.f, SmokeCubeBaseSize);
        const float SZ = LowerHeight / FMath::Max(1.f, SmokeCubeBaseSize);

        const float ZMid = FloorZ + (LowerHeight * 0.5f);

        LowerSmokeActor->SetActorLocation(FVector(Center.X, Center.Y, ZMid));
        LowerSmokeActor->SetActorRotation(FRotator::ZeroRotator);
        LowerSmokeActor->SetActorScale3D(FVector(SX, SY, SZ));
    }
}

void ARoomActor::PushSmokeMaterialParams()
{
    // Smoke(최종 합성) 기반으로 불투명 부스트 계산
    const float S = FMath::Clamp(Smoke, 0.f, 1.f);

    // 0..1로 정규화 (OpaqueStart ~ OpaqueFull)
    const float Den = FMath::Max(0.0001f, (OpaqueFullSmoke01 - OpaqueStartSmoke01));
    const float X = FMath::Clamp((S - OpaqueStartSmoke01) / Den, 0.f, 1.f);

    // 임계 이후 급격 상승
    const float Hard = FMath::Pow(X, FMath::Max(1.f, OpaqueCurvePow));

    // 기본=1, 최대=OpaqueBoostMul
    const float Boost = FMath::Lerp(1.f, OpaqueBoostMul, Hard);

    if (IsValid(UpperSmokeMID))
    {
        UpperSmokeMID->SetScalarParameterValue(TEXT("NeutralPlaneZ"), NP.NeutralPlaneZ);
        UpperSmokeMID->SetScalarParameterValue(TEXT("FadeHeight"), FMath::Max(1.f, SmokeFadeHeight));

        const float UpperOpacity = UpperSmokeOpacity * Boost * NP.UpperSmoke01;
        UpperSmokeMID->SetScalarParameterValue(TEXT("Opacity"), FMath::Clamp(UpperOpacity, 0.f, 3.0f));
    }

    if (IsValid(LowerSmokeMID))
    {
        LowerSmokeMID->SetScalarParameterValue(TEXT("NeutralPlaneZ"), NP.NeutralPlaneZ);
        LowerSmokeMID->SetScalarParameterValue(TEXT("FadeHeight"), FMath::Max(1.f, SmokeFadeHeight));

        const float LowerOpacity = UpperSmokeOpacity * LowerSmokeOpacityScale * Boost * LowerSmoke01;
        LowerSmokeMID->SetScalarParameterValue(TEXT("Opacity"), FMath::Clamp(LowerOpacity, 0.f, 3.0f));
    }
}

// ============================ Backdraft ============================
bool ARoomActor::CanArmBackdraft() const
{
    if (!bEnableBackdraft) return false;

    if (Backdraft.bDisallowWhenRoomOnFire && State == ERoomState::Fire)
        return false;

    // Smoke는 최종 합성값(Upper+Lower)로 판정
    if (Smoke < Backdraft.SmokeMin) return false;
    if (FireValue < Backdraft.FireValueMin) return false;
    if (Oxygen > Backdraft.O2Max) return false;
    if (Heat < Backdraft.HeatMin) return false;

    return true;
}

void ARoomActor::NotifyDoorSealed(bool bSealed)
{
    // 멀티 도어에서는 Door->true를 신뢰하지 않음.
    // false(열림/파손)만 “즉시 리셋” 용도로 사용
    if (bSealed) return;
    SealedTime = 0.f;
    bBackdraftArmed = false;
}

void ARoomActor::EvaluateBackdraftArming(float DeltaSeconds)
{
    if (!bEnableBackdraft) return;

    const bool bSealedNow = (NP.Vent01 <= SealEpsilonVent01);

    if (bSealedNow && CanArmBackdraft())
        SealedTime += DeltaSeconds;
    else
        SealedTime = 0.f;

    bBackdraftArmed = (SealedTime >= Backdraft.ArmedHoldSeconds);
}

void ARoomActor::TriggerBackdraft(const FTransform& DoorTM, float VentBoost01)
{
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    if (Now - LastBackdraftTime < Backdraft.CooldownSeconds)
        return;

    if (!bBackdraftArmed)
        return;

    LastBackdraftTime = Now;
    bBackdraftArmed = false;
    SealedTime = 0.f;

    const float Boost = FMath::Clamp(VentBoost01, 0.f, 1.f) * Backdraft.VentBoostOnTrigger;
    NP.Vent01 = FMath::Clamp(NP.Vent01 + Boost, 0.f, 1.f);

    FireValue += Backdraft.FireValueBoost;

    // 연기 일부 급격 감소(상층 중심)
    NP.UpperSmoke01 = FMath::Clamp(NP.UpperSmoke01 - Backdraft.SmokeDropOnTrigger, 0.f, 1.f);
    LowerSmoke01 = FMath::Clamp(LowerSmoke01 - Backdraft.SmokeDropOnTrigger * 0.25f, 0.f, 1.f);

    OnBackdraft.Broadcast();

    if (bIgniteOnBackdraft)
    {
        // 전기 포함 여부는 원하는대로
        AFireActor* Reignite = IgniteRandomCombustibleInRoom(/*bAllowElectric=*/true);
        if (IsValid(Reignite))
        {
            UE_LOG(LogFire, Warning, TEXT("[Room] Backdraft reignited: %s"), *GetNameSafe(Reignite));
        }
    }

    UE_LOG(LogFire, Warning, TEXT("[Room] BACKDRAFT! Room=%s Smoke=%.2f Upper=%.2f Lower=%.2f O2=%.2f FireValue=%.1f Heat=%.1f Vent=%.2f"),
        *GetName(), Smoke, NP.UpperSmoke01, LowerSmoke01, Oxygen, FireValue, Heat, NP.Vent01);
}

// Tick()에서 RebuildSmokeFromNP() 이후, UpdateRoomState() 이전 정도에 넣기 추천
void ARoomActor::ApplyOxygenCapBySmoke(float DeltaSeconds)
{
    const float S = FMath::Clamp(Smoke, 0.f, 1.f);

    // Smoke 0 -> 1.0, Smoke 1 -> 0.2
    const float O2Cap = FMath::Lerp(1.0f, 0.2f, FMath::Pow(S, 1.2f));

    // "상한"만 걸기 (산소를 억지로 올리진 않음)
    Oxygen = FMath::Min(Oxygen, O2Cap);
}
