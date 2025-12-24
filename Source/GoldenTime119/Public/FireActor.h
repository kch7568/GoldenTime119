// ============================ FireActor.h ============================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombustibleType.h"
#include "RoomActor.h" // FFireRuntimeTuning
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Components/PointLightComponent.h"
#include "FireActor.generated.h"

class ARoomActor;
class USceneComponent;
class UCombustibleComponent;
class UPointLightComponent;

UCLASS()
class GOLDENTIME119_API AFireActor : public AActor
{
    GENERATED_BODY()

public:
    AFireActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fire|Component")
    TObjectPtr<USceneComponent> Root = nullptr;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Data")
    FGuid FireID;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Data")
    TObjectPtr<ARoomActor> LinkedRoom = nullptr;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Data")
    TObjectPtr<UCombustibleComponent> LinkedCombustible = nullptr;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Data")
    ECombustibleType CombustibleType = ECombustibleType::Normal;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire|Intensity")
    float BaseIntensity = 1.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Intensity")
    float EffectiveIntensity = 1.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Spread")
    float CurrentSpreadRadius = 350.f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Timing")
    float SpreadInterval = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire|Timing")
    float InfluenceInterval = 0.5f;

    UPROPERTY(EditAnywhere, Category = "VFX")
    float IgniteRampSeconds = 1.25f;

    UPROPERTY(VisibleAnywhere, Category = "VFX")
    float SpawnAge = 0.f;              // 스폰 후 경과

    UPROPERTY(VisibleAnywhere, Category = "VFX")
    TObjectPtr<UParticleSystemComponent> FirePsc = nullptr;

    UPROPERTY(EditAnywhere, Category = "VFX")
    TObjectPtr<UParticleSystem> FireTemplate = nullptr;

    UPROPERTY(VisibleInstanceOnly, Category = "VFX")
    float Strength01 = 1.f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|VFX")
    float FireVfxScale01 = 0.f;

    void UpdateVfx(float DeltaSeconds);

    // 스폰 파라미터(방어)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = "Fire|Spawn")
    TObjectPtr<ARoomActor> SpawnRoom = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = "Fire|Spawn")
    ECombustibleType SpawnType = ECombustibleType::Normal;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Data")
    TWeakObjectPtr<AActor> IgnitedTarget = nullptr;

    void InitFire(ARoomActor* InRoom, ECombustibleType InType);

    UFUNCTION(BlueprintCallable, Category = "Fire")
    FVector GetSpreadOrigin() const;

    void Extinguish();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    float InfluenceAcc = 0.f;
    float SpreadAcc = 0.f;

    bool bInitialized = false;
    bool bIsActive = false;

    static constexpr float WaterDecayPerSec = 1.25f; // 물 입력은 금방 사라지게(취향)
    float PendingWater01 = 0.f; // <- 클래스 멤버로 넣으세요 (private)

private:
    void UpdateRuntimeFromRoom();
    void SubmitInfluenceToRoom();
    void ApplyToOwnerCombustible();   // 연료 소비 등
    void SpreadPressureToNeighbors(); // 주변 가연물에 “압력”만 전달

    bool ShouldExtinguish() const;

};
