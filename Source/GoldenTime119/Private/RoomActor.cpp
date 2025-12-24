// ============================ RoomActor.cpp ============================
#include "RoomActor.h"

#include "FireActor.h"
#include "CombustibleComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogFire, Log, All);

static FORCEINLINE FVector GetActorCenter(AActor* A)
{
    return IsValid(A) ? A->GetComponentsBoundingBox(true).GetCenter() : FVector::ZeroVector;
}

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

    // 기본 정책(원하면 BP에서 조정)
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

void ARoomActor::BeginPlay()
{
    Super::BeginPlay();

    // 초기 스캔(Overlap이 누락될 수 있으니 보조로 한번)
    Debug_RescanCombustibles();

    UE_LOG(LogFire, Warning, TEXT("[Room] BeginPlay Room=%s Combustibles=%d"), *GetName(), Combustibles.Num());
}

void ARoomActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    ApplyAccumulators(DeltaSeconds);
    UpdateRoomState();
    ResetAccumulators();
}

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

    // 연료/강도 혼합으로 반경, 주기 결정
    const float SpreadAlpha = FMath::Clamp(Fuel01 * (0.35f + 0.65f * Intensity01), 0.f, 1.f);

    Out.FuelRatio01 = Fuel01;
    Out.Intensity01 = Intensity01;

    Out.SpreadRadius = FMath::Lerp(P.SpreadRadius_Min, P.SpreadRadius_Max, SpreadAlpha);
    Out.SpreadInterval = FMath::Lerp(P.SpreadInterval_Max, P.SpreadInterval_Min, Intensity01);
    Out.ConsumePerSecond = FMath::Lerp(P.ConsumePerSecond_Min, P.ConsumePerSecond_Max, Intensity01);

    Out.InfluenceScale = FMath::Clamp((0.5f + 0.5f * Intensity01) * (0.3f + 0.7f * Fuel01), 0.f, 3.f);
    return true;
}

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

AFireActor* ARoomActor::SpawnFireForCombustible(UCombustibleComponent* Comb, ECombustibleType Type)
{
    if (!IsValid(Comb) || !GetWorld())
        return nullptr;

    AActor* OwnerActor = Comb->GetOwner();
    if (!IsValid(OwnerActor))
        return nullptr;

    Comb->EnsureFuelInitialized();

    if (!FireClass)
        FireClass = AFireActor::StaticClass();

    const FVector TargetCenter = OwnerActor->GetComponentsBoundingBox(true).GetCenter();
    const FTransform SpawnTM(FRotator::ZeroRotator, TargetCenter);

    AFireActor* NewFire = GetWorld()->SpawnActorDeferred<AFireActor>(
        FireClass, SpawnTM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    if (!IsValid(NewFire))
    {
        UE_LOG(LogFire, Error, TEXT("[Room] SpawnFireForCombustible failed: SpawnActorDeferred"));
        return nullptr;
    }

    // Fire 초기화: 타겟/룸/타입 주입
    NewFire->SpawnRoom = this;
    NewFire->SpawnType = Type;
    NewFire->IgnitedTarget = OwnerActor;
    NewFire->LinkedCombustible = Comb;

    // Init before BeginPlay
    NewFire->InitFire(this, Type);

    NewFire->FinishSpawning(SpawnTM);

    // 0,0,0 문제 방지: 스폰 후 강제 위치 + 어태치
    NewFire->SetActorLocation(TargetCenter, false, nullptr, ETeleportType::TeleportPhysics);
    NewFire->AttachToActor(OwnerActor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    NewFire->SetActorRelativeLocation(FVector::ZeroVector);
    NewFire->SetActorRelativeRotation(FRotator::ZeroRotator);
    NewFire->SetActorLocation(TargetCenter, false, nullptr, ETeleportType::TeleportPhysics);

    UE_LOG(LogFire, Warning, TEXT("[Room] FireSpawned Fire=%s Id=%s Target=%s Loc=%s"),
        *GetNameSafe(NewFire),
        *NewFire->FireID.ToString(),
        *GetNameSafe(OwnerActor),
        *NewFire->GetActorLocation().ToString());

    Comb->ActiveFire = NewFire;
    Comb->bIsBurning = true;

    // 점화 진행도는 붙었으니 리셋(선택)
    Comb->Ignition.IgnitionProgress01 = 0.f;

    OnFireSpawned.Broadcast(NewFire);
    return NewFire;
}


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

void ARoomActor::ApplyAccumulators(float DeltaSeconds)
{
    Heat += AccHeat * DeltaSeconds;
    Smoke += AccSmoke * DeltaSeconds;
    FireValue += AccFireValue * DeltaSeconds;

    Oxygen = FMath::Clamp(Oxygen - (AccOxygenSub * DeltaSeconds), 0.f, 1.f);
}

void ARoomActor::ResetAccumulators()
{
    AccHeat = AccSmoke = AccOxygenSub = AccFireValue = 0.f;
}

void ARoomActor::UpdateRoomState()
{
    const int32 FireCount = ActiveFires.Num();
    const ERoomState Prev = State;

    if (FireCount <= 0)
        State = (Heat >= RiskHeatThreshold || Smoke > 0.2f) ? ERoomState::Risk : ERoomState::Idle;
    else
        State = ERoomState::Fire;

    if (Prev != State)
    {
        UE_LOG(LogFire, Warning, TEXT("[Room] StateChange Room=%s %d -> %d (Heat=%.1f Smoke=%.2f O2=%.2f Fires=%d)"),
            *GetName(), (int32)Prev, (int32)State, Heat, Smoke, Oxygen, FireCount);
    }
}

bool ARoomActor::IsInsideRoomBox(const UBoxComponent* Box, const FVector& WorldPos)
{
    if (!Box) return false;
    const FVector Local = Box->GetComponentTransform().InverseTransformPosition(WorldPos);
    const FVector Extent = Box->GetScaledBoxExtent();
    return FMath::Abs(Local.X) <= Extent.X
        && FMath::Abs(Local.Y) <= Extent.Y
        && FMath::Abs(Local.Z) <= Extent.Z;
}

void ARoomActor::OnRoomBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!IsValid(OtherActor)) return;
    if (OtherActor == this) return;

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

AFireActor* ARoomActor::IgniteActor(AActor* TargetActor)
{
    if (!IsValid(TargetActor) || !IsValid(GetWorld())) return nullptr;

    UCombustibleComponent* Comb = TargetActor->FindComponentByClass<UCombustibleComponent>();
    if (!Comb) return nullptr;

    // 룸 소속 보장(혹시 누락 대비)
    Comb->SetOwningRoom(this);

    if (Comb->IsBurning())
        return Comb->ActiveFire;

    if (Comb->CombustibleType == ECombustibleType::Electric && !Comb->bElectricIgnitionTriggered)
        return nullptr;

    // 여기서 Fire 스폰 + Attach + LinkedCombustible 세팅까지 한 번에 처리
    AFireActor* NewFire = SpawnFireForCombustible(Comb, Comb->CombustibleType);

    return NewFire;
}

AFireActor* ARoomActor::IgniteRandomCombustibleInRoom(bool bAllowElectric)
{
    TArray<UCombustibleComponent*> List;
    GetCombustiblesInRoom(List, /*bExcludeBurning*/ true);

    List.RemoveAll([&](UCombustibleComponent* C)
        {
            if (!IsValid(C)) return true;
            if (!IsValid(C->GetOwner())) return true;

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
