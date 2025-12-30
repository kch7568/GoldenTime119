// ============================ FireActor.h ============================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombustibleType.h"
#include "FireRuntimeTuning.h" // 분리 헤더
#include "FireActor.generated.h"

class ARoomActor;
class USceneComponent;
class UCombustibleComponent;
class UParticleSystem;
class UParticleSystemComponent;

UCLASS()
class GOLDENTIME119_API AFireActor : public AActor
{
    GENERATED_BODY()

public:
    AFireActor();

    // ===== Components =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fire|Component")
    TObjectPtr<USceneComponent> Root = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fire|VFX")
    TObjectPtr<UParticleSystemComponent> FirePsc = nullptr;

    // ===== Runtime Data =====
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Data")
    FGuid FireID;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Data")
    TObjectPtr<ARoomActor> LinkedRoom = nullptr;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Data")
    TObjectPtr<UCombustibleComponent> LinkedCombustible = nullptr;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Data")
    ECombustibleType CombustibleType = ECombustibleType::Normal;

    // ===== Intensity =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire|Intensity")
    float BaseIntensity = 1.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Intensity")
    float EffectiveIntensity = 1.0f;

    // ===== Spread / Timing =====
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Spread")
    float CurrentSpreadRadius = 350.f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Timing")
    float SpreadInterval = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire|Timing")
    float InfluenceInterval = 0.5f;

    // ===== VFX =====
    UPROPERTY(EditAnywhere, Category = "Fire|VFX")
    TObjectPtr<UParticleSystem> FireTemplate = nullptr;

    UPROPERTY(EditAnywhere, Category = "Fire|VFX")
    float IgniteRampSeconds = 1.25f;

    UPROPERTY(VisibleAnywhere, Category = "Fire|VFX")
    float SpawnAge = 0.f;

    UPROPERTY(VisibleInstanceOnly, Category = "Fire|VFX")
    float Strength01 = 1.f;

    // (옵션) BP가 참고할 수 있는 최종 스케일
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Backdraft")
    float BackdraftScale01 = 1.f;

    // ===== Spawn Params (ExposeOnSpawn) =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = "Fire|Spawn")
    TObjectPtr<ARoomActor> SpawnRoom = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = "Fire|Spawn")
    ECombustibleType SpawnType = ECombustibleType::Normal;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Data")
    TWeakObjectPtr<AActor> IgnitedTarget = nullptr;

public:
    void InitFire(ARoomActor* InRoom, ECombustibleType InType);

    UFUNCTION(BlueprintCallable, Category = "Fire")
    FVector GetSpreadOrigin() const;

    UFUNCTION(BlueprintCallable, Category = "Fire")
    void Extinguish();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    float InfluenceAcc = 0.f;
    float SpreadAcc = 0.f;

    bool bInitialized = false;
    bool bIsActive = false;

private:
    void UpdateRuntimeFromRoom(float DeltaSeconds);
    void SubmitInfluenceToRoom();
    void ApplyToOwnerCombustible();
    void SpreadPressureToNeighbors();

    bool ShouldExtinguish() const;
    void UpdateVfx(float DeltaSeconds);

    // ===== Helpers =====
    float GetCombustionScaleFromRoom() const; // Backdraft ready 등
};
