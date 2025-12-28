#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Particles/ParticleSystemComponent.h"
#include "FireHose.generated.h"

UENUM(BlueprintType)
enum class EHoseMode : uint8
{
    Focused  UMETA(DisplayName = "Focused"),
    Spray    UMETA(DisplayName = "Spray")
};

UCLASS()
class GOLDENTIME119_API AFireHose : public AActor
{
    GENERATED_BODY()

public:
    AFireHose();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hose")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hose")
    TObjectPtr<UParticleSystemComponent> WaterPsc;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hose")
    TObjectPtr<USceneComponent> NozzlePoint;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|VFX")
    TObjectPtr<UParticleSystem> FocusedWaterTemplate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|VFX")
    TObjectPtr<UParticleSystem> SprayWaterTemplate;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hose|State")
    EHoseMode CurrentMode = EHoseMode::Focused;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hose|State")
    bool bIsFiring = false;

    // === Focused 모드 설정 ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Focused")
    float FocusedRange = 800.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Focused")
    float FocusedRadius = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Focused")
    float FocusedWaterAmount = 3.0f;

    // === Spray 모드 설정 ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Spray")
    float SprayRange = 500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Spray")
    float SprayRadius = 250.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Spray")
    float SprayWaterAmount = 2.0f;

    // === 곡선(포물선) 설정 ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Curve")
    float WaterGravity = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Curve")
    int32 TraceSegments = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Curve")
    bool bDebugDraw = true;

    UFUNCTION(BlueprintCallable, Category = "Hose")
    void SetMode(EHoseMode NewMode);

    UFUNCTION(BlueprintCallable, Category = "Hose")
    void StartFiring();

    UFUNCTION(BlueprintCallable, Category = "Hose")
    void StopFiring();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    void UpdateWaterVFX();
    void SetupInputBindings();
    void CalculateWaterPath(TArray<FVector>& OutPoints);
    void TraceAlongWaterPath(float DeltaSeconds);

    void OnFocusedModePressed();
    void OnSprayModePressed();
    void OnFirePressed();
    void OnFireReleased();
};