#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombustibleType.h"
#include "RoomActor.h"          // FFireRuntimeTuning 사용
#include "FireActor.generated.h"

class ARoomActor;
class USceneComponent;

UCLASS()
class GOLDENTIME119_API AFireActor : public AActor
{
    GENERATED_BODY()

public:
    AFireActor();

    // ===== Root (중요) =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fire|Component")
    TObjectPtr<USceneComponent> Root = nullptr;

    // ===== 최소 데이터 =====
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Data")
    FGuid FireID;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Data")
    TObjectPtr<ARoomActor> LinkedRoom = nullptr;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Data")
    ECombustibleType CombustibleType = ECombustibleType::Normal;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Data")
    bool bIsActive = false;

    // ===== 방어적 스폰 파라미터 =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = "Fire|Spawn")
    TObjectPtr<ARoomActor> SpawnRoom = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = "Fire|Spawn")
    ECombustibleType SpawnType = ECombustibleType::Normal;

    // Fire “기본 세기”
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire|Intensity")
    float BaseIntensity = 1.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Intensity")
    float EffectiveIntensity = 1.0f;

    // ===== 동적(런타임) 확산 파라미터 =====
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Spread")
    float CurrentSpreadRadius = 350.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Timing")
    float CurrentSpreadInterval = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire|Spread")
    FName ElectricNetIdHint = NAME_None;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Type")
    bool bExplosionTriggered = false;

    UFUNCTION(BlueprintCallable, Category = "Fire")
    void InitFire(ARoomActor* InRoom, ECombustibleType InType);

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Data")
    TWeakObjectPtr<AActor> IgnitedTarget = nullptr;

    void Extinguish();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

public:
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFireEvent, AFireActor*, Fire);

    // 확산/거리 계산에 사용할 Origin (TargetCenter 우선)
    UFUNCTION(BlueprintCallable, Category = "Fire")
    FVector GetSpreadOrigin() const;

    UPROPERTY(BlueprintAssignable, Category = "Fire|Event") FFireEvent OnFireStarted;
    UPROPERTY(BlueprintAssignable, Category = "Fire|Event") FFireEvent OnFireExtinguished;
    UPROPERTY(BlueprintAssignable, Category = "Fire|Event") FFireEvent OnFireSpread;

private:
    float InfluenceAcc = 0.0f;
    float SpreadAcc = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Fire|Timing")
    float InfluenceInterval = 0.5f;

    UPROPERTY(EditAnywhere, Category = "Fire|Timing")
    float SpreadInterval = 1.0f;

    bool ShouldExtinguish() const;
    void UpdateIntensityFromRoom();
    void UpdateRuntimeTuningFromRoom();

    void RequestInfluence();
    void RequestSpread();
    void TriggerExplosiveOnce();

private:
    bool bStartedEventFired = false;
    bool bExtinguishedEventFired = false;
    bool bInitialized = false;
};
