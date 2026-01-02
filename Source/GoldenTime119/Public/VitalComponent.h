// ============================ VitalComponent.h ============================
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RoomActor.h"
#include "VitalComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnVitals01Changed, float, Hp01, float, Temp01, float, O201);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GOLDENTIME119_API UVitalComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVitalComponent();

    UPROPERTY(BlueprintAssignable, Category = "Vitals")
    FOnVitals01Changed OnVitals01Changed;

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    void SetCurrentRoom(ARoomActor* InRoom);

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    float GetHp01() const { return Hp01; }

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    float GetTemp01() const { return Temp01; }

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    float GetO201() const { return O201; }

protected:
    virtual void BeginPlay() override;

    // 이 시그니처가 헤더에 반드시 있어야 합니다.
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    void StepVitals(float Dt);
    void BroadcastIfChanged(bool bForce);

private:
    UPROPERTY()
    TObjectPtr<ARoomActor> CurrentRoom = nullptr;

    // ===== Vital State (0..1) =====
    UPROPERTY(VisibleAnywhere, Category = "Vitals")
    float Hp01 = 1.f;

    UPROPERTY(VisibleAnywhere, Category = "Vitals")
    float Temp01 = 0.f;

    UPROPERTY(VisibleAnywhere, Category = "Vitals")
    float O201 = 1.f;

    float PrevHp01 = -1.f;
    float PrevTemp01 = -1.f;
    float PrevO201 = -1.f;

    // ===== Update cadence =====
    UPROPERTY(EditAnywhere, Category = "Vitals|Update")
    float UpdateInterval = 0.05f; // 20Hz

    float Acc = 0.f;

    // ===== Env->Vital tuning =====
    UPROPERTY(EditAnywhere, Category = "Vitals|Tuning")
    float HeatDangerRef = 600.f;

    UPROPERTY(EditAnywhere, Category = "Vitals|Tuning")
    float O2ResponseSpeed = 2.0f;

    UPROPERTY(EditAnywhere, Category = "Vitals|Tuning")
    float TempResponseSpeed = 1.2f;

    // ===== Thresholds =====
    UPROPERTY(EditAnywhere, Category = "Vitals|Threshold")
    float SmokeSafe = 0.35f;

    UPROPERTY(EditAnywhere, Category = "Vitals|Threshold")
    float HypoxiaSafe = 0.22f;

    UPROPERTY(EditAnywhere, Category = "Vitals|Threshold")
    float TempSafe01 = 0.55f;

    // ===== DPS =====
    UPROPERTY(EditAnywhere, Category = "Vitals|Damage")
    float SmokeDps = 0.10f;

    UPROPERTY(EditAnywhere, Category = "Vitals|Damage")
    float HypoxiaDps = 0.25f;

    UPROPERTY(EditAnywhere, Category = "Vitals|Damage")
    float HeatDps = 0.15f;

    // ===== Smoke breathing penalty =====
    UPROPERTY(EditAnywhere, Category = "Vitals|Tuning")
    float SmokeBreathMul = 0.6f;

    // ===== Proximity heat (cm) =====
    UPROPERTY(EditAnywhere, Category = "Vitals|Proximity")
    float ProxInnerCm = 80.f;

    UPROPERTY(EditAnywhere, Category = "Vitals|Proximity")
    float ProxOuterCm = 300.f;

    UPROPERTY(EditAnywhere, Category = "Vitals|Proximity")
    float ProxPow = 1.8f;

    // ===== Temp combine pow =====
    UPROPERTY(EditAnywhere, Category = "Vitals|Tuning")
    float EnvPow = 1.15f;

    UPROPERTY(EditAnywhere, Category = "Vitals|Tuning")
    float ProxEnvPow = 1.0f;

    // ===== Broadcast threshold =====
    UPROPERTY(EditAnywhere, Category = "Vitals|Update")
    float Epsilon = 0.002f;
};
