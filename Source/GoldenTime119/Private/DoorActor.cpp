// ============================ DoorActor.cpp ============================
#include "DoorActor.h"
#include "RoomActor.h"
#include "BreakableComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogDoorActor, Log, All);

ADoorActor::ADoorActor()
{
    PrimaryActorTick.bCanEverTick = true;

    // BreakableComponent 생성
    Breakable = CreateDefaultSubobject<UBreakableComponent>(TEXT("Breakable"));
    Breakable->MaxHP = 80.f;
    Breakable->Material = EBreakableMaterial::Wood;
    Breakable->RequiredTool = EBreakToolType::Axe;
    Breakable->bAllowPassthroughWhenBroken = false; // DoorActor가 직접 처리
}

void ADoorActor::BeginPlay()
{
    Super::BeginPlay();

    // ===== Legacy mapping =====
    if (!IsValid(RoomA) && IsValid(OwningRoom))
        RoomA = OwningRoom;

    if (LinkType == EDoorLinkType::RoomToRoom && !IsValid(RoomB))
        LinkType = EDoorLinkType::RoomToOutside;

    // Hinge 캐시
    CacheHingeComponent();
    CacheDoorMeshComponent();

    // Breakable 설정
    SetupBreakableComponent();

    // 초기 상태
    ApplyStateByOpenAmount();
    PrevStateForEdge = DoorState;

    // 초기 시각 적용
    VisualYawCurrent = ClosedYawOffsetDeg;
    ApplyDoorVisual(0.f);

    // Room 등록
    SyncRoomRegistration(true);

    // VFX 준비 + Room 이벤트 바인딩
    EnsureDoorVfx();
    BindRoomSignals(true);

    if (DoorState != EDoorState::Closed)
        NotifyRoomsDoorOpenedOrBreached();

    if (SmokeLeakPSC)
        SmokeLeakPSC->SetFloatParameter(LeakParamName, 0.f);
    if (BackdraftPSC)
        BackdraftPSC->SetFloatParameter(BackdraftScaleParamName, 0.f);
}

void ADoorActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // ===== Debug input =====
    if (bEnableDebugOpen && DoorState != EDoorState::Breached)
    {
        APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
        if (PC)
        {
            if (PC->WasInputKeyJustPressed(EKeys::Two))
            {
                bDebugOpening = true;
                bDebugClosing = false;
            }

            if (PC->WasInputKeyJustPressed(EKeys::One))
            {
                bDebugClosing = true;
                bDebugOpening = false;
            }

            if (PC->WasInputKeyJustPressed(EKeys::Three))
            {
                const bool bShouldOpen = (OpenAmount01 < 0.5f);
                bDebugOpening = bShouldOpen;
                bDebugClosing = !bShouldOpen;
            }
        }

        if (bDebugOpening)
        {
            SetOpenAmount01(OpenAmount01 + DebugOpenSpeed * DeltaSeconds);
            if (OpenAmount01 >= 0.999f) bDebugOpening = false;
        }
        else if (bDebugClosing)
        {
            SetOpenAmount01(OpenAmount01 - DebugOpenSpeed * DeltaSeconds);
            if (OpenAmount01 <= 0.001f) bDebugClosing = false;
        }
    }

    // ===== VFX update =====
    UpdateDoorVfx(DeltaSeconds);

    // ===== Visual apply (hinge rotate) =====
    ApplyDoorVisual(DeltaSeconds);
}

void ADoorActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    BindRoomSignals(false);
    SyncRoomRegistration(false);
    Super::EndPlay(EndPlayReason);
}

// ============================ Breakable ============================
void ADoorActor::SetupBreakableComponent()
{
    if (!bIsBreakable || !Breakable)
        return;

    // 이벤트 바인딩
    Breakable->OnBroken.AddDynamic(this, &ADoorActor::OnDoorBrokenByAxe);
    Breakable->OnDamageReceived.AddDynamic(this, &ADoorActor::OnDoorDamagedByAxe);

    UE_LOG(LogDoorActor, Log, TEXT("[Door] %s Breakable setup - HP:%.1f Material:%d"),
        *GetName(), Breakable->MaxHP, (int32)Breakable->Material);
}

void ADoorActor::OnDoorDamagedByAxe(float Damage, float RemainingHP)
{
    UE_LOG(LogDoorActor, Warning, TEXT("[Door] %s HIT by axe! Damage:%.1f HP:%.1f/%.1f"),
        *GetName(), Damage, RemainingHP, Breakable ? Breakable->MaxHP : 0.f);
}

void ADoorActor::OnDoorBrokenByAxe()
{
    UE_LOG(LogDoorActor, Error, TEXT("[Door] ====== %s BROKEN BY AXE ======"), *GetName());

    // 문짝 숨기기
    if (bHideDoorMeshOnBreak && CachedDoorMesh)
    {
        CachedDoorMesh->SetVisibility(false);
        CachedDoorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // 잔해 스폰
    SpawnDebris();

    // Breached 상태로 전환 (백드래프트 트리거 포함)
    SetBreached();
}

void ADoorActor::SpawnDebris()
{
    if (DebrisMeshes.Num() == 0)
    {
        UE_LOG(LogDoorActor, Log, TEXT("[Door] No debris meshes assigned"));
        return;
    }

    const FVector DoorLocation = CachedDoorMesh ? CachedDoorMesh->GetComponentLocation() : GetActorLocation();

    for (int32 i = 0; i < DebrisCount; i++)
    {
        UStaticMesh* DebrisMesh = DebrisMeshes[FMath::RandRange(0, DebrisMeshes.Num() - 1)];
        if (!DebrisMesh)
            continue;

        const FVector SpawnOffset = FVector(
            FMath::RandRange(-20.f, 20.f),
            FMath::RandRange(-40.f, 40.f),
            FMath::RandRange(0.f, 150.f)
        );
        const FVector SpawnLocation = DoorLocation + SpawnOffset;

        const FRotator SpawnRotation = FRotator(
            FMath::RandRange(-180.f, 180.f),
            FMath::RandRange(-180.f, 180.f),
            FMath::RandRange(-180.f, 180.f)
        );

        UStaticMeshComponent* DebrisComp = NewObject<UStaticMeshComponent>(this);
        if (DebrisComp)
        {
            DebrisComp->SetStaticMesh(DebrisMesh);
            DebrisComp->SetWorldLocation(SpawnLocation);
            DebrisComp->SetWorldRotation(SpawnRotation);
            DebrisComp->SetSimulatePhysics(true);
            DebrisComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            DebrisComp->RegisterComponent();

            const FVector ImpulseDir = FVector(
                FMath::RandRange(-1.f, 1.f),
                FMath::RandRange(-1.f, 1.f),
                FMath::RandRange(0.5f, 1.f)
            ).GetSafeNormal();
            DebrisComp->AddImpulse(ImpulseDir * DebrisImpulseStrength, NAME_None, true);
        }
    }

    UE_LOG(LogDoorActor, Log, TEXT("[Door] Spawned %d debris pieces"), DebrisCount);
}

bool ADoorActor::IsBreakableDoor() const
{
    return bIsBreakable && Breakable != nullptr;
}

float ADoorActor::GetBreakableHPRatio() const
{
    if (!Breakable)
        return 1.f;

    return Breakable->GetHPRatio();
}

void ADoorActor::CacheDoorMeshComponent()
{
    CachedDoorMesh = nullptr;

    if (DoorMeshComponentName != NAME_None)
    {
        TArray<UActorComponent*> Comps;
        GetComponents(UStaticMeshComponent::StaticClass(), Comps);
        for (UActorComponent* C : Comps)
        {
            if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(C))
            {
                if (SMC->GetFName() == DoorMeshComponentName)
                {
                    CachedDoorMesh = SMC;
                    break;
                }
            }
        }
    }

    // 못 찾으면 Hinge 아래 첫 번째 StaticMesh
    if (!CachedDoorMesh && CachedHinge)
    {
        TArray<USceneComponent*> ChildComps;
        CachedHinge->GetChildrenComponents(false, ChildComps);
        for (USceneComponent* Child : ChildComps)
        {
            if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Child))
            {
                CachedDoorMesh = SMC;
                break;
            }
        }
    }
}

// ============================ Visual ============================
void ADoorActor::CacheHingeComponent()
{
    CachedHinge = nullptr;

    if (HingeComponentName != NAME_None)
    {
        TArray<UActorComponent*> Comps;
        GetComponents(USceneComponent::StaticClass(), Comps);
        for (UActorComponent* C : Comps)
        {
            if (USceneComponent* SC = Cast<USceneComponent>(C))
            {
                if (SC->GetFName() == HingeComponentName)
                {
                    CachedHinge = SC;
                    break;
                }
            }
        }
    }

    if (!CachedHinge)
        CachedHinge = GetRootComponent();
}

void ADoorActor::ApplyDoorVisual(float DeltaSeconds)
{
    if (!CachedHinge)
        CacheHingeComponent();
    if (!CachedHinge)
        return;

    const float O = FMath::Clamp(OpenAmount01, 0.f, 1.f);
    const float Sign = (OpenDirection == EDoorOpenDirection::PositiveYaw) ? 1.f : -1.f;
    const float TargetYaw = ClosedYawOffsetDeg + (Sign * MaxOpenYawDeg * O);

    if (VisualInterpSpeed > 0.f && DeltaSeconds > 0.f)
        VisualYawCurrent = FMath::FInterpTo(VisualYawCurrent, TargetYaw, DeltaSeconds, VisualInterpSpeed);
    else
        VisualYawCurrent = TargetYaw;

    FRotator R = CachedHinge->GetRelativeRotation();
    R.Yaw = VisualYawCurrent;
    CachedHinge->SetRelativeRotation(R);
}

// ============================ Room register ============================
void ADoorActor::SyncRoomRegistration(bool bRegister)
{
    if (IsValid(RoomA))
    {
        if (bRegister) RoomA->RegisterDoor(this);
        else          RoomA->UnregisterDoor(this);
    }

    if (LinkType == EDoorLinkType::RoomToRoom && IsValid(RoomB))
    {
        if (bRegister) RoomB->RegisterDoor(this);
        else          RoomB->UnregisterDoor(this);
    }
}

// ============================ Vent/Leak ============================
float ADoorActor::ComputeVent01() const
{
    if (DoorState == EDoorState::Breached) return 1.f;

    const float O = FMath::Clamp(OpenAmount01, 0.f, 1.f);
    const float V = FMath::Pow(O, VentPow) * VentMax;
    return FMath::Clamp(V, 0.f, 1.f);
}

float ADoorActor::ComputeLeak01() const
{
    if (DoorState != EDoorState::Closed) return 0.f;

    const float O = FMath::Clamp(OpenAmount01, 0.f, 1.f);
    const float L = (1.f - O) * LeakMax;
    return FMath::Clamp(L, 0.f, 1.f);
}

// ============================ Link query ============================
ARoomActor* ADoorActor::GetOtherRoom(const ARoomActor* From) const
{
    if (!IsValid(From)) return nullptr;

    if (From == RoomA)
        return (LinkType == EDoorLinkType::RoomToRoom) ? RoomB.Get() : nullptr;

    if (From == RoomB)
        return (LinkType == EDoorLinkType::RoomToRoom) ? RoomA.Get() : nullptr;

    return nullptr;
}

bool ADoorActor::IsOutsideConnectionFor(const ARoomActor* From) const
{
    if (!IsValid(From)) return false;

    if (LinkType == EDoorLinkType::RoomToOutside)
        return (From == RoomA) || (From == RoomB);

    return false;
}

// ============================ State 변경 ============================
void ADoorActor::SetOpenAmount01(float InOpen01)
{
    if (DoorState == EDoorState::Breached)
        return;

    const float NewOpen = FMath::Clamp(InOpen01, 0.f, 1.f);
    if (FMath::IsNearlyEqual(NewOpen, OpenAmount01, 0.0001f))
        return;

    OpenAmount01 = NewOpen;
    OnDoorOpenAmountChanged.Broadcast(OpenAmount01);

    const EDoorState Prev = DoorState;
    ApplyStateByOpenAmount();

    if (Prev != DoorState)
        OnDoorStateChanged.Broadcast(DoorState);

    if (DoorState != EDoorState::Closed)
        NotifyRoomsDoorOpenedOrBreached();

    const bool bEdgeClosedToOpen = (Prev == EDoorState::Closed && DoorState != EDoorState::Closed);
    if (bEdgeClosedToOpen)
        TryTriggerBackdraftIfNeeded(false);

    PrevStateForEdge = DoorState;
}

void ADoorActor::SetClosed()
{
    if (DoorState == EDoorState::Breached)
        return;

    SetOpenAmount01(0.f);
}

void ADoorActor::SetBreached()
{
    const EDoorState Prev = DoorState;

    DoorState = EDoorState::Breached;
    OpenAmount01 = 1.f;

    if (Prev != DoorState)
        OnDoorStateChanged.Broadcast(DoorState);

    OnDoorOpenAmountChanged.Broadcast(OpenAmount01);

    NotifyRoomsDoorOpenedOrBreached();
    TryTriggerBackdraftIfNeeded(true);

    PrevStateForEdge = DoorState;
}

void ADoorActor::ApplyStateByOpenAmount()
{
    if (DoorState == EDoorState::Breached)
        return;

    const bool bClosed = (OpenAmount01 <= ClosedDeadzone01);
    DoorState = bClosed ? EDoorState::Closed : EDoorState::Open;
}

void ADoorActor::NotifyRoomsDoorOpenedOrBreached()
{
    if (IsValid(RoomA)) RoomA->NotifyDoorSealed(false);
    if (LinkType == EDoorLinkType::RoomToRoom && IsValid(RoomB)) RoomB->NotifyDoorSealed(false);
}

void ADoorActor::TryTriggerBackdraftIfNeeded(bool bFromBreach)
{
    const float VentBoost = bFromBreach ? 1.f : ComputeVent01();

    const bool bAArmed = IsValid(RoomA) ? RoomA->IsBackdraftArmed() : false;
    const bool bBArmed = (LinkType == EDoorLinkType::RoomToRoom && IsValid(RoomB)) ? RoomB->IsBackdraftArmed() : false;

    if (bAArmed && bBArmed)
        return;

    if (LinkType == EDoorLinkType::RoomToOutside)
    {
        if (bAArmed && IsValid(RoomA))
            RoomA->TriggerBackdraft(GetActorTransform(), VentBoost);
        return;
    }

    if (bAArmed && IsValid(RoomA))
        RoomA->TriggerBackdraft(GetActorTransform(), VentBoost);

    if (bBArmed && IsValid(RoomB))
        RoomB->TriggerBackdraft(GetActorTransform(), VentBoost);
}

// ============================ Door VFX ============================
void ADoorActor::EnsureDoorVfx()
{
    if (!SmokeLeakPSC)
    {
        SmokeLeakPSC = NewObject<UParticleSystemComponent>(this, TEXT("SmokeLeakPSC"));
        SmokeLeakPSC->SetupAttachment(GetRootComponent());
        SmokeLeakPSC->RegisterComponent();
        SmokeLeakPSC->bAutoActivate = false;
        if (SmokeLeakTemplate) SmokeLeakPSC->SetTemplate(SmokeLeakTemplate);
    }

    if (!BackdraftPSC)
    {
        BackdraftPSC = NewObject<UParticleSystemComponent>(this, TEXT("BackdraftPSC"));
        BackdraftPSC->SetupAttachment(GetRootComponent());
        BackdraftPSC->RegisterComponent();
        BackdraftPSC->bAutoActivate = false;
        if (BackdraftTemplate) BackdraftPSC->SetTemplate(BackdraftTemplate);
    }
}

void ADoorActor::BindRoomSignals(bool bBind)
{
    if (IsValid(RoomA))
    {
        if (bBind)
        {
            RoomA->OnBackdraftLeakStrength.RemoveDynamic(this, &ADoorActor::OnRoomABackdraftLeakStrength);
            RoomA->OnBackdraft.RemoveDynamic(this, &ADoorActor::OnRoomBackdraftTriggered);

            RoomA->OnBackdraftLeakStrength.AddDynamic(this, &ADoorActor::OnRoomABackdraftLeakStrength);
            RoomA->OnBackdraft.AddDynamic(this, &ADoorActor::OnRoomBackdraftTriggered);
        }
        else
        {
            RoomA->OnBackdraftLeakStrength.RemoveDynamic(this, &ADoorActor::OnRoomABackdraftLeakStrength);
            RoomA->OnBackdraft.RemoveDynamic(this, &ADoorActor::OnRoomBackdraftTriggered);
        }
    }

    if (LinkType == EDoorLinkType::RoomToRoom && IsValid(RoomB))
    {
        if (bBind)
        {
            RoomB->OnBackdraftLeakStrength.RemoveDynamic(this, &ADoorActor::OnRoomBBackdraftLeakStrength);
            RoomB->OnBackdraft.RemoveDynamic(this, &ADoorActor::OnRoomBackdraftTriggered);

            RoomB->OnBackdraftLeakStrength.AddDynamic(this, &ADoorActor::OnRoomBBackdraftLeakStrength);
            RoomB->OnBackdraft.AddDynamic(this, &ADoorActor::OnRoomBackdraftTriggered);
        }
        else
        {
            RoomB->OnBackdraftLeakStrength.RemoveDynamic(this, &ADoorActor::OnRoomBBackdraftLeakStrength);
            RoomB->OnBackdraft.RemoveDynamic(this, &ADoorActor::OnRoomBackdraftTriggered);
        }
    }
}

void ADoorActor::OnRoomABackdraftLeakStrength(float Leak01)
{
    LeakFromRoomA01 = FMath::Clamp(Leak01, 0.f, 1.f);
}

void ADoorActor::OnRoomBBackdraftLeakStrength(float Leak01)
{
    LeakFromRoomB01 = FMath::Clamp(Leak01, 0.f, 1.f);
}

void ADoorActor::SetLeakSideToRoomA()
{
    if (!SmokeLeakPSC) return;
    SmokeLeakPSC->SetRelativeLocation(FVector(0.f, -LeakSideOffsetCm, 0.f));
}

void ADoorActor::SetLeakSideToRoomB()
{
    if (!SmokeLeakPSC) return;
    SmokeLeakPSC->SetRelativeLocation(FVector(0.f, +LeakSideOffsetCm, 0.f));
}

void ADoorActor::SetLeakSideToOutsideFromRoomA()
{
    if (!SmokeLeakPSC) return;
    SmokeLeakPSC->SetRelativeLocation(FVector(0.f, +LeakSideOffsetCm, 0.f));
}

void ADoorActor::UpdateDoorVfx(float DeltaSeconds)
{
    EnsureDoorVfx();
    if (!SmokeLeakPSC) return;

    const bool bClosed = (DoorState == EDoorState::Closed);

    const bool bAArmed = IsValid(RoomA) ? RoomA->IsBackdraftArmed() : false;
    const bool bBArmed = (LinkType == EDoorLinkType::RoomToRoom && IsValid(RoomB)) ? RoomB->IsBackdraftArmed() : false;

    if ((LinkType == EDoorLinkType::RoomToRoom) && bAArmed && bBArmed)
    {
        LeakValSmoothed = FMath::FInterpTo(LeakValSmoothed, 0.f, DeltaSeconds, LeakInterpSpeed);
        SmokeLeakPSC->SetFloatParameter(LeakParamName, LeakValSmoothed);
        if (LeakValSmoothed <= 0.001f) SmokeLeakPSC->DeactivateSystem();
        return;
    }

    float TargetLeak = 0.f;
    bool bShouldLeak = false;

    if (bClosed)
    {
        if (LinkType == EDoorLinkType::RoomToOutside)
        {
            if (bAArmed)
            {
                bShouldLeak = true;
                TargetLeak = LeakFromRoomA01;
                SetLeakSideToOutsideFromRoomA();
            }
        }
        else
        {
            if (bAArmed && !bBArmed)
            {
                bShouldLeak = true;
                TargetLeak = LeakFromRoomA01;
                SetLeakSideToRoomB();
            }
            else if (bBArmed && !bAArmed)
            {
                bShouldLeak = true;
                TargetLeak = LeakFromRoomB01;
                SetLeakSideToRoomA();
            }
        }
    }

    if (bShouldLeak && TargetLeak > 0.001f)
        SmokeLeakPSC->ActivateSystem(true);

    LeakValSmoothed = FMath::FInterpTo(LeakValSmoothed, bShouldLeak ? TargetLeak : 0.f, DeltaSeconds, LeakInterpSpeed);
    SmokeLeakPSC->SetFloatParameter(LeakParamName, LeakValSmoothed);

    if (!bShouldLeak && LeakValSmoothed <= 0.001f)
        SmokeLeakPSC->DeactivateSystem();
}

void ADoorActor::OnRoomBackdraftTriggered()
{
    EnsureDoorVfx();

    const bool bAArmed = IsValid(RoomA) ? RoomA->IsBackdraftArmed() : false;
    const bool bBArmed = (LinkType == EDoorLinkType::RoomToRoom && IsValid(RoomB)) ? RoomB->IsBackdraftArmed() : false;

    if ((LinkType == EDoorLinkType::RoomToRoom) && bAArmed && bBArmed)
        return;

    if (BackdraftPSC)
    {
        BackdraftPSC->ActivateSystem(true);
        const float Scale = (DoorState == EDoorState::Breached) ? 1.0f : FMath::Clamp(ComputeVent01(), 0.f, 1.f);
        BackdraftPSC->SetFloatParameter(BackdraftScaleParamName, Scale);
    }

    if (bForceOpenOnBackdraft && DoorState != EDoorState::Breached)
    {
        SetOpenAmount01(1.0f);
    }
}