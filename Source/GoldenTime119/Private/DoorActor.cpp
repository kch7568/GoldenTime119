// ============================ DoorActor.cpp ============================
#include "DoorActor.h"
#include "RoomActor.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h" // EKeys

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

    // 초기 상태
    ApplyStateByOpenAmount();
    PrevStateForEdge = DoorState;

    // 양쪽 Room 등록(멀티 도어 합성용)
    SyncRoomRegistration(true);

    // 초기 상태가 Open/Breached라면 Room에 “열림” 리셋 힌트
    if (DoorState != EDoorState::Closed)
        NotifyRoomsDoorOpenedOrBreached();
}

void ADoorActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bEnableDebugOpen) return;
    if (DoorState == EDoorState::Breached) return;

    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;

    // 2번 키를 누르면 열기 시작 (누르고 있는 동안만이 아니라 "누르면 시작" 방식)
    if (PC->IsInputKeyDown(EKeys::Two))
        bDebugOpening = true;

    // 천천히 열기
    if (bDebugOpening)
    {
        SetOpenAmount01(OpenAmount01 + DebugOpenSpeed * DeltaSeconds);

        // 완전 개방 시 멈춤
        if (OpenAmount01 >= 0.999f)
            bDebugOpening = false;
    }
}

void ADoorActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    SyncRoomRegistration(false);
    Super::EndPlay(EndPlayReason);
}

void ADoorActor::SyncRoomRegistration(bool bRegister)
{
    // RoomA
    if (IsValid(RoomA))
    {
        if (bRegister) RoomA->RegisterDoor(this);
        else          RoomA->UnregisterDoor(this);
    }

    // RoomB (RoomToRoom일 때만)
    if (LinkType == EDoorLinkType::RoomToRoom && IsValid(RoomB))
    {
        if (bRegister) RoomB->RegisterDoor(this);
        else          RoomB->UnregisterDoor(this);
    }
}

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

// ===== Link query for RoomActor =====
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

void ADoorActor::SetOpenAmount01(float InOpen01)
{
    // 파손 이후에는 상태 되돌리기 방지
    if (DoorState == EDoorState::Breached)
        return;

    const float NewOpen = FMath::Clamp(InOpen01, 0.f, 1.f);

    // 값 변화 없으면 패스
    if (FMath::IsNearlyEqual(NewOpen, OpenAmount01, 0.0001f))
        return;

    OpenAmount01 = NewOpen;
    OnDoorOpenAmountChanged.Broadcast(OpenAmount01);

    const EDoorState Prev = DoorState;
    ApplyStateByOpenAmount();

    if (Prev != DoorState)
        OnDoorStateChanged.Broadcast(DoorState);

    // “열림/파손”이면 Room에 즉시 리셋 힌트(멀티 도어 안전)
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

    // 열림/파손 리셋 + 즉시 트리거 시도
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
    // 멀티 도어에서는 sealed 판정은 Room이 Vent 합성으로 처리.
    // 여기서는 “열림이면 즉시 리셋” 신호만 주면 충분.
    if (IsValid(RoomA)) RoomA->NotifyDoorSealed(false);
    if (LinkType == EDoorLinkType::RoomToRoom && IsValid(RoomB)) RoomB->NotifyDoorSealed(false);
}

void ADoorActor::TryTriggerBackdraftIfNeeded(bool bFromBreach)
{
    const float VentBoost = bFromBreach ? 1.f : ComputeVent01();

    // 문이 열리는 순간, 양쪽 방 각각이 Armed면 각각 폭발 가능
    if (IsValid(RoomA) && RoomA->IsBackdraftArmed())
        RoomA->TriggerBackdraft(GetActorTransform(), VentBoost);

    if (LinkType == EDoorLinkType::RoomToRoom && IsValid(RoomB) && RoomB->IsBackdraftArmed())
        RoomB->TriggerBackdraft(GetActorTransform(), VentBoost);
}
