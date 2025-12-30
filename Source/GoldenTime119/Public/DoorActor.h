// ============================ DoorActor.h ============================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DoorActor.generated.h"

class ARoomActor;
class USceneComponent;
class UParticleSystem;
class UParticleSystemComponent;

UENUM(BlueprintType)
enum class EDoorState : uint8
{
    Closed   UMETA(DisplayName = "Closed"),
    Open     UMETA(DisplayName = "Open"),
    Breached UMETA(DisplayName = "Breached"),
};

UENUM(BlueprintType)
enum class EDoorLinkType : uint8
{
    RoomToRoom     UMETA(DisplayName = "RoomToRoom"),
    RoomToOutside  UMETA(DisplayName = "RoomToOutside"),
};

UENUM(BlueprintType)
enum class EDoorOpenDirection : uint8
{
    PositiveYaw UMETA(DisplayName = "PositiveYaw"),
    NegativeYaw UMETA(DisplayName = "NegativeYaw"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDoorStateChanged, EDoorState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDoorOpenAmountChanged, float, OpenAmount01);

UCLASS()
class GOLDENTIME119_API ADoorActor : public AActor
{
    GENERATED_BODY()

public:
    ADoorActor();

    // ===== Rooms =====
    // Legacy: 과거 코드에서 OwningRoom을 쓰던 경우를 RoomA로 매핑
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Link")
    TObjectPtr<ARoomActor> OwningRoom = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Link")
    TObjectPtr<ARoomActor> RoomA = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Link")
    TObjectPtr<ARoomActor> RoomB = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Link")
    EDoorLinkType LinkType = EDoorLinkType::RoomToRoom;

    // ===== State =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|State")
    EDoorState DoorState = EDoorState::Closed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|State", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float OpenAmount01 = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|State", meta = (ClampMin = "0.0", ClampMax = "0.2"))
    float ClosedDeadzone01 = 0.02f;

    // ===== Vent/Leak =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Vent", meta = (ClampMin = "0.1", ClampMax = "6.0"))
    float VentPow = 1.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Vent", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float VentMax = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Leak", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LeakMax = 0.08f; // 완전 밀폐라도 아주 미세 누설(원하면 0)

    // ===== Door visual(hinge) =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Visual")
    FName HingeComponentName = TEXT("Hinge");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Visual")
    EDoorOpenDirection OpenDirection = EDoorOpenDirection::PositiveYaw;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Visual")
    float MaxOpenYawDeg = 110.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Visual")
    float ClosedYawOffsetDeg = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Visual", meta = (ClampMin = "0.0", ClampMax = "60.0"))
    float VisualInterpSpeed = 12.f;

    // ===== Debug =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Debug")
    bool bEnableDebugOpen = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Debug")
    float DebugOpenSpeed = 0.35f;

    // ===== Events =====
    UPROPERTY(BlueprintAssignable, Category = "Door|Event")
    FDoorStateChanged OnDoorStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Door|Event")
    FDoorOpenAmountChanged OnDoorOpenAmountChanged;

    // ===== API =====
    UFUNCTION(BlueprintCallable, Category = "Door|Vent")
    float ComputeVent01() const;

    UFUNCTION(BlueprintCallable, Category = "Door|Leak")
    float ComputeLeak01() const;

    UFUNCTION(BlueprintCallable, Category = "Door|Link")
    ARoomActor* GetOtherRoom(const ARoomActor* From) const;

    UFUNCTION(BlueprintCallable, Category = "Door|Link")
    bool IsOutsideConnectionFor(const ARoomActor* From) const;

    UFUNCTION(BlueprintCallable, Category = "Door|State")
    void SetOpenAmount01(float InOpen01);

    UFUNCTION(BlueprintCallable, Category = "Door|State")
    void SetClosed();

    UFUNCTION(BlueprintCallable, Category = "Door|State")
    void SetBreached();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // hinge cache
    UPROPERTY() TObjectPtr<USceneComponent> CachedHinge = nullptr;
    float VisualYawCurrent = 0.f;

    // debug opening flags
    bool bDebugOpening = false;
    bool bDebugClosing = false;

    // edge tracking
    EDoorState PrevStateForEdge = EDoorState::Closed;

private:
    void CacheHingeComponent();
    void ApplyDoorVisual(float DeltaSeconds);
    void ApplyStateByOpenAmount();

    // Room registration
    void SyncRoomRegistration(bool bRegister);
    void NotifyRoomsDoorOpenedOrBreached();
    void TryTriggerBackdraftIfNeeded(bool bFromBreach);

    // ===== Door VFX =====
private:
    UPROPERTY(EditAnywhere, Category = "Door|VFX")
    TObjectPtr<UParticleSystem> SmokeLeakTemplate = nullptr;

    UPROPERTY(EditAnywhere, Category = "Door|VFX")
    TObjectPtr<UParticleSystem> BackdraftTemplate = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "Door|VFX")
    TObjectPtr<UParticleSystemComponent> SmokeLeakPSC = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "Door|VFX")
    TObjectPtr<UParticleSystemComponent> BackdraftPSC = nullptr;

    UPROPERTY(EditAnywhere, Category = "Door|VFX")
    FName LeakParamName = TEXT("LeakVal");     // 단일 파라미터

    UPROPERTY(EditAnywhere, Category = "Door|VFX")
    FName BackdraftScaleParamName = TEXT("Scale"); // 단일 파라미터

    // Leak 방향 오프셋(문 기준 로컬 Right 방향)
    UPROPERTY(EditAnywhere, Category = "Door|VFX", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float LeakSideOffsetCm = 25.f;

    // Leak 값 스무딩
    UPROPERTY(EditAnywhere, Category = "Door|VFX", meta = (ClampMin = "0.0", ClampMax = "30.0"))
    float LeakInterpSpeed = 10.f;

    // Backdraft 시 문 확 열림
    UPROPERTY(EditAnywhere, Category = "Door|Backdraft")
    bool bForceOpenOnBackdraft = true;

    // 내부 상태(방별)
    float LeakFromRoomA01 = 0.f;
    float LeakFromRoomB01 = 0.f;

    // 최종 출력
    float LeakValSmoothed = 0.f;

private:
    void EnsureDoorVfx();
    void BindRoomSignals(bool bBind);

    void UpdateDoorVfx(float DeltaSeconds);
    void SetLeakSideToRoomA();
    void SetLeakSideToRoomB();
    void SetLeakSideToOutsideFromRoomA(); // RoomToOutside에서 RoomA 기준 바깥쪽

    // Room 이벤트 수신
    UFUNCTION()
    void OnRoomABackdraftLeakStrength(float Leak01);

    UFUNCTION()
    void OnRoomBBackdraftLeakStrength(float Leak01);

    UFUNCTION()
    void OnRoomBackdraftTriggered(); // 어느 방이든 Trigger되면 호출되게 바인딩(둘 다 같은 핸들러로 가능)
};
