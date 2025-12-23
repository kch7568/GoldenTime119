#include "FireActor.h"
#include "RoomActor.h"
#include "Components/SceneComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogFire, Log, All);

AFireActor::AFireActor()
{
    PrimaryActorTick.bCanEverTick = true;

    // 핵심: RootComponent 없으면 Actor 위치가 제대로 동작 안함
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

    UE_LOG(LogFire, Warning,
        TEXT("[Fire] InitFire Fire=%s Id=%s Room=%s Type=%d BaseInt=%.2f Loc=%s"),
        *GetName(), *FireID.ToString(),
        IsValid(LinkedRoom) ? *LinkedRoom->GetName() : TEXT("None"),
        (int32)CombustibleType, BaseIntensity,
        *GetActorLocation().ToString()
    );
}

void AFireActor::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogFire, Warning,
        TEXT("[Fire] BeginPlay Fire=%s Id=%s Init=%d Active=%d LinkedRoom=%s SpawnRoom=%s SpawnType=%d Loc=%s"),
        *GetName(), *FireID.ToString(),
        bInitialized ? 1 : 0,
        bIsActive ? 1 : 0,
        IsValid(LinkedRoom) ? *LinkedRoom->GetName() : TEXT("None"),
        IsValid(SpawnRoom) ? *SpawnRoom->GetName() : TEXT("None"),
        (int32)SpawnType,
        *GetActorLocation().ToString()
    );

    if (!bInitialized)
    {
        UE_LOG(LogFire, Warning, TEXT("[Fire] BeginPlay: not initialized -> InitFire(SpawnRoom, SpawnType)"));
        InitFire(SpawnRoom, SpawnType);
    }

    if (!bStartedEventFired)
    {
        bStartedEventFired = true;
        OnFireStarted.Broadcast(this);
    }

    if (IsValid(LinkedRoom))
    {
        UE_LOG(LogFire, Warning, TEXT("[Fire] RegisterFire -> Room=%s FireId=%s"),
            *LinkedRoom->GetName(), *FireID.ToString());
        LinkedRoom->RegisterFire(this);
    }
    else
    {
        UE_LOG(LogFire, Error, TEXT("[Fire] BeginPlay: LinkedRoom is None -> Destroy"));
        Destroy();
        return;
    }

    UpdateRuntimeTuningFromRoom();
}

void AFireActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bIsActive) return;

    if (ShouldExtinguish())
    {
        UE_LOG(LogFire, Warning, TEXT("[Fire] ShouldExtinguish=true Fire=%s Id=%s"), *GetName(), *FireID.ToString());
        Extinguish();
        return;
    }

    if (CombustibleType == ECombustibleType::Explosive)
    {
        TriggerExplosiveOnce();
    }

    UpdateIntensityFromRoom();
    UpdateRuntimeTuningFromRoom();

    InfluenceAcc += DeltaSeconds;
    if (InfluenceAcc >= InfluenceInterval)
    {
        InfluenceAcc = 0.0f;
        RequestInfluence();
    }

    SpreadAcc += DeltaSeconds;
    if (SpreadAcc >= SpreadInterval)
    {
        SpreadAcc = 0.0f;
        RequestSpread();
    }
}

bool AFireActor::ShouldExtinguish() const
{
    if (!IsValid(LinkedRoom)) return true;
    return LinkedRoom->ShouldExtinguishFire(FireID);
}

void AFireActor::UpdateIntensityFromRoom()
{
    if (!IsValid(LinkedRoom)) return;

    const float Scale = LinkedRoom->GetIntensityScale(FireID);
    EffectiveIntensity = FMath::Max(0.0f, BaseIntensity * Scale);
}

void AFireActor::UpdateRuntimeTuningFromRoom()
{
    if (!IsValid(LinkedRoom)) return;

    FFireRuntimeTuning T;
    if (!LinkedRoom->GetRuntimeTuning(FireID, CombustibleType, EffectiveIntensity, T))
        return;

    CurrentSpreadRadius = T.SpreadRadius;
    CurrentSpreadInterval = T.SpreadInterval;

    SpreadInterval = FMath::Max(0.05f, CurrentSpreadInterval);
}

void AFireActor::RequestInfluence()
{
    if (!IsValid(LinkedRoom)) return;

    UE_LOG(LogFire, Log,
        TEXT("[Fire] InfluenceReq Fire=%s Id=%s Type=%d Eff=%.2f"),
        *GetName(), *FireID.ToString(), (int32)CombustibleType, EffectiveIntensity
    );

    LinkedRoom->SubmitInfluence(FireID, CombustibleType, EffectiveIntensity);
}

void AFireActor::RequestSpread()
{
    if (!IsValid(LinkedRoom)) return;

    UE_LOG(LogFire, Log,
        TEXT("[Fire] SpreadReq Fire=%s Id=%s Type=%d Origin=%s Radius=%.1f Interval=%.2f"),
        *GetName(), *FireID.ToString(), (int32)CombustibleType,
        *GetSpreadOrigin().ToString(),
        CurrentSpreadRadius, SpreadInterval
    );

    const bool bSpawned = LinkedRoom->TrySpawnFireFromSpread(this);
    if (bSpawned)
    {
        UE_LOG(LogFire, Warning, TEXT("[Fire] SpreadSuccess SourceFire=%s Id=%s"), *GetName(), *FireID.ToString());
        OnFireSpread.Broadcast(this);
    }
}

void AFireActor::TriggerExplosiveOnce()
{
    if (bExplosionTriggered) return;
    bExplosionTriggered = true;

    SpreadInterval = FMath::Max(0.35f, SpreadInterval * 0.5f);
    BaseIntensity *= 1.5f;
}

void AFireActor::Extinguish()
{
    if (!bIsActive) return;
    bIsActive = false;

    if (IsValid(LinkedRoom))
        LinkedRoom->UnregisterFire(FireID);

    if (!bExtinguishedEventFired)
    {
        bExtinguishedEventFired = true;
        OnFireExtinguished.Broadcast(this);
    }

    Destroy();
}

FVector AFireActor::GetSpreadOrigin() const
{
    if (IgnitedTarget.IsValid())
    {
        return IgnitedTarget->GetComponentsBoundingBox(true).GetCenter();
    }
    return GetActorLocation();
}
