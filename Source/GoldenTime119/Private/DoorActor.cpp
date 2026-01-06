// ============================ DoorActor.cpp ============================
#include "DoorActor.h"
#include "RoomActor.h"
#include "BreakableComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"

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
    Breakable->bAllowPassthroughWhenBroken = false;
}

void ADoorActor::BeginPlay()
{
    Super::BeginPlay();

    // Legacy mapping
    if (!IsValid(RoomA) && IsValid(OwningRoom))
        RoomA = OwningRoom;

    if (LinkType == EDoorLinkType::RoomToRoom && !IsValid(RoomB))
        LinkType = EDoorLinkType::RoomToOutside;

    // Cache
    CacheHingeComponent();
    CacheDoorMeshComponent();

    // Breakable 설정
    SetupBreakableComponent();
    LastHPRatioForVentHole = 1.f;

    // 초기 상태
    ApplyStateByOpenAmount();
    PrevStateForEdge = DoorState;

    VisualYawCurrent = ClosedYawOffsetDeg;
    ApplyDoorVisual(0.f);

    // Room 등록
    SyncRoomRegistration(true);

    // VFX
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

    // Debug input
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

    // VentHole 연기 업데이트
    UpdateVentHoleEffects(DeltaSeconds);

    // VFX update
    UpdateDoorVfx(DeltaSeconds);

    // Visual apply
    ApplyDoorVisual(DeltaSeconds);
}

void ADoorActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 타이머 정리
    GetWorld()->GetTimerManager().ClearTimer(BackdraftDelayTimer);

    BindRoomSignals(false);
    SyncRoomRegistration(false);
    Super::EndPlay(EndPlayReason);
}

// ============================ Breakable ============================
void ADoorActor::SetupBreakableComponent()
{
    if (!bIsBreakable || !Breakable)
        return;

    Breakable->OnBroken.AddDynamic(this, &ADoorActor::OnDoorBrokenByAxe);
    Breakable->OnDamageReceived.AddDynamic(this, &ADoorActor::OnDoorDamagedByAxe);

    UE_LOG(LogDoorActor, Log, TEXT("[Door] %s Breakable setup - HP:%.1f Material:%d"),
        *GetName(), Breakable->MaxHP, (int32)Breakable->Material);
}

void ADoorActor::OnDoorDamagedByAxe(float Damage, float RemainingHP)
{
    const float CurrentHPRatio = Breakable ? Breakable->GetHPRatio() : 1.f;

    UE_LOG(LogDoorActor, Warning, TEXT("[Door] %s HIT! Damage:%.1f HP:%.1f/%.1f Ratio:%.2f"),
        *GetName(), Damage, RemainingHP, Breakable ? Breakable->MaxHP : 0.f, CurrentHPRatio);

    // 환기 구멍 체크 (완전 파괴 전에만)
    if (CurrentHPRatio > 0.f)
    {
        // 히트 위치 추정 (문 중앙 기준 랜덤 오프셋)
        FVector HitLocation = CachedDoorMesh ? CachedDoorMesh->GetComponentLocation() : GetActorLocation();
        HitLocation += FVector(
            FMath::RandRange(-20.f, 20.f),
            FMath::RandRange(-30.f, 30.f),
            FMath::RandRange(50.f, 150.f)  // 보통 문 상단을 찍음
        );
        LastDamageLocation = HitLocation;

        CheckAndCreateVentHole(CurrentHPRatio, HitLocation);
    }

    LastHPRatioForVentHole = CurrentHPRatio;
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

    // 환기 구멍 VFX 모두 정지
    for (FVentHoleInfo& Hole : VentHoles)
    {
        if (Hole.VentSmokePSC)
        {
            Hole.VentSmokePSC->DeactivateSystem();
        }
        Hole.bIsVenting = false;
    }

    // 잔해 스폰
    SpawnDebris();

    // Breached 상태로 전환
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
        if (!DebrisMesh) continue;

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
    if (!Breakable) return 1.f;
    return Breakable->GetHPRatio();
}

// ============================ VentHole System ============================
void ADoorActor::CheckAndCreateVentHole(float CurrentHPRatio, FVector HitLocation)
{
    // 이미 최대 구멍 수에 도달
    if (VentHoles.Num() >= MaxVentHoles)
        return;

    // HP가 임계값 이하로 내려갔는지 체크
    // 각 구멍마다 다른 임계값 사용 (70%, 50%, 30%)
    const int32 CurrentHoleCount = VentHoles.Num();
    float ThresholdForNextHole = VentHoleCreateThreshold;

    // 구멍이 많을수록 더 낮은 HP에서 생성
    switch (CurrentHoleCount)
    {
    case 0: ThresholdForNextHole = VentHoleCreateThreshold; break;        // 70%
    case 1: ThresholdForNextHole = VentHoleCreateThreshold - 0.2f; break; // 50%
    case 2: ThresholdForNextHole = VentHoleCreateThreshold - 0.4f; break; // 30%
    default: return;
    }

    // 이전 HP는 임계값 위, 현재 HP는 임계값 이하 → 구멍 생성
    if (LastHPRatioForVentHole > ThresholdForNextHole && CurrentHPRatio <= ThresholdForNextHole)
    {
        // 월드 좌표 → 로컬 좌표
        FVector LocalPos = GetActorTransform().InverseTransformPosition(HitLocation);
        CreateVentHole(LocalPos);
    }
}

void ADoorActor::CreateVentHole(FVector LocalPosition)
{
    FVentHoleInfo NewHole;
    NewHole.LocalPosition = LocalPosition;
    NewHole.CreatedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    NewHole.bIsVenting = false;

    const FVector WorldPos = GetActorTransform().TransformPosition(LocalPosition);

    // 균열 데칼 생성
    if (CrackDecalMaterial)
    {
        UDecalComponent* Decal = NewObject<UDecalComponent>(this);
        if (Decal)
        {
            Decal->SetDecalMaterial(CrackDecalMaterial);
            Decal->DecalSize = CrackDecalSize;
            Decal->SetWorldLocation(WorldPos);
            Decal->SetWorldRotation(GetActorRotation());
            Decal->RegisterComponent();
            Decal->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
            NewHole.CrackDecal = Decal;
        }
    }

    // 환기 연기 VFX 생성
    if (VentSmokeTemplate)
    {
        UParticleSystemComponent* PSC = NewObject<UParticleSystemComponent>(this);
        if (PSC)
        {
            PSC->SetTemplate(VentSmokeTemplate);
            PSC->SetWorldLocation(WorldPos);
            // 연기가 가로로 나가도록 회전 (문 노멀 방향)
            PSC->SetWorldRotation(GetActorRotation() + FRotator(0.f, 90.f, 0.f));
            PSC->bAutoActivate = false;
            PSC->RegisterComponent();
            PSC->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
            NewHole.VentSmokePSC = PSC;
        }
    }

    VentHoles.Add(NewHole);

    // Room에 알림
    NotifyRoomVentHoleCreated();

    OnVentHoleCreated.Broadcast(VentHoles.Num());

    UE_LOG(LogDoorActor, Warning, TEXT("[Door] %s VentHole created! Count:%d LocalPos:%s"),
        *GetName(), VentHoles.Num(), *LocalPosition.ToString());
}

void ADoorActor::UpdateVentHoleEffects(float DeltaSeconds)
{
    if (VentHoles.Num() == 0)
        return;

    // Room의 백드래프트 압력 가져오기
    const float Pressure = GetBackdraftPressureFromRoom();
    const bool bShouldVent = (Pressure > MinPressureForVentSmoke);

    for (FVentHoleInfo& Hole : VentHoles)
    {
        if (!Hole.VentSmokePSC)
            continue;

        if (bShouldVent && !Hole.bIsVenting)
        {
            // 연기 시작
            Hole.VentSmokePSC->ActivateSystem(true);
            Hole.bIsVenting = true;
            UE_LOG(LogDoorActor, Log, TEXT("[Door] VentHole smoke started (Pressure:%.2f)"), Pressure);
        }
        else if (!bShouldVent && Hole.bIsVenting)
        {
            // 연기 정지
            Hole.VentSmokePSC->DeactivateSystem();
            Hole.bIsVenting = false;
            UE_LOG(LogDoorActor, Log, TEXT("[Door] VentHole smoke stopped (Pressure:%.2f)"), Pressure);
        }

        // 연기 강도 조절 (압력에 비례)
        if (Hole.bIsVenting)
        {
            const float Intensity = FMath::Clamp(Pressure, 0.f, 1.f);
            Hole.VentSmokePSC->SetFloatParameter(TEXT("Intensity"), Intensity);
        }
    }
}

void ADoorActor::NotifyRoomVentHoleCreated()
{
    // Armed 상태인 방에 환기 구멍 알림
    ARoomActor* ArmedRoom = nullptr;

    if (IsValid(RoomA) && RoomA->IsBackdraftArmed())
    {
        ArmedRoom = RoomA;
    }
    else if (LinkType == EDoorLinkType::RoomToRoom && IsValid(RoomB) && RoomB->IsBackdraftArmed())
    {
        ArmedRoom = RoomB;
    }

    if (ArmedRoom)
    {
        // Room에 환기 시작 알림 → BackdraftPressure 감소 시작
        ArmedRoom->AddDoorVentHole(this, VentRatePerHole);
        UE_LOG(LogDoorActor, Warning, TEXT("[Door] Notified Room %s about VentHole (Rate:%.2f)"),
            *ArmedRoom->GetName(), GetTotalVentRate());
    }
}

float ADoorActor::GetBackdraftPressureFromRoom() const
{
    // Armed 상태인 방의 BackdraftPressure 반환
    if (IsValid(RoomA) && RoomA->IsBackdraftArmed())
    {
        return RoomA->GetBackdraftPressure();
    }
    if (LinkType == EDoorLinkType::RoomToRoom && IsValid(RoomB) && RoomB->IsBackdraftArmed())
    {
        return RoomB->GetBackdraftPressure();
    }
    return 0.f;
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

    // 환기 구멍이 있으면 약간의 Vent 추가
    float HoleVent = VentHoles.Num() * 0.1f; // 구멍당 10% Vent

    const float O = FMath::Clamp(OpenAmount01, 0.f, 1.f);
    const float V = FMath::Pow(O, VentPow) * VentMax + HoleVent;
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

    const EDoorState Prev = DoorState;

    OpenAmount01 = NewOpen;
    OnDoorOpenAmountChanged.Broadcast(OpenAmount01);

    ApplyStateByOpenAmount();

    if (Prev != DoorState)
        OnDoorStateChanged.Broadcast(DoorState);

    // 순서 변경: 백드래프트 먼저
    const bool bEdgeClosedToOpen = (Prev == EDoorState::Closed && DoorState != EDoorState::Closed);
    if (bEdgeClosedToOpen)
        TryTriggerBackdraftIfNeeded(false);

    // 그 다음에 Room 알림 (Armed 리셋)
    if (DoorState != EDoorState::Closed)
        NotifyRoomsDoorOpenedOrBreached();

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

    // ★ 환기 구멍이 있으면 백드래프트 약화
    if (VentHoles.Num() > 0)
    {
        // 환기 구멍으로 압력이 빠졌으면 백드래프트 강도 감소
        UE_LOG(LogDoorActor, Warning, TEXT("[Door] %s Breached with %d VentHoles - Backdraft may be weakened"),
            *GetName(), VentHoles.Num());
    }

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
    UE_LOG(LogDoorActor, Warning, TEXT("[Door] ====== TryTriggerBackdraft START ====== bFromBreach:%d"), bFromBreach);

    const float VentBoost = bFromBreach ? 1.f : ComputeVent01();

    const bool bAArmed = IsValid(RoomA) ? RoomA->IsBackdraftArmed() : false;
    const bool bBArmed = (LinkType == EDoorLinkType::RoomToRoom && IsValid(RoomB))
        ? RoomB->IsBackdraftArmed() : false;

    UE_LOG(LogDoorActor, Warning, TEXT("[Door] RoomA:%s Armed:%d | RoomB:%s Armed:%d"),
        RoomA ? *RoomA->GetName() : TEXT("NULL"), bAArmed,
        RoomB ? *RoomB->GetName() : TEXT("NULL"), bBArmed);

    if (bAArmed && bBArmed)
        return;

    ARoomActor* TargetRoom = nullptr;

    if (LinkType == EDoorLinkType::RoomToOutside)
    {
        if (bAArmed && IsValid(RoomA))
            TargetRoom = RoomA;
    }
    else
    {
        if (bAArmed && IsValid(RoomA))
            TargetRoom = RoomA;
        else if (bBArmed && IsValid(RoomB))
            TargetRoom = RoomB;
    }

    if (!TargetRoom)
        return;

    // 딜레이 설정 (1초 후 폭발)
    PendingBackdraftRoom = TargetRoom;
    PendingBackdraftVentBoost = VentBoost;

    // 문이 열린 방향 계산 (Hinge 회전 기준)
    FVector BlastDirection;

    if (CachedHinge)
    {
        // 문이 열린 방향 = Hinge의 Forward 벡터
        FRotator HingeRotation = CachedHinge->GetComponentRotation();
        BlastDirection = HingeRotation.Vector();

        // 문 열림 방향에 따라 반전
        if (OpenDirection == EDoorOpenDirection::NegativeYaw)
        {
            BlastDirection = -BlastDirection;
        }
    }
    else
    {
        // 폴백: Room에서 문 바깥쪽으로
        FVector DoorLocation = GetActorLocation();
        FVector RoomCenter = TargetRoom->GetActorLocation();
        BlastDirection = (DoorLocation - RoomCenter).GetSafeNormal();
    }

    // Z 방향은 약간 위로 (폭발이 위로 퍼지는 느낌)
    BlastDirection.Z = FMath::Max(BlastDirection.Z, 0.2f);
    BlastDirection.Normalize();

    FRotator BlastRotation = BlastDirection.Rotation();
    FVector BlastLocation = GetActorLocation() + BlastDirection * 50.f; // 문 앞쪽으로 약간 이동

    PendingBackdraftDoorTM = FTransform(BlastRotation, BlastLocation);

    UE_LOG(LogDoorActor, Warning, TEXT("[Door] Backdraft scheduled in 1 second - BlastDir:%s"),
        *BlastDirection.ToString());

    // 1초 후 ExecuteDelayedBackdraft 호출
    GetWorld()->GetTimerManager().SetTimer(
        BackdraftDelayTimer,
        this,
        &ADoorActor::ExecuteDelayedBackdraft,
        1.0f,
        false
    );
}
void ADoorActor::ExecuteDelayedBackdraft()
{
    if (!IsValid(PendingBackdraftRoom))
    {
        UE_LOG(LogDoorActor, Warning, TEXT("[Door] Delayed backdraft cancelled - Room invalid"));
        return;
    }

    UE_LOG(LogDoorActor, Warning, TEXT("[Door] >>> Executing delayed backdraft NOW!"));

    PendingBackdraftRoom->TriggerBackdraft(PendingBackdraftDoorTM, PendingBackdraftVentBoost);

    // VFX 재생 (문 열린 방향으로)
    if (BackdraftPSC)
    {
        BackdraftPSC->SetWorldLocation(PendingBackdraftDoorTM.GetLocation());
        BackdraftPSC->SetWorldRotation(PendingBackdraftDoorTM.GetRotation());
        BackdraftPSC->ActivateSystem(true);
    }

    // 문이 폭발로 날아감
    if (bDoorCanBeBlownOff && CachedDoorMesh && DoorState != EDoorState::Breached)
    {
        // 문짝을 물리 시뮬레이션으로 전환
        CachedDoorMesh->SetSimulatePhysics(true);
        CachedDoorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

        // Hinge에서 분리
        CachedDoorMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

        // 폭발 방향으로 Impulse
        FVector ImpulseDir = PendingBackdraftDoorTM.GetRotation().Vector();
        ImpulseDir.Z += 0.3f; // 약간 위로
        ImpulseDir.Normalize();

        CachedDoorMesh->AddImpulse(ImpulseDir * BackdraftDoorImpulse, NAME_None, true);

        // 회전 Torque (문이 빙글빙글 돌면서 날아가게)
        FVector TorqueAxis = FVector::CrossProduct(ImpulseDir, FVector::UpVector).GetSafeNormal();
        CachedDoorMesh->AddTorqueInRadians(TorqueAxis * BackdraftDoorTorque, NAME_None, true);

        // 상태를 Breached로 변경
        DoorState = EDoorState::Breached;
        OpenAmount01 = 1.f;
        OnDoorStateChanged.Broadcast(DoorState);

        UE_LOG(LogDoorActor, Warning, TEXT("[Door] %s BLOWN OFF by backdraft!"), *GetName());
    }

    PendingBackdraftRoom = nullptr;
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