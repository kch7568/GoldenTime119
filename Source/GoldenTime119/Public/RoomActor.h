// ============================ RoomActor.h ============================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombustibleType.h"
#include "RoomActor.generated.h"

class UBoxComponent;
class AFireActor;
class UCombustibleComponent;

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
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HeatAdd = 0.f;     // per second-ish
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SmokeAdd = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float OxygenSub = 0.f;  // 0..1 per sec
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FireValueAdd = 0.f;
};

USTRUCT(BlueprintType)
struct FRoomEnvSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) float Heat = 0.f;
    UPROPERTY(BlueprintReadOnly) float Smoke = 0.f;
    UPROPERTY(BlueprintReadOnly) float Oxygen = 1.f; // 0..1
    UPROPERTY(BlueprintReadOnly) float FireValue = 0.f;
    UPROPERTY(BlueprintReadOnly) ERoomState State = ERoomState::Idle;
};

// Fire가 룸에서 “현재 튜닝 결과”를 받아갈 때 쓰는 구조체
USTRUCT(BlueprintType)
struct FFireRuntimeTuning
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float FuelRatio01 = 1.f;     // combustible fuel ratio
    UPROPERTY(BlueprintReadOnly) float Intensity01 = 1.f;     // 0..1

    UPROPERTY(BlueprintReadOnly) float SpreadRadius = 350.f;
    UPROPERTY(BlueprintReadOnly) float SpreadInterval = 1.f;

    UPROPERTY(BlueprintReadOnly) float ConsumePerSecond = 1.f;

    UPROPERTY(BlueprintReadOnly) float InfluenceScale = 1.f;
};

USTRUCT(BlueprintType)
struct FNeutralPlaneState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float NeutralPlaneZ = 200.f;   // cm
    UPROPERTY(BlueprintReadOnly) float UpperSmoke01 = 0.f;     // 0..1
    UPROPERTY(BlueprintReadOnly) float UpperTempC = 25.f;    // 선택
    UPROPERTY(BlueprintReadOnly) float Vent01 = 0.f;     // 0..1
};

// 타입별 정책(최소)
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

UCLASS()
class GOLDENTIME119_API ARoomActor : public AActor
{
    GENERATED_BODY()

public:
    ARoomActor();

    // ===== Env =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Env") float Heat = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Env") float Smoke = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Env") float Oxygen = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Env") float FireValue = 0.f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Room|State") ERoomState State = ERoomState::Idle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Threshold") float RiskHeatThreshold = 30.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Threshold") float MinOxygenToSustain = 0.15f;

    // 인접 룸 (필요하면 Spread에서 약하게 전달)
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Room|Link")
    TArray<TObjectPtr<ARoomActor>> AdjacentRooms;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Volume")
    TObjectPtr<UBoxComponent> RoomBounds;

    // 스폰할 Fire 클래스
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Fire")
    TSubclassOf<AFireActor> FireClass;

    // 타입별 정책
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Policy") FFirePolicy PolicyNormal;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Policy") FFirePolicy PolicyOil;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Policy") FFirePolicy PolicyElectric;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Policy") FFirePolicy PolicyExplosive;

    // Spread 후보 제한(성능/게임성)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Spread") int32 SpreadTopK_Normal = 3;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Spread") int32 SpreadTopK_Oil = 6;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Spread") int32 SpreadTopK_Electric = 4;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Spread") int32 SpreadTopK_Explosive = 12;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|NeutralPlane")
    bool bEnableNeutralPlane = true;

    // 룸 높이/바닥 높이는 Box로부터 자동 계산 (cm)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|NeutralPlane")
    float FloorZ = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|NeutralPlane")
    float CeilingZ = 300.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|NeutralPlane")
    FNeutralPlaneState NP;

    // 튜닝 파라미터
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|NeutralPlane")
    float SmokeToUpperFillRate = 0.12f;   // Smoke -> UpperSmoke01 전환 비율

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|NeutralPlane")
    float VentSmokeRemoveRate = 0.35f;    // 환기 시 UpperSmoke01 감소

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|NeutralPlane")
    float NeutralPlaneDropPerSec = 90.f;  // UpperSmoke01이 높을수록 cm/s로 내려오는 속도

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|NeutralPlane")
    float NeutralPlaneRisePerSec = 60.f;  // 환기/진압 등으로 올라가는 속도

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|NeutralPlane")
    float MinNeutralPlaneFromFloor = 40.f; // cm (완전 바닥까지는 안 가게)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|NeutralPlane")
    float MaxNeutralPlaneFromCeiling = 10.f; // cm (천장 바로 밑까지만)

    // ===== Events =====
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRoomFireEvent, AFireActor*, Fire);
    UPROPERTY(BlueprintAssignable, Category = "Room|Event") FRoomFireEvent OnFireStarted;
    UPROPERTY(BlueprintAssignable, Category = "Room|Event") FRoomFireEvent OnFireExtinguished;
    UPROPERTY(BlueprintAssignable, Category = "Room|Event") FRoomFireEvent OnFireSpawned; // spread or ignite

public:
    // Combustible 등록(Overlap/수동 둘 다 가능)
    void RegisterCombustible(UCombustibleComponent* Comb);
    void UnregisterCombustible(UCombustibleComponent* Comb);

    // Fire 등록(룸 상태용)
    void RegisterFire(AFireActor* Fire);
    void UnregisterFire(const FGuid& FireId);

    // Fire -> Room 영향 누적
    void AccumulateInfluence(ECombustibleType Type, float EffectiveIntensity, float InfluenceScale);

    // Fire가 런타임 튜닝 요청(연료는 Combustible이 권위자. Room은 “정책+환경 기반”으로만 튜닝)
    bool GetRuntimeTuning(ECombustibleType Type, float EffectiveIntensity, float FuelRatio01, FFireRuntimeTuning& Out) const;

    // Fire 유지 가능? (룸 산소 기반만)
    bool CanSustainFire() const { return Oxygen > MinOxygenToSustain; }

    // Env snapshot (Combustible/Fire가 읽음)
    UFUNCTION(BlueprintCallable, Category = "Room|Env")
    FRoomEnvSnapshot GetEnvSnapshot() const;

    // “가연물”이 최종 권위: 이 함수로 Fire 스폰 요청
    AFireActor* SpawnFireForCombustible(UCombustibleComponent* TargetComb, ECombustibleType Type);

    // Spread용: 룸 내부 가연물 리스트 제공 (RoomBounds 내부 + 등록된 컴포넌트 기준)
    void GetCombustiblesInRoom(TArray<UCombustibleComponent*>& Out, bool bExcludeBurning = true) const;

    // BP 테스트 편의
    UFUNCTION(BlueprintCallable, Category = "Room|NeutralPlane")
    FNeutralPlaneState GetNeutralPlane() const { return NP; }

    // 문/창/배연 상태를 BP/코드에서 주입
    UFUNCTION(BlueprintCallable, Category = "Room|NeutralPlane")
    void SetVent01(float InVent01) { NP.Vent01 = FMath::Clamp(InVent01, 0.f, 1.f); }
    void Debug_RescanCombustibles(); // RoomBounds 내부에서 찾아서 등록

    UFUNCTION(BlueprintCallable, Category = "Room|Fire")
    AFireActor* IgniteActor(AActor* TargetActor);

    // 방 안 임의 가연물 점화(예전 RandomIgnite 복구)
    UFUNCTION(BlueprintCallable, Category = "Room|Fire")
    AFireActor* IgniteRandomCombustibleInRoom(bool bAllowElectric = false);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    // 현재 룸이 “알고 있는” 가연물들(권위는 컴포넌트)
    UPROPERTY() TSet<TWeakObjectPtr<UCombustibleComponent>> Combustibles;

    // 활성 Fire(상태용)
    UPROPERTY() TMap<FGuid, TObjectPtr<AFireActor>> ActiveFires;

    // 누적치
    float AccHeat = 0.f;
    float AccSmoke = 0.f;
    float AccOxygenSub = 0.f;
    float AccFireValue = 0.f;

private:
    const FFirePolicy& GetPolicy(ECombustibleType Type) const;
    FRoomInfluence BaseInfluence(ECombustibleType Type, float EffectiveIntensity) const;

    void ApplyAccumulators(float DeltaSeconds);
    void ResetAccumulators();
    void UpdateRoomState();

    // RoomBounds 내부 판정(스케일 포함)
    static bool IsInsideRoomBox(const UBoxComponent* Box, const FVector& WorldPos);

    void UpdateRoomGeometryFromBounds();
    void UpdateNeutralPlane(float DeltaSeconds);

    // Overlap 이벤트(선택)
    UFUNCTION() void OnRoomBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);
    UFUNCTION() void OnRoomEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};

