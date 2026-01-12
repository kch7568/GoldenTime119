// ============================ DoorActor.h ============================
#pragma once

#include "CoreMinimal.h"
#include "GrabInteractable.h"
#include "GameFramework/Actor.h"
#include "DoorActor.generated.h"

class ARoomActor;
class USceneComponent;
class UParticleSystem;
class UParticleSystemComponent;
class UBreakableComponent;
class UDecalComponent;

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

// 환기 구멍 정보
USTRUCT(BlueprintType)
struct FVentHoleInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FVector LocalPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    float CreatedTime = 0.f;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<UDecalComponent> CrackDecal = nullptr;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<UParticleSystemComponent> VentSmokePSC = nullptr;

    // 이 구멍에서 연기가 나오고 있는지
    UPROPERTY(BlueprintReadOnly)
    bool bIsVenting = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDoorStateChanged, EDoorState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDoorOpenAmountChanged, float, OpenAmount01);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVentHoleCreated, int32, TotalHoleCount);

UCLASS()
class GOLDENTIME119_API ADoorActor : public AActor, public IGrabInteractable
{
    GENERATED_BODY()

public:
    ADoorActor();

    // ===== Rooms =====
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
    float LeakMax = 0.08f;

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

    // ===== Breakable (도끼로 파괴) =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Breakable")
    TObjectPtr<UBreakableComponent> Breakable = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Breakable")
    bool bIsBreakable = true;

    UPROPERTY(EditAnywhere, Category = "Door|Breakable")
    TArray<TObjectPtr<UStaticMesh>> DebrisMeshes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Breakable")
    int32 DebrisCount = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Breakable")
    float DebrisImpulseStrength = 500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Breakable")
    bool bHideDoorMeshOnBreak = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Breakable")
    FName DoorMeshComponentName = TEXT("DoorMesh");

    // ===== 환기 구멍 시스템 =====

    // 환기 구멍 생성 HP 임계값 (이 비율 이하로 내려가면 구멍 생성)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|VentHole")
    float VentHoleCreateThreshold = 0.7f;

    // 환기 구멍 최대 개수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|VentHole")
    int32 MaxVentHoles = 3;

    // 구멍당 백드래프트 압력 감소 속도 (초당)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|VentHole")
    float VentRatePerHole = 0.15f;

    // 환기 구멍에서 연기가 나오는 최소 BackdraftPressure
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|VentHole")
    float MinPressureForVentSmoke = 0.1f;

    // 현재 환기 구멍들
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Door|VentHole")
    TArray<FVentHoleInfo> VentHoles;

    // 환기 구멍용 균열 데칼 머티리얼
    UPROPERTY(EditAnywhere, Category = "Door|VentHole|VFX")
    TObjectPtr<UMaterialInterface> CrackDecalMaterial;

    // 환기 구멍용 연기 VFX
    UPROPERTY(EditAnywhere, Category = "Door|VentHole|VFX")
    TObjectPtr<UParticleSystem> VentSmokeTemplate;

    // 균열 데칼 크기
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|VentHole|VFX")
    FVector CrackDecalSize = FVector(30.f, 15.f, 15.f);

    // ===== Events =====
    UPROPERTY(BlueprintAssignable, Category = "Door|Event")
    FDoorStateChanged OnDoorStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Door|Event")
    FDoorOpenAmountChanged OnDoorOpenAmountChanged;

    UPROPERTY(BlueprintAssignable, Category = "Door|VentHole")
    FOnVentHoleCreated OnVentHoleCreated;

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

    // ===== Breakable API =====
    UFUNCTION(BlueprintCallable, Category = "Door|Breakable")
    bool IsBreakableDoor() const;

    UFUNCTION(BlueprintCallable, Category = "Door|Breakable")
    float GetBreakableHPRatio() const;

    // ===== VentHole API =====
    UFUNCTION(BlueprintCallable, Category = "Door|VentHole")
    int32 GetVentHoleCount() const { return VentHoles.Num(); }

    UFUNCTION(BlueprintCallable, Category = "Door|VentHole")
    float GetTotalVentRate() const { return VentHoles.Num() * VentRatePerHole; }

    UFUNCTION(BlueprintCallable, Category = "Door|VentHole")
    bool HasVentHoles() const { return VentHoles.Num() > 0; }

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // hinge cache
    UPROPERTY() TObjectPtr<USceneComponent> CachedHinge = nullptr;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> CachedDoorMesh = nullptr;
    float VisualYawCurrent = 0.f;

    // debug opening flags
    bool bDebugOpening = false;
    bool bDebugClosing = false;

    // edge tracking
    EDoorState PrevStateForEdge = EDoorState::Closed;

    // VentHole tracking
    float LastHPRatioForVentHole = 1.f;
    FVector LastDamageLocation = FVector::ZeroVector;

private:
    void CacheHingeComponent();
    void CacheDoorMeshComponent();
    void ApplyDoorVisual(float DeltaSeconds);
    void ApplyStateByOpenAmount();

    // Room registration
    void SyncRoomRegistration(bool bRegister);
    void NotifyRoomsDoorOpenedOrBreached();
    void TryTriggerBackdraftIfNeeded(bool bFromBreach);

    // ===== Breakable =====
    void SetupBreakableComponent();

    UFUNCTION()
    void OnDoorBrokenByAxe();

    UFUNCTION()
    void OnDoorDamagedByAxe(float Damage, float RemainingHP);

    void SpawnDebris();

    // ===== VentHole =====
    void CheckAndCreateVentHole(float CurrentHPRatio, FVector HitLocation);
    void CreateVentHole(FVector LocalPosition);
    void UpdateVentHoleEffects(float DeltaSeconds);
    void NotifyRoomVentHoleCreated();
    float GetBackdraftPressureFromRoom() const;

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
    FName LeakParamName = TEXT("LeakVal");

    UPROPERTY(EditAnywhere, Category = "Door|VFX")
    FName BackdraftScaleParamName = TEXT("Scale");

    UPROPERTY(EditAnywhere, Category = "Door|VFX", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float LeakSideOffsetCm = 25.f;

    UPROPERTY(EditAnywhere, Category = "Door|VFX", meta = (ClampMin = "0.0", ClampMax = "30.0"))
    float LeakInterpSpeed = 10.f;

    UPROPERTY(EditAnywhere, Category = "Door|Backdraft")
    bool bForceOpenOnBackdraft = true;

    float LeakFromRoomA01 = 0.f;
    float LeakFromRoomB01 = 0.f;
    float LeakValSmoothed = 0.f;

private:
    void EnsureDoorVfx();
    void BindRoomSignals(bool bBind);

    void UpdateDoorVfx(float DeltaSeconds);
    void SetLeakSideToRoomA();
    void SetLeakSideToRoomB();
    void SetLeakSideToOutsideFromRoomA();

    UFUNCTION()
    void OnRoomABackdraftLeakStrength(float Leak01);

    UFUNCTION()
    void OnRoomBBackdraftLeakStrength(float Leak01);

    UFUNCTION()
    void OnRoomBackdraftTriggered();

    FTimerHandle BackdraftDelayTimer;
    FTransform PendingBackdraftDoorTM;
    float PendingBackdraftVentBoost;
    ARoomActor* PendingBackdraftRoom = nullptr;

    // 딜레이 후 호출될 함수
    void ExecuteDelayedBackdraft();

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Backdraft")
    bool bDoorCanBeBlownOff = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Backdraft")
    float BackdraftDoorImpulse = 800.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Backdraft")
    float BackdraftDoorTorque = 500.f;

    UPROPERTY(EditAnywhere, Category = "VFX|Leak")
    float LeakMinWhenArmed = 0.3f;  // Armed 상태일 때 최소 연기량

    // IGrabInteractable 인터페이스 구현
    virtual void OnGrabbed_Implementation(USceneComponent* GrabbingController, bool bIsLeftHand) override;
    virtual void OnReleased_Implementation(USceneComponent* GrabbingController, bool bIsLeftHand) override;
    virtual bool CanBeGrabbed_Implementation() const override;
protected:
    // [추가] VR 상호작용 관련 내부 변수
    UPROPERTY(VisibleAnywhere, Category = "Door|VR")
    bool bIsGrabbed = false;

    UPROPERTY(VisibleAnywhere, Category = "Door|VR")
    TObjectPtr<USceneComponent> GrabbingController = nullptr;

    float InitialHandYaw = 0.f;
    float InitialOpenAmount = 0.f;

    // [추가] 매 프레임 문 각도를 업데이트하는 함수
    void UpdateDoorFromController();
};