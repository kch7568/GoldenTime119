// FireHose_VR.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "GrabInteractable.h"

#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

#include "FireHose_VR.generated.h"

UENUM(BlueprintType)
enum class EHoseMode_VR : uint8
{
	Focused UMETA(DisplayName = "Focused"),
	Spray   UMETA(DisplayName = "Spray")
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

	// ============================================================
	// Niagara (Water) - ONLY 3 USER PARAMS
	// ============================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Niagara")
	TObjectPtr<UNiagaraSystem> NS_Water;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hose|Niagara")
	TObjectPtr<UNiagaraComponent> NC_Water;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Niagara")
	FName NiagaraParam_SpreadAngleDeg = TEXT("User.SpreadAngleDeg");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Niagara")
	FName NiagaraParam_ParticleSpawnRate = TEXT("User.ParticleSpawnRate");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Niagara")
	FName NiagaraParam_InitialSpeed = TEXT("User.InitialSpeed");

	// ============================================================
	// Audio
	// ============================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Audio")
	TObjectPtr<USoundBase> Snd_HoseSprayLoop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Audio")
	TObjectPtr<USoundBase> Snd_WaterHitFloorHeavy_OneShot;

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
	float ImpactMinIntervalSec = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Audio|Impact")
	float ImpactVolumeMax = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Audio|Impact")
	float ImpactPitchMin = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Audio|Impact")
	float ImpactPitchMax = 1.05f;

	// ============================================================
	// Haptics
	// ============================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Haptics")
	bool bEnableHaptics = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Haptics")
	float SprayHapticAmpMax = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Haptics")
	float SprayHapticFreqMax = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Haptics")
	float NozzleTurnPulseAmp = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Haptics")
	float NozzleTurnPulseFreq = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Haptics")
	float NozzleTurnPulseMinInterval = 0.05f;

	// ============================================================
	// State
	// ============================================================

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hose|State")
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

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hose|State")
	float PatternAlpha = 0.f;

	// ============================================================
	// Tunables (Gameplay + Niagara Drive)
	// ============================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Pattern")
	float FocusedThreshold = 0.35f;

	// Gameplay
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Gameplay")
	float FocusedRange = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Gameplay")
	float SprayRange = 650.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Gameplay")
	float FocusedRadius = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Gameplay")
	float SprayRadius = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Gameplay")
	float FocusedWaterAmount = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Gameplay")
	float SprayWaterAmount = 2.5f;

	// Niagara drive (3 params only)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|NiagaraDrive")
	float FocusedSpreadAngleDeg = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|NiagaraDrive")
	float SpraySpreadAngleDeg = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|NiagaraDrive")
	float FocusedInitialSpeed = 5200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|NiagaraDrive")
	float SprayInitialSpeed = 3600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|NiagaraDrive")
	float FocusedSpawnRateMax = 2600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|NiagaraDrive")
	float SpraySpawnRateMax = 4200.f;

	// Trace
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hose|Trace")
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

	// Niagara drive (3 params only)
	void UpdateNiagara(float DeltaSeconds);
	void SetNiagaraParams3();

	// Mode display + audio filter
	void UpdateModeFromPattern();
	void ApplyModeFilter();

	// VR
	void UpdateVRLeverFromController();
	void UpdateVRBarrelFromController();

	// Gameplay trace
	void CalculateWaterPath(TArray<FVector>& OutPoints, float EffectiveRange) const;
	void TraceAlongWaterPath(float DeltaSeconds);

	FVector GetNozzleLocation() const;
	FVector GetNozzleForward() const;

	// Audio
	void UpdateAudio(float DeltaSeconds);
	void TryPlayImpactOneShot(const TArray<FVector>& WaterPath);

	// Haptics
	void UpdateHaptics(float DeltaSeconds);
	void SetHandHaptics(bool bLeft, float Frequency01, float Amplitude01);
	void StopHandHaptics(bool bLeft);
	void PulseLeftHandOnNozzleTurn(float Strength01);

	// Helpers
	float ComputePatternAlpha() const;
	void ComputeGameplayParams(float& OutRange, float& OutRadius, float& OutWaterAmount) const;
	void ComputeNiagaraParams3(float& OutSpreadAngleDeg, float& OutSpawnRate, float& OutInitialSpeed) const;

private:
	bool bInputBound = false;
	bool bTestFiring = false;

	// Barrel grab tracking
	FVector PreviousLocalBarrelHandPos = FVector::ZeroVector;
	bool bIsBarrelFirstTick = false;

	UPROPERTY(EditAnywhere, Category = "Hose|Barrel")
	float BarrelSensitivity = 2.5f;

	// Impact one-shot
	float LastImpactPlayTime = -1000.f;

	// Haptics timing
	float LastNozzlePulseTime = -1000.f;

	// Cached PC
	TWeakObjectPtr<APlayerController> CachedPC;
};
