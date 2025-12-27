// FireHose.h
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

    // === 컴포넌트 ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hose")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hose")
    TObjectPtr<UParticleSystemComponent> WaterPsc;

    // === VFX 템플릿 ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|VFX")
    TObjectPtr<UParticleSystem> FocusedWaterTemplate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|VFX")
    TObjectPtr<UParticleSystem> SprayWaterTemplate;

    // === 모드 ===
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hose|State")
    EHoseMode CurrentMode = EHoseMode::Focused;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hose|State")
    bool bIsFiring = false;

    // === 설정 ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Settings")
    float FocusedRange = 800.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Settings")
    float FocusedRadius = 50.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Settings")
    float FocusedWaterAmount = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Settings")
    float SprayRange = 500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Settings")
    float SprayRadius = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Settings")
    float SprayWaterAmount = 0.3f;

    // === 함수 ===
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
    void ApplyWaterToTargets(float DeltaSeconds);
    void SetupInputBindings();

    // 입력 핸들러
    void OnFocusedModePressed();
    void OnSprayModePressed();
    void OnFirePressed();
    void OnFireReleased();
};