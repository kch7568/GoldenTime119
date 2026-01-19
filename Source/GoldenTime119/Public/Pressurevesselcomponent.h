// ============================ PressureVesselComponent.h ============================
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/AudioComponent.h"
#include "PressureVesselComponent.generated.h"

class UCombustibleComponent;
class AFireballActor;

UENUM(BlueprintType)
enum class EPressureVesselState : uint8
{
    Normal,
    Heating,
    Venting,
    Critical,
    Ruptured
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBLEVE, FVector, ExplosionLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVesselStateChanged, EPressureVesselState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSafetyValveVenting, float, VentStrength01);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GOLDENTIME119_API UPressureVesselComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPressureVesselComponent();

    // ===== 용기 물성 =====

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|Properties")
    float VesselCapacityLiters = 50.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|Properties")
    float LiquidFillLevel01 = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|Properties")
    float VesselStrength = 1.0f;

    // ===== 압력/온도 =====

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Vessel|Runtime")
    float InternalPressure = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|Pressure")
    float BasePressure = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|Pressure")
    float BurstPressure = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|Pressure")
    float BaseBurstPressure = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|Pressure")
    float CriticalPressure = 20.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Vessel|Runtime")
    float InternalTemperature = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|Temperature")
    float WallWeakeningTemp = 400.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Vessel|Runtime")
    float WallTemperature = 25.0f;

    // ===== 안전밸브 =====

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|SafetyValve")
    float SafetyValveActivationPressure = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|SafetyValve")
    float SafetyValveReleaseRate = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|SafetyValve")
    bool bSafetyValveFailed = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Vessel|Runtime")
    float SafetyValveVentStrength01 = 0.f;

    // ===== 열 입력 =====

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|Heat")
    float ExternalHeatMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|Heat")
    float HeatPerIgnitionProgress = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|Heat")
    float BurningHeatBonus = 50.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|Heat")
    float PressureRisePerDegree = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|Heat")
    float TempRisePerHeat = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|Heat")
    float LiquidCoolingFactor = 0.7f;

    // ===== BLEVE 결과 =====

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|BLEVE")
    float FireballRadiusMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|BLEVE")
    float BlastWaveIntensity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vessel|BLEVE")
    TSubclassOf<AFireballActor> FireballClass;

    // ===== 상태 =====

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Vessel|Runtime")
    EPressureVesselState VesselState = EPressureVesselState::Normal;

    // ===== 델리게이트 =====

    UPROPERTY(BlueprintAssignable, Category = "Vessel|Events")
    FOnBLEVE OnBLEVE;

    UPROPERTY(BlueprintAssignable, Category = "Vessel|Events")
    FOnVesselStateChanged OnVesselStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Vessel|Events")
    FOnSafetyValveVenting OnSafetyValveVenting;

    // ===== VFX/SFX 기본 자원 =====

    UPROPERTY(EditAnywhere, Category = "Vessel|VFX")
    TObjectPtr<UParticleSystem> SafetyValveVentTemplate;

    UPROPERTY(EditAnywhere, Category = "Vessel|Audio")
    TObjectPtr<USoundBase> SafetyValveSound;

    UPROPERTY(EditAnywhere, Category = "Vessel|Audio")
    TObjectPtr<USoundBase> CriticalWarningSound;

    // ===== Audio: Safety Valve Hiss Pitch Control =====

    // 누출음 최소 피치 (초기 상태)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|SafetyValve")
    float SafetyValvePitchMin = 0.0f;

    // 압력 비율 1.0에서의 최대 피치
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|SafetyValve")
    float SafetyValvePitchMax = 2.0f;

    // 이 비율 이후부터 피치가 급격히 증가 (폭발 직전 느낌)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|SafetyValve")
    float SafetyValveRapidIncreaseStartRatio = 0.8f;

    // 피치 보간 속도
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|SafetyValve")
    float SafetyValvePitchInterpSpeed = 6.0f;

    // ===== Audio: BLEVE Explosion OneShot =====

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Explosion")
    TObjectPtr<USoundBase> ExplosionSound;

    // ===== 함수 =====

    UFUNCTION(BlueprintCallable, Category = "Vessel")
    bool IsBLEVEPossible() const;

    UFUNCTION(BlueprintCallable, Category = "Vessel")
    float GetPressureRatio01() const;

    UFUNCTION(BlueprintCallable, Category = "Vessel")
    float GetTimeToRupture() const;

    UFUNCTION(BlueprintCallable, Category = "Vessel")
    void ForceRupture();

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UPROPERTY()
    TObjectPtr<UCombustibleComponent> LinkedCombustible;

    UPROPERTY()
    TObjectPtr<UParticleSystemComponent> SafetyValvePSC;

    UPROPERTY()
    TObjectPtr<UAudioComponent> SafetyValveAudioComp;

    UPROPERTY()
    TObjectPtr<UAudioComponent> CriticalWarningAudioComp;

    float AccumulatedHeat = 0.f;
    float HeatingStartTime = -1.f;
    float LastHeatInputTime = 0.f;

    void DetectHeatFromCombustible(float DeltaTime);
    void UpdatePressureAndTemperature(float DeltaTime);
    void UpdateSafetyValve(float DeltaTime);
    void UpdateVesselState();
    void CheckBLEVECondition();
    void ExecuteBLEVE();
    void SetVesselState(EPressureVesselState NewState);
    void EnsureComponentsCreated();
};
