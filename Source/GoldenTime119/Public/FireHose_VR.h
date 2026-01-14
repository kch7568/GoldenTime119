// FireHose_VR.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "GrabInteractable.h"
#include "FireHose_VR.generated.h"

UENUM(BlueprintType)
enum class EHoseMode_VR : uint8
{
    Focused  UMETA(DisplayName = "Focused"),
    Spray    UMETA(DisplayName = "Spray")
};

UCLASS()
class GOLDENTIME119_API AFireHose_VR : public AActor, public IGrabInteractable
{
    GENERATED_BODY()

public:
    AFireHose_VR();

    // ============================================================
    // Mesh
    // ============================================================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hose|Mesh")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hose|Mesh")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hose|Mesh")
    TObjectPtr<USceneComponent> BarrelPivot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hose|Mesh")
    TObjectPtr<UStaticMeshComponent> BarrelMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hose|Mesh")
    TObjectPtr<USceneComponent> LeverPivot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hose|Mesh")
    TObjectPtr<UStaticMeshComponent> LeverMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hose|Mesh")
    TObjectPtr<USceneComponent> WaterSpawnPoint;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hose|VFX")
    TObjectPtr<UParticleSystemComponent> WaterPsc;

    // ============================================================
    // VFX Templates
    // ============================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|VFX")
    TObjectPtr<UParticleSystem> FocusedWaterTemplate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|VFX")
    TObjectPtr<UParticleSystem> SprayWaterTemplate;

    // ============================================================
    // Audio (Loop + Impact OneShot)
    // ============================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Audio")
    TObjectPtr<USoundBase> Snd_HoseSprayLoop; // 1_Hose_Spray_Loop

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Audio")
    TObjectPtr<USoundBase> Snd_WaterHitFloorHeavy_OneShot; // 2_Water_Hit_Floor_Heavy_OneShot

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hose|Audio")
    TObjectPtr<UAudioComponent> AC_HoseSpray;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Audio")
    float HoseVolumeMax = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Audio")
    float AudioOnThreshold = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Audio")
    float HoseFadeInSec = 0.20f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Audio")
    float HoseFadeOutSec = 0.60f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Audio|Filter")
    bool bEnableModeFilter = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Audio|Filter")
    float Focused_LPF_Hz = 18000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Audio|Filter")
    float Spray_LPF_Hz = 9000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Audio|Impact")
    float ImpactMinIntervalSec = 2.0f; // ø‰√ª: 2√ 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Audio|Impact")
    float ImpactVolumeMax = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Audio|Impact")
    float ImpactPitchMin = 0.95f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Audio|Impact")
    float ImpactPitchMax = 1.05f;

    // ============================================================
    // State
    // ============================================================

    UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Hose|State")
    EHoseMode_VR CurrentMode = EHoseMode_VR::Focused;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Hose|State")
    bool bIsGrabbedBody = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Hose|State")
    bool bIsGrabbedLever = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Hose|State")
    bool bIsGrabbedBarrel = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hose|State")
    float PressureAlpha = 0.f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Hose|State")
    float TargetPressure = 0.f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hose|State")
    float LeverPullAmount = 0.f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Hose|State")
    float BarrelRotation = 0.f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Hose|State")
    float TargetBarrelRotation = 0.f;

    // ============================================================
    // Tunables
    // ============================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Focused")
    float FocusedRange = 1200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Focused")
    float FocusedRadius = 80.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Focused")
    float FocusedWaterAmount = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Spray")
    float SprayRange = 600.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Spray")
    float SprayRadius = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Spray")
    float SprayWaterAmount = 2.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Curve")
    float WaterGravity = 250.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Curve")
    int32 TraceSegments = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Pressure")
    float PressureIncreaseSpeed = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Pressure")
    float PressureDecreaseSpeed = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Lever")
    float LeverMaxRotation = 70.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Barrel")
    float BarrelRotationSpeed = 5.0f;

    // ============================================================
    // VR refs
    // ============================================================

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hose|VR")
    FVector LeverGrabStartLocation;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hose|VR")
    TObjectPtr<USceneComponent> GrabbingLeverController;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hose|VR")
    TObjectPtr<USceneComponent> GrabbingBarrelController;

    // ============================================================
    // IGrabInteractable
    // ============================================================

    virtual void OnGrabbed_Implementation(USceneComponent* GrabbingController, bool bIsLeftHand) override;
    virtual void OnReleased_Implementation(USceneComponent* GrabbingController, bool bIsLeftHand) override;
    virtual bool CanBeGrabbed_Implementation() const override;
    virtual FTransform GetGrabTransform_Implementation(bool bIsLeftHand) const override;

    // ============================================================
    // VR API
    // ============================================================

    UFUNCTION(BlueprintCallable, Category = "Hose|VR")
    void OnBodyGrabbed();

    UFUNCTION(BlueprintCallable, Category = "Hose|VR")
    void OnBodyReleased();

    UFUNCTION(BlueprintCallable, Category = "Hose|VR")
    void OnLeverGrabbed(USceneComponent* GrabbingController);

    UFUNCTION(BlueprintCallable, Category = "Hose|VR")
    void OnLeverReleased();

    UFUNCTION(BlueprintCallable, Category = "Hose|VR")
    void OnBarrelGrabbed(USceneComponent* GrabbingController);

    UFUNCTION(BlueprintCallable, Category = "Hose|VR")
    void OnBarrelReleased();

    UFUNCTION(BlueprintCallable, Category = "Hose|VR")
    void SetLeverPull(float PullAmount);

    UFUNCTION(BlueprintCallable, Category = "Hose|VR")
    void SetBarrelRotation(float RotationDegrees);

    // ============================================================
    // Control
    // ============================================================

    UFUNCTION(BlueprintCallable, Category = "Hose|Control")
    void SetPressure(float NewPressure);

    UFUNCTION(BlueprintCallable, Category = "Hose|Control")
    void SetMode(EHoseMode_VR NewMode);

    UFUNCTION(BlueprintCallable, Category = "Hose|Control")
    void ToggleMode();

    UFUNCTION(BlueprintCallable, Category = "Hose|Control")
    void StartFiring();

    UFUNCTION(BlueprintCallable, Category = "Hose|Control")
    void StopFiring();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    void SetupKeyboardTest();
    void UpdatePressure(float DeltaSeconds);
    void UpdateBarrelRotation(float DeltaSeconds);
    void UpdateLeverVisual();
    void UpdateBarrelVisual();
    void UpdateWaterVFX();
    void UpdateMode();
    void UpdateVRLeverFromController();
    void UpdateVRBarrelFromController();
    void CalculateWaterPath(TArray<FVector>& OutPoints);
    void TraceAlongWaterPath(float DeltaSeconds);

    FVector GetNozzleLocation() const;
    FVector GetNozzleForward() const;

    // Audio
    void UpdateAudio(float DeltaSeconds);
    void ApplyModeFilter();
    void TryPlayImpactOneShot(const TArray<FVector>& WaterPath);

    bool bInputBound = false;
    bool bTestFiring = false;
    bool bWaterVFXActive = false;

    FVector PreviousLocalBarrelHandPos;
    float CurrentGrabRotationSum = 0.f;
    bool bIsBarrelFirstTick = false;

    UPROPERTY(EditAnywhere, Category = "Hose|Barrel")
    float RotationThresholdPerGrab = 90.f;

    bool bModeSwappedInThisGrab = false;

    UPROPERTY(EditAnywhere, Category = "Hose|Barrel")
    float BarrelSensitivity = 2.5f;

    float RotationAtGrabStart = 0.f;

    float LastImpactPlayTime = -1000.f;
};
