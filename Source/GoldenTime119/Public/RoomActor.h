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

    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HeatAdd = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SmokeAdd = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float OxygenSub = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FireValueAdd = 0.0f;
};

USTRUCT(BlueprintType)
struct FFireFuelPolicy
{
    GENERATED_BODY()

    // ===== Fuel =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel") float InitialFuel = 10.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel") float ConsumePerSecond_Min = 0.6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel") float ConsumePerSecond_Max = 1.6f;

    // scale = clamp(Fuel / InitialFuel, 0~1) ^ ScalePow
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel") float ScalePow = 1.0f;

    // 연소 “강도”가 Consume/Spread/Influence에 들어갈 때의 커브
    // Intensity01 = clamp(EffectiveIntensity / IntensityRef, 0~1) ^ IntensityPow
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float IntensityRef = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float IntensityPow = 1.0f;

    // ===== Spread =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread") float SpreadRadius_Min = 250.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread") float SpreadRadius_Max = 900.0f;

    // “확산 주기”: 불이 강할수록 더 자주 확산하도록 Min/Max로 제어
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread") float SpreadInterval_Min = 0.45f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread") float SpreadInterval_Max = 1.25f;

    // ===== Influence =====
    // 룸 값 영향 배수 (타입별로 조절)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Influence") float HeatMul = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Influence") float SmokeMul = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Influence") float OxygenMul = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Influence") float FireValueMul = 1.0f;
};

// Fire가 룸에서 “현재 튜닝 결과”를 받아갈 때 쓰는 구조체
USTRUCT(BlueprintType)
struct FFireRuntimeTuning
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float FuelRatio01 = 1.0f;
    UPROPERTY(BlueprintReadOnly) float Intensity01 = 1.0f;

    UPROPERTY(BlueprintReadOnly) float SpreadRadius = 350.0f;
    UPROPERTY(BlueprintReadOnly) float SpreadInterval = 1.0f;

    UPROPERTY(BlueprintReadOnly) float ConsumePerSecond = 1.0f;

    // Influence를 Room에 Submit할 때 곱해줄 스케일
    UPROPERTY(BlueprintReadOnly) float InfluenceScale = 1.0f;
};

USTRUCT(BlueprintType)
struct FPendingIgnition
{
    GENERATED_BODY()

    UPROPERTY() TWeakObjectPtr<AActor> Target = nullptr;
    UPROPERTY() float IgniteAtTime = 0.f;                 // 월드 시간(초)
    UPROPERTY() ECombustibleType Type = ECombustibleType::Normal;

    // 디버그용(누가 예약했는지 추적)
    UPROPERTY() FGuid SourceFireId;
    UPROPERTY() FVector SourceOrigin = FVector::ZeroVector;
};


UCLASS()
class GOLDENTIME119_API ARoomActor : public AActor
{
    GENERATED_BODY()

public:
    ARoomActor();

    // ===== Room 환경 수치 =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Env") float Heat = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Env") float Smoke = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Env") float Oxygen = 1.0f;   // 0~1
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Env") float FireValue = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Room|State")
    ERoomState State = ERoomState::Idle;

    // 임계값(예시)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Threshold") float RiskHeatThreshold = 30.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Threshold") float FireHeatThreshold = 60.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Threshold") float MinOxygenToSustain = 0.15f;

    // 인접 Room 그래프
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Room|Link")
    TArray<TObjectPtr<ARoomActor>> AdjacentRooms;

    // 룸 경계
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Volume")
    TObjectPtr<UBoxComponent> RoomBounds;

    // 스폰할 Fire 클래스
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Fire")
    TSubclassOf<AFireActor> FireClass;


    // ===== Fuel/Spread/Influence 정책(타입별) =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Policy") FFireFuelPolicy PolicyNormal;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Policy") FFireFuelPolicy PolicyOil;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Policy") FFireFuelPolicy PolicyElectric;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Policy") FFireFuelPolicy PolicyExplosive;

    // ===== Fire 등록/해제 =====
    void RegisterFire(AFireActor* Fire);
    void UnregisterFire(const FGuid& FireID);

    // Fire -> Room 영향 요청
    void SubmitInfluence(const FGuid& FireID, ECombustibleType Type, float EffectiveIntensity);

    // Fire 유지 가능 여부(산소 + 연료)
    bool ShouldExtinguishFire(const FGuid& FireID) const;

    // Fire 강도 스케일(연료 기반)
    float GetIntensityScale(const FGuid& FireID) const;

    // Fire가 매 프레임/주기마다 현재 튜닝값 요청
    bool GetRuntimeTuning(const FGuid& FireID, ECombustibleType Type, float EffectiveIntensity, FFireRuntimeTuning& Out) const;

    // Fire 확산 요청 시
    bool TrySpawnFireFromSpread(AFireActor* SourceFire);

    // 최초 점화
    UFUNCTION(BlueprintCallable, Category = "Room|Fire")
    AFireActor* IgniteTarget(AActor* TargetActor);

    // ===== BP Helper =====
    UFUNCTION(BlueprintCallable, Category = "Room|Fire")
    void GetCombustibleActorsInRoom(UPARAM(ref) TArray<AActor*>& OutActors, bool bExcludeBurning = true) const;

    UFUNCTION(BlueprintCallable, Category = "Room|Fire")
    AFireActor* IgniteRandomCombustibleInRoom(bool bAllowElectric = false);

    // ===== Room 이벤트 =====
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRoomFireEvent, AFireActor*, Fire);

    UPROPERTY(BlueprintAssignable, Category = "Room|Event") FRoomFireEvent OnFireStarted;
    UPROPERTY(BlueprintAssignable, Category = "Room|Event") FRoomFireEvent OnFireExtinguished;
    UPROPERTY(BlueprintAssignable, Category = "Room|Event") FRoomFireEvent OnFireSpread;

    UPROPERTY(EditAnywhere, Category = "Room|SpreadDelay")
    float SpreadDelayNearSec = 5.0f;

    UPROPERTY(EditAnywhere, Category = "Room|SpreadDelay")
    float SpreadDelayFarSec = 40.0f;

    UPROPERTY(EditAnywhere, Category = "Room|SpreadDelay")
    float SpreadDelayJitterSec = 1.5f; // 동시 점화 방지용 약간 랜덤

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    // 활성 Fire
    UPROPERTY() TMap<FGuid, TObjectPtr<AFireActor>> ActiveFires;

    // 불붙은 대상 추적 (중복 방지)
    UPROPERTY() TMap<TWeakObjectPtr<AActor>, FGuid> BurningTargets;

    // FireID별 연료 상태
    UPROPERTY() TMap<FGuid, float> FireFuelCurrent;
    UPROPERTY() TMap<FGuid, float> FireFuelInitial;
    UPROPERTY() TMap<FGuid, ECombustibleType> FireTypeById;

    // 프레임 누적 영향치
    float AccHeat = 0.0f;
    float AccSmoke = 0.0f;
    float AccOxygenSub = 0.0f;
    float AccFireValue = 0.0f;

    void ResetAccumulators();
    void ApplyAccumulators(float DeltaSeconds);
    void UpdateRoomState();

    // 타입별 영향(기본값)
    FRoomInfluence CalcInfluence(ECombustibleType Type, float EffectiveIntensity) const;

    // 타입별 정책
    const FFireFuelPolicy& GetFuelPolicy(ECombustibleType Type) const;

    bool IsActorAlreadyOnFire(AActor* Target) const;

    // 오버랩이 아니라 수학적으로 “RoomBounds 내부” 판정
    static bool IsInsideRoomBox(const UBoxComponent* Box, const FVector& WorldPos);

    // 확산 후보 탐색/선정
    bool CollectCandidates(const FVector& Origin, float Radius, ARoomActor* InRoom, TArray<AActor*>& OutActors) const;
    AActor* PickNearestCandidate(const FVector& Origin, const TArray<AActor*>& Candidates) const;

    // 타입 기반 확산 분기
    bool Spread_Normal(AFireActor* SourceFire);
    bool Spread_Oil(AFireActor* SourceFire);
    bool Spread_Electric(AFireActor* SourceFire);
    bool Spread_Explosive(AFireActor* SourceFire);

    AFireActor* SpawnFireInternal(ARoomActor* TargetRoom, AActor* TargetActor, ECombustibleType Type);

    // ===== Pending Ignitions =====
    UPROPERTY()
    TMap<TWeakObjectPtr<AActor>, FPendingIgnition> PendingIgnitions;

    // 여러 불이 같은 타겟을 동시에 예약하는 폭주 방지
    UPROPERTY()
    TSet<TWeakObjectPtr<AActor>> ReservedTargets;

    // Tick에서 예약 처리
    void ProcessPendingIgnitions();

    // Spread에서 “즉시 스폰” 대신 “예약”하기
    bool ScheduleIgnitionFromSpread(
        AFireActor* SourceFire,
        ARoomActor* TargetRoom,
        AActor* TargetActor,
        float Radius
    );
};
