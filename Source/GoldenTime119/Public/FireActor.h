// ============================ FireActor.h ============================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombustibleType.h"
#include "FireRuntimeTuning.h"
#include "FireActor.generated.h"

class ARoomActor;
class USceneComponent;
class UCombustibleComponent;
class UParticleSystem;
class UParticleSystemComponent;
class UAudioComponent;
class USoundBase;

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

    // ===== Audio (Fire Loop) =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fire|Audio")
    TObjectPtr<UAudioComponent> FireLoopAudio = nullptr;

    // 2_Heavy_Fire_Loop
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire|Audio")
    TObjectPtr<USoundBase> HeavyFireLoopSound = nullptr;

    // 3_Fire_Extinguish (OneShot)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire|Audio")
    TObjectPtr<USoundBase> FireExtinguishOneShotSound = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire|Audio")
    float FireLoopFadeIn = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire|Audio")
    float FireLoopFadeOut = 0.25f;

    // 원샷이 너무 연속으로 나가는 것을 방지(동일 FireActor에서 1회만 재생)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire|Audio")
    bool bPlayExtinguishOneShot = true;

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

    bool bPlayedExtinguishOneShot = false;

private:
    void UpdateRuntimeFromRoom(float DeltaSeconds);
    void SubmitInfluenceToRoom();
    void ApplyToOwnerCombustible();
    void SpreadPressureToNeighbors();

    bool ShouldExtinguish() const;
    void UpdateVfx(float DeltaSeconds);
    void UpdateAudio(float DeltaSeconds);

    float GetCombustionScaleFromRoom() const;
};
