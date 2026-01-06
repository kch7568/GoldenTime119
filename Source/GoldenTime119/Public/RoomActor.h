// ============================ RoomActor.h ============================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombustibleType.h"
#include "FireRuntimeTuning.h"
#include "RoomActor.generated.h"

class UBoxComponent;
class AFireActor;
class UCombustibleComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class ADoorActor;

UENUM(BlueprintType)
enum class ERoomState : uint8
{
    Idle UMETA(DisplayName = "IDLE"),
    Risk UMETA(DisplayName = "RISK"),
    Fire UMETA(DisplayName = "FIRE"),
};

USTRUCT(BlueprintType)
struct FRoomInfluence
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HeatAdd = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SmokeAdd = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float OxygenSub = 0.f;  // 0..1 per sec
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FireValueAdd = 0.f;
};

USTRUCT(BlueprintType)
struct FRoomEnvSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) float Heat = 0.f;
    UPROPERTY(BlueprintReadOnly) float Smoke = 0.f;        // 최종 합성(0..1)
    UPROPERTY(BlueprintReadOnly) float Oxygen = 1.f;       // 0..1
    UPROPERTY(BlueprintReadOnly) float FireValue = 0.f;
    UPROPERTY(BlueprintReadOnly) ERoomState State = ERoomState::Idle;
};

USTRUCT(BlueprintType)
struct FNeutralPlaneState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float NeutralPlaneZ = 200.f; // world Z (cm)
    UPROPERTY(BlueprintReadOnly) float UpperSmoke01 = 0.f;    // 0..1
    UPROPERTY(BlueprintReadOnly) float UpperTempC = 25.f;
    UPROPERTY(BlueprintReadOnly) float Vent01 = 0.f;          // 0..1
};

USTRUCT(BlueprintType)
struct FFirePolicy
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel") float InitialFuel = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel") float ConsumePerSecond_Min = 0.6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel") float ConsumePerSecond_Max = 1.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float IntensityRef = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float IntensityPow = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread") float SpreadRadius_Min = 250.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread") float SpreadRadius_Max = 900.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread") float SpreadInterval_Min = 0.45f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread") float SpreadInterval_Max = 1.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Influence") float HeatMul = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Influence") float SmokeMul = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Influence") float OxygenMul = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Influence") float FireValueMul = 1.f;
};

// ============================ Backdraft ============================
USTRUCT(BlueprintType)
struct FBackdraftParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdraft") float SmokeMin = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdraft") float FireValueMin = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdraft") float O2Max = 0.22f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdraft") float HeatMin = 30.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdraft") float ArmedHoldSeconds = 3.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdraft") float CooldownSeconds = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdraft") float VentBoostOnTrigger = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdraft") float FireValueBoost = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdraft") float SmokeDropOnTrigger = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdraft") bool bDisallowWhenRoomOnFire = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBackdraftEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBackdraftReadyChanged, bool, bReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBackdraftLeakStrength, float, Strength01);

UCLASS()
class GOLDENTIME119_API ARoomActor : public AActor
{
    GENERATED_BODY()

public:
    ARoomActor();

    UFUNCTION(BlueprintCallable, Category = "Room|Fire")
    float GetNearestFireDistance(const FVector& WorldPos) const;

    // ===== Env (권위값) =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Env") float Heat = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Env") float Oxygen = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Env") float FireValue = 0.f;

    // ===== Smoke (연동) =====
    // UpperSmoke는 NP.UpperSmoke01이 권위
    // LowerSmoke01은 Upper의 1/4을 기본 목표로(요구사항)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Smoke") float LowerSmoke01 = 0.f;

    // 최종 합성 Smoke(0..1): State/Backdraft/UI에서 이 값을 사용
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Smoke") float Smoke = 0.f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Room|State")
    ERoomState State = ERoomState::Idle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Threshold") float RiskHeatThreshold = 30.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Threshold") float MinOxygenToSustain = 0.15f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Volume")
    TObjectPtr<UBoxComponent> RoomBounds;

    // 스폰할 Fire 클래스
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Fire")
    TSubclassOf<AFireActor> FireClass;

    // ===== Backdraft Ready (연출/연소 억제) =====
    UPROPERTY(BlueprintAssignable, Category = "Room|Backdraft")
    FBackdraftReadyChanged OnBackdraftReadyChanged;

    UPROPERTY(BlueprintAssignable, Category = "Room|Backdraft")
    FBackdraftLeakStrength OnBackdraftLeakStrength;

    // 준비상태(Ready)일 때 연소 스케일: 요구사항 “연소 거의 최소”
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Backdraft", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BackdraftReadyCombustionScale = 0.05f;

    // 누출 연출 강도 계산용(연기 임계)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Backdraft", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BackdraftLeakStartSmoke01 = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Backdraft", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BackdraftLeakFullSmoke01 = 0.95f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Backdraft", meta = (ClampMin = "0.1", ClampMax = "8.0"))
    float BackdraftLeakCurvePow = 3.0f;

    UFUNCTION(BlueprintCallable, Category = "Room|Backdraft")
    bool IsBackdraftReady() const { return bBackdraftReady; }

    // FireActor가 참조할 연소 스케일(Ready면 0.05)
    UFUNCTION(BlueprintCallable, Category = "Room|Backdraft")
    float GetBackdraftCombustionScale01() const
    {
        return bBackdraftReady ? BackdraftReadyCombustionScale : 1.0f;
    }

    // BP에서 문틈 연기 VFX 강도(0..1)를 바로 쓰기 좋게
    UFUNCTION(BlueprintCallable, Category = "Room|Backdraft")
    float ComputeBackdraftLeakStrength01() const;

    // 타입별 정책
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Policy") FFirePolicy PolicyNormal;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Policy") FFirePolicy PolicyOil;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Policy") FFirePolicy PolicyElectric;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Policy") FFirePolicy PolicyExplosive;

    // ===== NeutralPlane =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|NeutralPlane") bool bEnableNeutralPlane = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|NeutralPlane") float FloorZ = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|NeutralPlane") float CeilingZ = 300.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|NeutralPlane") FNeutralPlaneState NP;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|NeutralPlane") float SmokeToUpperFillRate = 0.03f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|NeutralPlane") float VentSmokeRemoveRate = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|NeutralPlane") float NeutralPlaneDropPerSec = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|NeutralPlane") float NeutralPlaneRisePerSec = 80.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|NeutralPlane") float MinNeutralPlaneFromFloor = 40.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|NeutralPlane") float MaxNeutralPlaneFromCeiling = 10.f;

    UFUNCTION(BlueprintCallable, Category = "Room|NeutralPlane")
    FNeutralPlaneState GetNeutralPlane() const { return NP; }


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|SmokeVolume", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float OpaqueStartSmoke01 = 0.5f;

    // 이 값쯤에서 거의 불투명(최대)으로
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|SmokeVolume", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float OpaqueFullSmoke01 = 0.75f;

    // 임계 이후 곡선 강도(클수록 더 “훅” 올라감)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|SmokeVolume", meta = (ClampMin = "1.0", ClampMax = "8.0"))
    float OpaqueCurvePow = 3.5f;

    // 불투명 부스트(기본 UpperSmokeOpacity에 곱)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|SmokeVolume", meta = (ClampMin = "1.0", ClampMax = "4.0"))
    float OpaqueBoostMul = 2.2f;
    // ===== Door / Vent aggregation =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Door") bool bEnableDoorVentAggregation = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Door")
    float SealEpsilonVent01 = 0.02f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Door")
    float VentAggregateClampMax = 1.f;

    // ===== Door Exchange (방<->방, 방<->밖) =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|DoorExchange")
    float DoorSmokeExchangeRate = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|DoorExchange")
    float DoorOxygenExchangeRate = 0.45f;

    // ===== Smoke 연동 규칙 =====
    // 요구사항: 하층은 상층의 1/4
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Smoke")
    float LowerSmokeTargetRatio = 0.25f; // 1/4

    // 상층/하층 부드러운 추종
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Smoke")
    float LowerSmokeFollowSpeed = 1.25f;

    // 최종 Smoke 합성(상층+하층)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Smoke")
    float UpperSmokeWeight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Smoke")
    float LowerSmokeWeight = 1.0f;

    // ===== 자연 감쇠/회복(화재 꺼짐/환기) =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Relax")
    float HeatCoolToAmbientPerSec = 0.08f;   // Heat -> 0으로 수렴

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Relax")
    float FireValueDecayPerSec = 0.18f;      // FireValue -> 0

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Relax")
    float OxygenRecoverPerSec = 0.25f;       // Oxygen -> 1 (Vent 영향을 받음)

    // Smoke는 Upper/Lower 자체가 문/환기에서 제거되므로, 별도 자연감쇠는 약하게만
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Relax")
    float SmokeNaturalDissipatePerSec = 0.02f; // UpperSmoke01/LoweSmoke01 추가 소실(작게)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|SmokeVolume", meta = (ClampMin = "0.05", ClampMax = "0.5"))
    float LowerVolumeHeightRatioToUpper = 0.25f;

    // ===== Backdraft =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Backdraft") bool bEnableBackdraft = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Backdraft") FBackdraftParams Backdraft;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Backdraft") float SealedTime = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Backdraft") bool bBackdraftArmed = false;

    UPROPERTY(BlueprintAssignable, Category = "Room|Backdraft") FBackdraftEvent OnBackdraft;

    UFUNCTION(BlueprintCallable, Category = "Room|Backdraft") bool CanArmBackdraft() const;
    UFUNCTION(BlueprintCallable, Category = "Room|Backdraft") bool IsBackdraftArmed() const { return bBackdraftArmed; }

    UFUNCTION(BlueprintCallable, Category = "Room|Backdraft") void NotifyDoorSealed(bool bSealed);

    UFUNCTION(BlueprintCallable, Category = "Room|Backdraft")
    void TriggerBackdraft(const FTransform& DoorTM, float VentBoost01);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Backdraft")
    bool bIgniteOnBackdraft = true;
    void ApplyOxygenCapBySmoke(float DeltaSeconds);

    // ===== Smoke Volumes (상층/하층 2개) =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|SmokeVolume") bool bEnableSmokeVolume = true;

    // 동일 클래스 재사용 가능(BP_Smoke)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|SmokeVolume")
    TSubclassOf<AActor> SmokeVolumeClass;

    // 상층 기본 오파시티
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|SmokeVolume")
    float UpperSmokeOpacity = 0.75f;

    // 하층은 상층의 1/4
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|SmokeVolume")
    float LowerSmokeOpacityScale = 0.25f;

    // 머티리얼 파라미터
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|SmokeVolume")
    float SmokeFadeHeight = 200.f;

    // 방 벽과 z-fighting / clipping 완화(0.99 추천)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|SmokeVolume")
    float SmokeXYInset = 0.99f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|SmokeVolume")
    float SmokeCeilingAttachOffset = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|SmokeVolume")
    float SmokeCubeBaseSize = 100.f;

    // ===== Events =====
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRoomFireEvent, AFireActor*, Fire);
    UPROPERTY(BlueprintAssignable, Category = "Room|Event") FRoomFireEvent OnFireStarted;
    UPROPERTY(BlueprintAssignable, Category = "Room|Event") FRoomFireEvent OnFireExtinguished;
    UPROPERTY(BlueprintAssignable, Category = "Room|Event") FRoomFireEvent OnFireSpawned;

public:
    void RegisterCombustible(UCombustibleComponent* Comb);
    void UnregisterCombustible(UCombustibleComponent* Comb);

    void RegisterFire(AFireActor* Fire);
    void UnregisterFire(const FGuid& FireId);

    void AccumulateInfluence(ECombustibleType Type, float EffectiveIntensity, float InfluenceScale);
    bool GetRuntimeTuning(ECombustibleType Type, float EffectiveIntensity, float FuelRatio01, FFireRuntimeTuning& Out) const;

    bool CanSustainFire() const { return Oxygen > MinOxygenToSustain; }

    UFUNCTION(BlueprintCallable, Category = "Room|Fire")
    void IgniteAllCombustiblesInRoom(bool bAllowElectric = false);
    UFUNCTION(BlueprintPure, Category = "Room|Fire")
    int32 GetActiveFireCount() const;
    UFUNCTION(BlueprintCallable, Category = "Room|Env")
    FRoomEnvSnapshot GetEnvSnapshot() const;

    AFireActor* SpawnFireForCombustible(UCombustibleComponent* TargetComb, ECombustibleType Type);
    void GetCombustiblesInRoom(TArray<UCombustibleComponent*>& Out, bool bExcludeBurning = true) const;

    UFUNCTION(BlueprintCallable, Category = "Room|Test")
    void Debug_RescanCombustibles();

    UFUNCTION(BlueprintCallable, Category = "Room|Fire")
    AFireActor* IgniteActor(AActor* TargetActor);

    UFUNCTION(BlueprintCallable, Category = "Room|Fire")
    AFireActor* IgniteRandomCombustibleInRoom(bool bAllowElectric = false);

    // Door registry (멀티 도어 지원)
    UFUNCTION(BlueprintCallable, Category = "Room|Door")
    void RegisterDoor(ADoorActor* Door);

    UFUNCTION(BlueprintCallable, Category = "Room|Door")
    void UnregisterDoor(ADoorActor* Door);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    UPROPERTY() TSet<TWeakObjectPtr<UCombustibleComponent>> Combustibles;
    UPROPERTY() TMap<FGuid, TObjectPtr<AFireActor>> ActiveFires;

    float AccHeat = 0.f;
    float AccSmoke = 0.f;
    float AccOxygenSub = 0.f;
    float AccFireValue = 0.f;

    // SmokeVolume runtime (2개)
    UPROPERTY() TObjectPtr<AActor> UpperSmokeActor = nullptr;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> UpperSmokeMesh = nullptr;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> UpperSmokeMID = nullptr;

    UPROPERTY() TObjectPtr<AActor> LowerSmokeActor = nullptr;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> LowerSmokeMesh = nullptr;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LowerSmokeMID = nullptr;

    // Doors
    UPROPERTY() TArray<TWeakObjectPtr<ADoorActor>> Doors;

    // Backdraft internal
    float LastBackdraftTime = -1000.f;

    // Backdraft Ready internal
    UPROPERTY(VisibleAnywhere, Category = "Room|Backdraft")
    bool bBackdraftReady = false;

    void UpdateBackdraftReadyAndLeak(float DeltaSeconds);

private:
    const FFirePolicy& GetPolicy(ECombustibleType Type) const;
    FRoomInfluence BaseInfluence(ECombustibleType Type, float EffectiveIntensity) const;

    void ApplyAccumulators(float DeltaSeconds);
    void ResetAccumulators();
    void UpdateRoomState();

    void RelaxEnv(float DeltaSeconds);

    static bool IsInsideRoomBox(const UBoxComponent* Box, const FVector& WorldPos);
    void UpdateRoomGeometryFromBounds();

    void UpdateNeutralPlane(float DeltaSeconds);

    // Door 합성/교환
    void UpdateVentFromDoors();
    void ApplyDoorExchange(float DeltaSeconds);

    // Smoke 연동(핵심)
    void RebuildSmokeFromNP(float DeltaSeconds);

    // Backdraft
    void EvaluateBackdraftArming(float DeltaSeconds);

    // SmokeVolume
    void EnsureSmokeVolumesSpawned();
    void UpdateSmokeVolumesTransform();
    void PushSmokeMaterialParams();

    // Overlap
    UFUNCTION() void OnRoomBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);
    UFUNCTION() void OnRoomEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    

public:
    // ===== 백드래프트 압력 (환기 구멍 시스템) =====

// 백드래프트 압력 (0~1, Armed 상태에서 축적, 환기 구멍으로 감소)
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Room|Backdraft")
    float BackdraftPressure = 0.f;

    // 압력이 이 값 이하로 떨어지면 백드래프트 Armed 해제
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Backdraft")
    float BackdraftSafeThreshold = 0.2f;

    // 압력 축적 속도 (Armed 상태에서 초당)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Backdraft")
    float BackdraftPressureBuildRate = 0.1f;

    // 환기 구멍을 통한 압력 감소 속도 배율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Backdraft")
    float VentHolePressureReleaseMultiplier = 1.0f;

    // 현재 환기 중인 문들의 총 환기율
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Room|Backdraft")
    float TotalDoorVentRate = 0.f;

    // ===== 환기 구멍 API =====

    // 문에서 환기 구멍이 생겼을 때 호출
    UFUNCTION(BlueprintCallable, Category = "Room|Backdraft")
    void AddDoorVentHole(ADoorActor* Door, float VentRate);

    // 현재 백드래프트 압력 반환
    UFUNCTION(BlueprintCallable, Category = "Room|Backdraft")
    float GetBackdraftPressure() const { return BackdraftPressure; }

    // 백드래프트가 안전한 상태인지
    UFUNCTION(BlueprintCallable, Category = "Room|Backdraft")
    bool IsBackdraftSafe() const { return BackdraftPressure <= BackdraftSafeThreshold; }

private:
    // 환기 중인 문 목록
    UPROPERTY()
    TMap<TWeakObjectPtr<ADoorActor>, float> VentingDoors;

    // 환기 구멍으로 인한 압력 감소 처리
    void UpdateBackdraftPressure(float DeltaSeconds);
};
