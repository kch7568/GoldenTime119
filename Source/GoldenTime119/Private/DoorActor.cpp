// ============================ DoorActor.cpp ============================
#include "DoorActor.h"
#include "RoomActor.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Components/SceneComponent.h"

#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"

ADoorActor::ADoorActor()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ADoorActor::BeginPlay()
{
    Super::BeginPlay();

    // ===== Legacy mapping =====
    if (!IsValid(RoomA) && IsValid(OwningRoom))
        RoomA = OwningRoom;

    // LinkType 보정: RoomB가 없으면 Outside로
    if (LinkType == EDoorLinkType::RoomToRoom && !IsValid(RoomB))
        LinkType = EDoorLinkType::RoomToOutside;

    // Hinge 캐시
    CacheHingeComponent();

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

    // 초기 상태가 Open/Breached면 sealed 리셋 힌트
    if (DoorState != EDoorState::Closed)
        NotifyRoomsDoorOpenedOrBreached();

    // 초기 VFX 0
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
            // 2 누르면 "열기 시작"
            if (PC->WasInputKeyJustPressed(EKeys::Two))
            {
                bDebugOpening = true;
                bDebugClosing = false;
            }

            // 1 누르면 "닫기 시작"
            if (PC->WasInputKeyJustPressed(EKeys::One))
            {
                bDebugClosing = true;
                bDebugOpening = false;
            }

            // 3 누르면 토글
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

// ============================ Visual ============================
void ADoorActor::CacheHingeComponent()
{
    CachedHinge = nullptr;

    // 1) 이름으로 찾기 (BP: "Hinge")
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

    // 2) 못 찾으면 Root (최후수단)
    if (!IsValid(CachedHinge))
        CachedHinge = GetRootComponent();
}

void ADoorActor::ApplyDoorVisual(float DeltaSeconds)
{
    if (!IsValid(CachedHinge))
        CacheHingeComponent();
    if (!IsValid(CachedHinge))
        return;

    const float O = FMath::Clamp(OpenAmount01, 0.f, 1.f);

    // 방향/최대각 적용
    const float Sign = (OpenDirection == EDoorOpenDirection::PositiveYaw) ? 1.f : -1.f;
    const float TargetYaw = ClosedYawOffsetDeg + (Sign * MaxOpenYawDeg * O);

    if (VisualInterpSpeed > 0.f && DeltaSeconds > 0.f)
        VisualYawCurrent = FMath::FInterpTo(VisualYawCurrent, TargetYaw, DeltaSeconds, VisualInterpSpeed);
    else
        VisualYawCurrent = TargetYaw;

    // 로컬 Yaw만 회전 (힌지 기준)
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

    // 열림이면 sealed 리셋 힌트
    if (DoorState != EDoorState::Closed)
        NotifyRoomsDoorOpenedOrBreached();

    // 닫힘→열림 에지에서만 백드래프트 트라이
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

// ============================ Backdraft trigger rule ============================
// 요구사항:
// - 양쪽 방 모두 armed면 backdraft X
// - 한쪽 armed면 그쪽만 trigger 가능 (다만 반대쪽도 armed면 무시)
void ADoorActor::TryTriggerBackdraftIfNeeded(bool bFromBreach)
{
    const float VentBoost = bFromBreach ? 1.f : ComputeVent01();

    const bool bAArmed = IsValid(RoomA) ? RoomA->IsBackdraftArmed() : false;
    const bool bBArmed = (LinkType == EDoorLinkType::RoomToRoom && IsValid(RoomB)) ? RoomB->IsBackdraftArmed() : false;

    // 양쪽 armed면 어떤 것도 트리거하지 않음
    if (bAArmed && bBArmed)
        return;

    // RoomToOutside: RoomA만 대상으로 취급
    if (LinkType == EDoorLinkType::RoomToOutside)
    {
        if (bAArmed && IsValid(RoomA))
            RoomA->TriggerBackdraft(GetActorTransform(), VentBoost);
        return;
    }

    // RoomToRoom
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
    // RoomA
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

    // RoomB (RoomToRoom)
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
    // Door 로컬 Right(-) 방향을 RoomA 쪽이라고 가정하는 기본 정책
    SmokeLeakPSC->SetRelativeLocation(FVector(0.f, -LeakSideOffsetCm, 0.f));
}

void ADoorActor::SetLeakSideToRoomB()
{
    if (!SmokeLeakPSC) return;
    // Door 로컬 Right(+) 방향을 RoomB 쪽이라고 가정하는 기본 정책
    SmokeLeakPSC->SetRelativeLocation(FVector(0.f, +LeakSideOffsetCm, 0.f));
}

void ADoorActor::SetLeakSideToOutsideFromRoomA()
{
    if (!SmokeLeakPSC) return;
    // RoomToOutside에서는 “바깥쪽”을 +Right로 둡니다(필요 시 반대로 바꿔서 씬에 맞추세요)
    SmokeLeakPSC->SetRelativeLocation(FVector(0.f, +LeakSideOffsetCm, 0.f));
}

void ADoorActor::UpdateDoorVfx(float DeltaSeconds)
{
    EnsureDoorVfx();
    if (!SmokeLeakPSC) return;

    // Leak는 “문이 닫혀 있을 때”만 의미 있게
    const bool bClosed = (DoorState == EDoorState::Closed);

    const bool bAArmed = IsValid(RoomA) ? RoomA->IsBackdraftArmed() : false;
    const bool bBArmed = (LinkType == EDoorLinkType::RoomToRoom && IsValid(RoomB)) ? RoomB->IsBackdraftArmed() : false;

    // ===== Rule 1: 양쪽 Armed면 leak 없음 =====
    if ((LinkType == EDoorLinkType::RoomToRoom) && bAArmed && bBArmed)
    {
        LeakValSmoothed = FMath::FInterpTo(LeakValSmoothed, 0.f, DeltaSeconds, LeakInterpSpeed);
        SmokeLeakPSC->SetFloatParameter(LeakParamName, LeakValSmoothed);
        if (LeakValSmoothed <= 0.001f) SmokeLeakPSC->DeactivateSystem();
        return;
    }

    // ===== Rule 2: Armed vs Normal이면 Normal 방향으로 leak =====
    float TargetLeak = 0.f;
    bool bShouldLeak = false;

    if (bClosed)
    {
        if (LinkType == EDoorLinkType::RoomToOutside)
        {
            // RoomA armed면 바깥(=normal) 방향으로 leak
            if (bAArmed)
            {
                bShouldLeak = true;
                TargetLeak = LeakFromRoomA01;
                SetLeakSideToOutsideFromRoomA();
            }
        }
        else // RoomToRoom
        {
            // A armed, B normal -> leak toward B
            if (bAArmed && !bBArmed)
            {
                bShouldLeak = true;
                TargetLeak = LeakFromRoomA01;
                SetLeakSideToRoomB();
            }
            // B armed, A normal -> leak toward A
            else if (bBArmed && !bAArmed)
            {
                bShouldLeak = true;
                TargetLeak = LeakFromRoomB01;
                SetLeakSideToRoomA();
            }
        }
    }

    // Activate/Deactivate
    if (bShouldLeak && TargetLeak > 0.001f)
        SmokeLeakPSC->ActivateSystem(true);

    LeakValSmoothed = FMath::FInterpTo(LeakValSmoothed, bShouldLeak ? TargetLeak : 0.f, DeltaSeconds, LeakInterpSpeed);
    SmokeLeakPSC->SetFloatParameter(LeakParamName, LeakValSmoothed);

    if (!bShouldLeak && LeakValSmoothed <= 0.001f)
        SmokeLeakPSC->DeactivateSystem();
}

// Backdraft 발생 시: 문 확 열기 + Backdraft VFX Scale 세팅
void ADoorActor::OnRoomBackdraftTriggered()
{
    EnsureDoorVfx();

    // 요구사항: “양쪽 arm인 경우 backdraft도 X”
    const bool bAArmed = IsValid(RoomA) ? RoomA->IsBackdraftArmed() : false;
    const bool bBArmed = (LinkType == EDoorLinkType::RoomToRoom && IsValid(RoomB)) ? RoomB->IsBackdraftArmed() : false;

    if ((LinkType == EDoorLinkType::RoomToRoom) && bAArmed && bBArmed)
        return;

    // Backdraft VFX
    if (BackdraftPSC)
    {
        BackdraftPSC->ActivateSystem(true);

        // Scale = 현재 VentBoost에 준하는 값으로
        const float Scale = (DoorState == EDoorState::Breached) ? 1.0f : FMath::Clamp(ComputeVent01(), 0.f, 1.f);
        BackdraftPSC->SetFloatParameter(BackdraftScaleParamName, Scale);
    }

    // 문 확 열기
    if (bForceOpenOnBackdraft && DoorState != EDoorState::Breached)
    {
        // 즉시 완전 개방(상태/이벤트/힌지 모두 연동)
        SetOpenAmount01(1.0f);
    }
}
