// ============================ DoorActor.h ============================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DoorActor.generated.h"

class ARoomActor;

UENUM(BlueprintType)
enum class EDoorState : uint8
{
    Closed     UMETA(DisplayName = "Closed"),
    Open       UMETA(DisplayName = "Open"),
    Breached   UMETA(DisplayName = "Breached"),
};

UENUM(BlueprintType)
enum class EDoorLinkType : uint8
{
    RoomToRoom     UMETA(DisplayName = "Room ↔ Room"),
    RoomToOutside  UMETA(DisplayName = "Room ↔ Outside"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDoorStateChanged, EDoorState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDoorOpenAmountChanged, float, OpenAmount01);

UCLASS()
class GOLDENTIME119_API ADoorActor : public AActor
{
    GENERATED_BODY()

public:
    ADoorActor();

    // ===== Link =====
    // 기본: RoomA 필수. RoomB가 null이면 Outside로 보정됨.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Link")
    EDoorLinkType LinkType = EDoorLinkType::RoomToRoom;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Link")
    TObjectPtr<ARoomActor> RoomA = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Link", meta = (EditCondition = "LinkType==EDoorLinkType::RoomToRoom"))
    TObjectPtr<ARoomActor> RoomB = nullptr;

    // (호환용) 기존 BP에서 OwningRoom만 쓰던 경우 자동 매핑
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Legacy", meta = (DisplayName = "OwningRoom (Legacy)"))
    TObjectPtr<ARoomActor> OwningRoom = nullptr;

    // ===== State =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
    EDoorState DoorState = EDoorState::Closed;

    // 0..1 (0=닫힘, 1=완전개방)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float OpenAmount01 = 0.f;

    // ===== Vent =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Vent", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float VentMax = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Vent", meta = (ClampMin = "0.1", ClampMax = "4.0"))
    float VentPow = 1.6f;

    // 닫혀있을 때 틈새 누출(연기 VFX량)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Leak", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LeakMax = 0.35f;

    // “닫힘” 판정 데드존 (OpenAmount가 아주 작은 값일 때 Closed로 간주)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Seal", meta = (ClampMin = "0.0", ClampMax = "0.2"))
    float ClosedDeadzone01 = 0.01f;

    // ===== Debug (Non-VR) =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Debug")
    bool bEnableDebugOpen = true;

    // 2번 키를 누르면 열기 시작, 천천히 OpenAmount 증가
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Debug")
    float DebugOpenSpeed = 0.4f; // 초당 OpenAmount 증가량

    // ===== Events (BP에서 VFX/SFX 연결) =====
    UPROPERTY(BlueprintAssignable, Category = "Door|Event")
    FDoorStateChanged OnDoorStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Door|Event")
    FDoorOpenAmountChanged OnDoorOpenAmountChanged;

public:
    // ===== API =====
    UFUNCTION(BlueprintCallable, Category = "Door")
    void SetOpenAmount01(float InOpen01);

    UFUNCTION(BlueprintCallable, Category = "Door")
    void SetClosed();

    UFUNCTION(BlueprintCallable, Category = "Door")
    void SetBreached();

    UFUNCTION(BlueprintCallable, Category = "Door")
    bool IsSealed() const { return DoorState == EDoorState::Closed; }

    UFUNCTION(BlueprintCallable, Category = "Door")
    float ComputeVent01() const;

    UFUNCTION(BlueprintCallable, Category = "Door")
    float ComputeLeak01() const;

    // ===== Link query for RoomActor =====
    UFUNCTION(BlueprintCallable, Category = "Door|Link")
    ARoomActor* GetOtherRoom(const ARoomActor* From) const;

    UFUNCTION(BlueprintCallable, Category = "Door|Link")
    bool IsOutsideConnectionFor(const ARoomActor* From) const;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void ApplyStateByOpenAmount();
    void SyncRoomRegistration(bool bRegister);
    void NotifyRoomsDoorOpenedOrBreached(); // 열림/파손 시 방 sealed 리셋 힌트
    void TryTriggerBackdraftIfNeeded(bool bFromBreach);

private:
    EDoorState PrevStateForEdge = EDoorState::Closed;
    bool bDebugOpening = false;
};
