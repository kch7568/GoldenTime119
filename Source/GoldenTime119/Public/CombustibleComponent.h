// ============================ CombustibleComponent.h (FIXED) ============================
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombustibleType.h"
#include "CombustibleComponent.generated.h"

class ARoomActor;
class AFireActor;

USTRUCT(BlueprintType)
struct FCombustibleIgnitionParams
{
    GENERATED_BODY()

    // 점화 진행도(0..1)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ignition") float IgnitionProgress01 = 0.f;

    // 외부(불)로부터 들어오는 “압력/열”을 진행도로 변환하는 속도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ignition") float IgnitionSpeed = 0.55f;

    // 압력 없으면 감쇠(진압/냉각/거리)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ignition") float IgnitionDecayPerSec = 0.08f;

    // 임계치(1.0이면 단순, 낮추면 더 잘 붙음)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ignition") float IgniteThreshold = 1.0f;

    // “가연성” 계수 (목재/천/플라스틱 등)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ignition") float Flammability = 1.0f;
};

USTRUCT(BlueprintType)
struct FCombustibleFuelParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel") float FuelInitial = 12.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel") float FuelCurrent = 12.f;

    // Fire가 Consume할 때 “기본 배수”(타입별 정책 + 이 값)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel") float FuelConsumeMul = 1.0f;

    // USTRUCT 안에서는 UFUNCTION 불가 -> C++ 인라인 함수로만 제공
    FORCEINLINE float FuelRatio01_Cpp() const
    {
        return (FuelInitial > 0.f) ? FMath::Clamp(FuelCurrent / FuelInitial, 0.f, 1.f) : 0.f;
    }
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GOLDENTIME119_API UCombustibleComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCombustibleComponent();

    // 재질 타입
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combustible")
    ECombustibleType CombustibleType = ECombustibleType::Normal;

    // 전기 조건
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combustible|Electric")
    bool bElectricIgnitionTriggered = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combustible|Electric")
    FName ElectricNetId = NAME_None;

    // 연료/점화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combustible|Ignition")
    FCombustibleIgnitionParams Ignition;

    UFUNCTION(BlueprintCallable, Category = "Combustible|Fuel")
    void EnsureFuelInitialized();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combustible|Fuel")
    FCombustibleFuelParams Fuel;

    // 현재 화재
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combustible|Runtime")
    bool bIsBurning = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combustible|Runtime")
    TObjectPtr<AFireActor> ActiveFire = nullptr;

    // 룸 링크(룸이 Set)
    void SetOwningRoom(ARoomActor* InRoom);
    ARoomActor* GetOwningRoom() const { return OwningRoom.Get(); }

    UFUNCTION(BlueprintCallable, Category = "Combustible")
    bool IsBurning() const { return bIsBurning && ActiveFire != nullptr; }

    // Blueprint에서도 연료비율을 쓰고 싶으면 UCLASS에 프록시로 제공
    UFUNCTION(BlueprintCallable, Category = "Combustible|Fuel")
    float GetFuelRatio01() const { return Fuel.FuelRatio01_Cpp(); }

    // Fire가 호출: 점화 압력(거리/강도 반영된 값)
    void AddIgnitionPressure(const FGuid& SourceFireId, float Pressure);

    // Fire가 호출: 열 전달(선택) -> 점화 진행도에 가산
    void AddHeat(float HeatDelta);

    // Fire가 호출: 연료 소비(권위는 가연물)
    void ConsumeFuel(float ConsumeAmount);

    // Tick에서 룸/압력 기반 진행도 업데이트
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "Combustible|Debug")
    AFireActor* ForceIgnite(bool bAllowElectric = true);

protected:
    virtual void BeginPlay() override;

private:
    // 룸
    UPROPERTY() TWeakObjectPtr<ARoomActor> OwningRoom = nullptr;

    // 이번 프레임 누적 입력
    float PendingPressure = 0.f;
    float PendingHeat = 0.f;

    // 디버그/추적
    float LastInputTime = 0.f;

private:
    bool CanIgniteNow() const;
    void UpdateIgnitionProgress(float DeltaTime);
    void TryIgnite();

    void OnIgnited();
    void OnExtinguished();
};
