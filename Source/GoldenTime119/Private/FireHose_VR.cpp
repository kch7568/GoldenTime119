// FireHose_VR.cpp
#include "FireHose_VR.h"

#include "CombustibleComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

#include "NiagaraComponent.h"

AFireHose_VR::AFireHose_VR()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Root);

	BarrelPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BarrelPivot"));
	BarrelPivot->SetupAttachment(BodyMesh);

	BarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrelMesh"));
	BarrelMesh->SetupAttachment(BarrelPivot);

	WaterSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("WaterSpawnPoint"));
	WaterSpawnPoint->SetupAttachment(BodyMesh);

	NC_Water = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NC_Water"));
	NC_Water->SetupAttachment(WaterSpawnPoint);
	NC_Water->SetAutoActivate(false);

	LeverPivot = CreateDefaultSubobject<USceneComponent>(TEXT("LeverPivot"));
	LeverPivot->SetupAttachment(BodyMesh);

	LeverMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeverMesh"));
	LeverMesh->SetupAttachment(LeverPivot);

	AC_HoseSpray = CreateDefaultSubobject<UAudioComponent>(TEXT("AC_HoseSpray"));
	AC_HoseSpray->SetupAttachment(WaterSpawnPoint);
	AC_HoseSpray->bAutoActivate = false;
}

void AFireHose_VR::BeginPlay()
{
	Super::BeginPlay();

	CachedPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;

	BarrelRotation = 0.f;
	TargetBarrelRotation = 0.f;
	PatternAlpha = ComputePatternAlpha();
	UpdateModeFromPattern();

	if (NC_Water && NS_Water)
	{
		NC_Water->SetAsset(NS_Water);
		SetNiagaraParams3();
	}

	if (AC_HoseSpray && Snd_HoseSprayLoop)
	{
		AC_HoseSpray->SetSound(Snd_HoseSprayLoop);
		ApplyModeFilter();
	}

	SetupKeyboardTest();
}

void AFireHose_VR::SetupKeyboardTest()
{
	if (bInputBound) return;

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC) return;

	EnableInput(PC);

	if (InputComponent)
	{
		InputComponent->BindAction("Fire", IE_Pressed, this, &AFireHose_VR::StartFiring);
		InputComponent->BindAction("Fire", IE_Released, this, &AFireHose_VR::StopFiring);
		bInputBound = true;
	}
}

void AFireHose_VR::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsGrabbedLever && IsValid(GrabbingLeverController))
	{
		UpdateVRLeverFromController();
	}
	if (bIsGrabbedBarrel && IsValid(GrabbingBarrelController))
	{
		UpdateVRBarrelFromController();
	}

	UpdatePressure(DeltaSeconds);
	UpdateBarrelRotation(DeltaSeconds);

	PatternAlpha = ComputePatternAlpha();
	UpdateModeFromPattern();

	UpdateNiagara(DeltaSeconds);
	UpdateAudio(DeltaSeconds);
	UpdateHaptics(DeltaSeconds);

	const bool bShouldFireGameplay = (PressureAlpha > AudioOnThreshold);
	if (bShouldFireGameplay)
	{
		TraceAlongWaterPath(DeltaSeconds);
	}
}

// ============================================================
// IGrabInteractable
// ============================================================

void AFireHose_VR::OnGrabbed_Implementation(USceneComponent* GrabbingController, bool bIsLeftHand)
{
	if (!IsValid(GrabbingController)) return;

	if (bIsLeftHand)
	{
		const FVector HandLocation = GrabbingController->GetComponentLocation();
		const FVector LeverLocation = IsValid(LeverMesh) ? LeverMesh->GetComponentLocation() : GetActorLocation();
		const FVector BarrelLocation = IsValid(BarrelMesh) ? BarrelMesh->GetComponentLocation() : GetActorLocation();

		const float DistToLever = FVector::Dist(HandLocation, LeverLocation);
		const float DistToBarrel = FVector::Dist(HandLocation, BarrelLocation);

		if (DistToLever < DistToBarrel)
		{
			OnLeverGrabbed(GrabbingController);
		}
		else
		{
			OnBarrelGrabbed(GrabbingController);
		}
	}
	else
	{
		OnBodyGrabbed();
	}
}

void AFireHose_VR::OnReleased_Implementation(USceneComponent* GrabbingController, bool bIsLeftHand)
{
	if (bIsLeftHand)
	{
		if (bIsGrabbedLever) OnLeverReleased();
		if (bIsGrabbedBarrel) OnBarrelReleased();
	}
	else
	{
		OnBodyReleased();
	}
}

bool AFireHose_VR::CanBeGrabbed_Implementation() const
{
	return true;
}

FTransform AFireHose_VR::GetGrabTransform_Implementation(bool bIsLeftHand) const
{
	if (bIsLeftHand && IsValid(LeverMesh))
	{
		return LeverMesh->GetComponentTransform();
	}
	if (IsValid(BodyMesh))
	{
		return BodyMesh->GetComponentTransform();
	}
	return GetActorTransform();
}

// ============================================================
// VR
// ============================================================

void AFireHose_VR::OnBodyGrabbed()
{
	bIsGrabbedBody = true;
}

void AFireHose_VR::OnBodyReleased()
{
	bIsGrabbedBody = false;
	if (PressureAlpha <= AudioOnThreshold)
	{
		StopHandHaptics(false);
	}
}

void AFireHose_VR::OnLeverGrabbed(USceneComponent* GrabbingController)
{
	bIsGrabbedLever = true;
	GrabbingLeverController = GrabbingController;
}

void AFireHose_VR::OnLeverReleased()
{
	bIsGrabbedLever = false;
	GrabbingLeverController = nullptr;
	TargetPressure = 0.f;

	if (PressureAlpha <= AudioOnThreshold)
	{
		StopHandHaptics(true);
	}
}

void AFireHose_VR::OnBarrelGrabbed(USceneComponent* GrabbingController)
{
	bIsGrabbedBarrel = true;
	GrabbingBarrelController = GrabbingController;

	bIsBarrelFirstTick = true;

	const FVector CurrentHandLoc = GrabbingController->GetComponentLocation();
	PreviousLocalBarrelHandPos = IsValid(BarrelPivot)
		? BarrelPivot->GetComponentTransform().InverseTransformPosition(CurrentHandLoc)
		: CurrentHandLoc;
}

void AFireHose_VR::OnBarrelReleased()
{
	bIsGrabbedBarrel = false;
	GrabbingBarrelController = nullptr;
	TargetBarrelRotation = BarrelRotation;
}

void AFireHose_VR::UpdateVRLeverFromController()
{
	if (!IsValid(GrabbingLeverController) || !IsValid(LeverPivot)) return;

	const FVector ControllerLocalPos =
		LeverPivot->GetComponentTransform().InverseTransformPosition(GrabbingLeverController->GetComponentLocation());

	const float PullDistance = -ControllerLocalPos.Z;

	const float NewPressure = FMath::GetMappedRangeValueClamped(
		FVector2D(0.f, 15.f),
		FVector2D(0.f, 1.0f),
		PullDistance
	);

	SetLeverPull(NewPressure);
}

void AFireHose_VR::UpdateVRBarrelFromController()
{
	if (!IsValid(GrabbingBarrelController) || !IsValid(BarrelPivot)) return;

	const FVector CurrentHandLoc = GrabbingBarrelController->GetComponentLocation();
	const FVector LocalHandPos = BarrelPivot->GetComponentTransform().InverseTransformPosition(CurrentHandLoc);

	if (bIsBarrelFirstTick)
	{
		PreviousLocalBarrelHandPos = LocalHandPos;
		bIsBarrelFirstTick = false;
		return;
	}

	const float MoveDistance = FMath::Abs(LocalHandPos.Y - PreviousLocalBarrelHandPos.Y);
	const float RotationToAdd = MoveDistance * BarrelSensitivity;

	if (RotationToAdd > KINDA_SMALL_NUMBER)
	{
		const float PrevRot = BarrelRotation;

		BarrelRotation = FMath::Clamp(BarrelRotation + RotationToAdd, 0.f, 180.f);
		TargetBarrelRotation = BarrelRotation;
		UpdateBarrelVisual();

		const float DeltaRot = FMath::Abs(BarrelRotation - PrevRot);
		const float Strength01 = FMath::Clamp(DeltaRot / 6.0f, 0.f, 1.f);
		PulseLeftHandOnNozzleTurn(Strength01);
	}

	PreviousLocalBarrelHandPos = LocalHandPos;
}

void AFireHose_VR::SetLeverPull(float PullAmount)
{
	LeverPullAmount = FMath::Clamp(PullAmount, 0.f, 1.f);
	TargetPressure = LeverPullAmount;
	UpdateLeverVisual();
}

void AFireHose_VR::SetBarrelRotation(float RotationDegrees)
{
	TargetBarrelRotation = FMath::Clamp(RotationDegrees, 0.f, 180.f);

	if (bIsGrabbedBarrel)
	{
		BarrelRotation = TargetBarrelRotation;
		UpdateBarrelVisual();
	}
}

// ============================================================
// Control
// ============================================================

void AFireHose_VR::SetPressure(float NewPressure)
{
	TargetPressure = FMath::Clamp(NewPressure, 0.f, 1.f);
	LeverPullAmount = TargetPressure;
	UpdateLeverVisual();
}

void AFireHose_VR::StartFiring()
{
	bTestFiring = true;
	TargetPressure = 1.0f;
}

void AFireHose_VR::StopFiring()
{
	bTestFiring = false;
	TargetPressure = 0.f;
}

// ============================================================
// Internal Updates
// ============================================================

void AFireHose_VR::UpdatePressure(float DeltaSeconds)
{
	const float InterpSpeed = (TargetPressure > PressureAlpha) ? PressureIncreaseSpeed : PressureDecreaseSpeed;
	PressureAlpha = FMath::FInterpTo(PressureAlpha, TargetPressure, DeltaSeconds, InterpSpeed);

	if (TargetPressure <= 0.0f && PressureAlpha < AudioOnThreshold)
	{
		PressureAlpha = 0.0f;
	}

	PressureAlpha = FMath::Clamp(PressureAlpha, 0.f, 1.f);
	LeverPullAmount = PressureAlpha;
	UpdateLeverVisual();
}

void AFireHose_VR::UpdateBarrelRotation(float DeltaSeconds)
{
	if (!bIsGrabbedBarrel)
	{
		BarrelRotation = FMath::FInterpTo(BarrelRotation, TargetBarrelRotation, DeltaSeconds, BarrelRotationSpeed);
		UpdateBarrelVisual();
	}
}

void AFireHose_VR::UpdateBarrelVisual()
{
	if (!IsValid(BarrelPivot)) return;

	FRotator NewRotation = FRotator::ZeroRotator;
	NewRotation.Yaw = BarrelRotation;
	BarrelPivot->SetRelativeRotation(NewRotation);
}

void AFireHose_VR::UpdateLeverVisual()
{
	if (!IsValid(LeverPivot)) return;

	const float RotationAngle = LeverPullAmount * LeverMaxRotation;

	FRotator NewRotation = FRotator::ZeroRotator;
	NewRotation.Pitch = RotationAngle;
	LeverPivot->SetRelativeRotation(NewRotation);
}

float AFireHose_VR::ComputePatternAlpha() const
{
	return FMath::Clamp(BarrelRotation / 180.f, 0.f, 1.f);
}

void AFireHose_VR::UpdateModeFromPattern()
{
	const EHoseMode_VR NewMode = (PatternAlpha < FocusedThreshold) ? EHoseMode_VR::Focused : EHoseMode_VR::Spray;
	if (NewMode != CurrentMode)
	{
		CurrentMode = NewMode;
		ApplyModeFilter();
	}
}

// === Gameplay params (C++ 전용) ===
void AFireHose_VR::ComputeGameplayParams(float& OutRange, float& OutRadius, float& OutWaterAmount) const
{
	const float RangeBase = FMath::Lerp(FocusedRange, SprayRange, PatternAlpha);
	const float RadiusBase = FMath::Lerp(FocusedRadius, SprayRadius, PatternAlpha);
	const float WaterBase = FMath::Lerp(FocusedWaterAmount, SprayWaterAmount, PatternAlpha);

	const float PressureCurve = FMath::Pow(PressureAlpha, 0.85f);

	OutRange = RangeBase * PressureCurve;
	OutRadius = RadiusBase;
	OutWaterAmount = WaterBase * PressureAlpha;
}

// === Niagara params (딱 3개만) ===
void AFireHose_VR::ComputeNiagaraParams3(float& OutSpreadAngleDeg, float& OutSpawnRate, float& OutInitialSpeed) const
{
	// 패턴 기반(노즐 회전)
	OutSpreadAngleDeg = FMath::Lerp(FocusedSpreadAngleDeg, SpraySpreadAngleDeg, PatternAlpha);

	// 속도는 패턴에 따라 기본값 보간 + 압력 영향(약간)
	const float SpeedBase = FMath::Lerp(FocusedInitialSpeed, SprayInitialSpeed, PatternAlpha);
	const float PressureCurve = FMath::Pow(PressureAlpha, 0.85f);
	OutInitialSpeed = SpeedBase * FMath::Lerp(0.65f, 1.0f, PressureCurve);

	// 스폰레이트는 패턴별 Max 보간 + 압력 스케일
	const float SpawnMax = FMath::Lerp(FocusedSpawnRateMax, SpraySpawnRateMax, PatternAlpha);
	OutSpawnRate = SpawnMax * PressureCurve;
}

void AFireHose_VR::UpdateNiagara(float DeltaSeconds)
{
	if (!IsValid(NC_Water)) return;

	const bool bShouldFX = (PressureAlpha > AudioOnThreshold);

	if (bShouldFX)
	{
		if (NS_Water && NC_Water->GetAsset() != NS_Water)
		{
			NC_Water->SetAsset(NS_Water);
		}

		if (!NC_Water->IsActive())
		{
			NC_Water->Activate(true);
		}

		SetNiagaraParams3();
	}
	else
	{
		// 끌 때도 “3개”만 0으로 내려서 잔상/변화 최소화
		NC_Water->SetVariableFloat(NiagaraParam_ParticleSpawnRate, 0.f);
		NC_Water->SetVariableFloat(NiagaraParam_InitialSpeed, 0.f);
		NC_Water->SetVariableFloat(NiagaraParam_SpreadAngleDeg, 0.f);

		if (NC_Water->IsActive())
		{
			NC_Water->Deactivate();
		}
	}
}

void AFireHose_VR::SetNiagaraParams3()
{
	if (!IsValid(NC_Water)) return;

	float SpreadDeg = 0.f;
	float SpawnRate = 0.f;
	float InitialSpeed = 0.f;

	ComputeNiagaraParams3(SpreadDeg, SpawnRate, InitialSpeed);

	NC_Water->SetVariableFloat(NiagaraParam_SpreadAngleDeg, SpreadDeg);
	NC_Water->SetVariableFloat(NiagaraParam_ParticleSpawnRate, SpawnRate);
	NC_Water->SetVariableFloat(NiagaraParam_InitialSpeed, InitialSpeed);
}

FVector AFireHose_VR::GetNozzleLocation() const
{
	if (IsValid(WaterSpawnPoint)) return WaterSpawnPoint->GetComponentLocation();
	if (IsValid(BarrelMesh)) return BarrelMesh->GetComponentLocation();
	return GetActorLocation();
}

FVector AFireHose_VR::GetNozzleForward() const
{
	if (IsValid(WaterSpawnPoint)) return WaterSpawnPoint->GetForwardVector();
	if (IsValid(BarrelMesh)) return BarrelMesh->GetForwardVector();
	return GetActorForwardVector();
}

void AFireHose_VR::CalculateWaterPath(TArray<FVector>& OutPoints, float EffectiveRange) const
{
	OutPoints.Empty();

	const FVector StartPos = GetNozzleLocation();
	const FVector Forward = GetNozzleForward();

	for (int32 i = 0; i <= TraceSegments; i++)
	{
		const float T = (float)i / (float)TraceSegments;
		const float Distance = EffectiveRange * T;

		OutPoints.Add(StartPos + (Forward * Distance));
	}
}

void AFireHose_VR::TraceAlongWaterPath(float DeltaSeconds)
{
	float EffectiveRange = 0.f;
	float EffectiveRadius = 0.f;
	float EffectiveWaterAmount = 0.f;

	ComputeGameplayParams(EffectiveRange, EffectiveRadius, EffectiveWaterAmount);

	TArray<FVector> WaterPath;
	CalculateWaterPath(WaterPath, EffectiveRange);

	TryPlayImpactOneShot(WaterPath);

	TSet<AActor*> HitActors;

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(this);

	for (int32 i = 0; i < WaterPath.Num() - 1; i++)
	{
		const FVector SegmentCenter = (WaterPath[i] + WaterPath[i + 1]) * 0.5f;

		TArray<AActor*> OutActors;
		const bool bHit = UKismetSystemLibrary::SphereOverlapActors(
			GetWorld(),
			SegmentCenter,
			EffectiveRadius,
			ObjectTypes,
			AActor::StaticClass(),
			ActorsToIgnore,
			OutActors
		);

		if (!bHit) continue;

		for (AActor* HitActor : OutActors)
		{
			if (!IsValid(HitActor)) continue;
			if (HitActors.Contains(HitActor)) continue;

			UCombustibleComponent* Comb = HitActor->FindComponentByClass<UCombustibleComponent>();
			if (!Comb) continue;

			const float Dist = FVector::Dist(WaterPath[0], SegmentCenter);
			float DistanceRatio = 1.f - FMath::Clamp(Dist / FMath::Max(1.f, EffectiveRange), 0.f, 1.f);
			DistanceRatio = FMath::Max(0.3f, DistanceRatio);

			const float FinalAmount = EffectiveWaterAmount * DistanceRatio * DeltaSeconds;
			Comb->AddWaterContact(FinalAmount);

			HitActors.Add(HitActor);
		}
	}
}

// ============================================================
// Audio
// ============================================================

void AFireHose_VR::UpdateAudio(float DeltaSeconds)
{
	if (!AC_HoseSpray || !AC_HoseSpray->Sound) return;

	const bool bSprayActive = (PressureAlpha > AudioOnThreshold);

	if (bSprayActive)
	{
		if (!AC_HoseSpray->IsPlaying())
		{
			AC_HoseSpray->FadeIn(HoseFadeInSec, 1.0f);
		}

		const float Vol = FMath::Clamp(PressureAlpha, 0.f, 1.f) * HoseVolumeMax;
		AC_HoseSpray->SetVolumeMultiplier(Vol);
	}
	else
	{
		if (AC_HoseSpray->IsPlaying())
		{
			AC_HoseSpray->FadeOut(HoseFadeOutSec, 0.0f);
		}
	}
}

void AFireHose_VR::ApplyModeFilter()
{
	if (!AC_HoseSpray) return;

	if (!bEnableModeFilter)
	{
		AC_HoseSpray->SetLowPassFilterEnabled(false);
		return;
	}

	AC_HoseSpray->SetLowPassFilterEnabled(true);

	const float TargetHz = (CurrentMode == EHoseMode_VR::Focused) ? Focused_LPF_Hz : Spray_LPF_Hz;
	AC_HoseSpray->SetLowPassFilterFrequency(TargetHz);
}

void AFireHose_VR::TryPlayImpactOneShot(const TArray<FVector>& WaterPath)
{
	if (!Snd_WaterHitFloorHeavy_OneShot) return;
	if (PressureAlpha <= AudioOnThreshold) return;
	if (WaterPath.Num() < 2) return;

	UWorld* W = GetWorld();
	if (!W) return;

	const float Now = W->GetTimeSeconds();
	if (Now - LastImpactPlayTime < ImpactMinIntervalSec) return;

	const FVector EndA = WaterPath.Last();
	const FVector EndB = EndA + (GetNozzleForward() * 60.f);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HoseImpactTrace), false, this);

	const bool bHit = W->LineTraceSingleByChannel(
		Hit,
		EndA,
		EndB,
		ECC_Visibility,
		Params
	);

	if (!bHit) return;

	const float Vol = FMath::Clamp(PressureAlpha, 0.f, 1.f) * ImpactVolumeMax;
	const float Pitch = FMath::FRandRange(ImpactPitchMin, ImpactPitchMax);

	UGameplayStatics::PlaySoundAtLocation(
		this,
		Snd_WaterHitFloorHeavy_OneShot,
		Hit.ImpactPoint,
		Vol,
		Pitch
	);

	LastImpactPlayTime = Now;
}

// ============================================================
// Haptics (Dynamic)
// ============================================================

void AFireHose_VR::SetHandHaptics(bool bLeft, float Frequency01, float Amplitude01)
{
	if (!bEnableHaptics) return;

	APlayerController* PC = CachedPC.IsValid()
		? CachedPC.Get()
		: (GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr);

	if (!PC) return;

	const float F = FMath::Clamp(Frequency01, 0.f, 1.f);
	const float A = FMath::Clamp(Amplitude01, 0.f, 1.f);

	PC->SetHapticsByValue(F, A, bLeft ? EControllerHand::Left : EControllerHand::Right);
}

void AFireHose_VR::StopHandHaptics(bool bLeft)
{
	if (!bEnableHaptics) return;
	SetHandHaptics(bLeft, 0.f, 0.f);
}

void AFireHose_VR::PulseLeftHandOnNozzleTurn(float Strength01)
{
	if (!bEnableHaptics) return;

	UWorld* W = GetWorld();
	if (!W) return;

	const float Now = W->GetTimeSeconds();
	if (Now - LastNozzlePulseTime < NozzleTurnPulseMinInterval) return;

	const float Amp = FMath::Clamp(NozzleTurnPulseAmp * Strength01, 0.f, 1.f);
	const float Freq = FMath::Clamp(NozzleTurnPulseFreq, 0.f, 1.f);

	SetHandHaptics(true, Freq, Amp);
	LastNozzlePulseTime = Now;
}

void AFireHose_VR::UpdateHaptics(float DeltaSeconds)
{
	if (!bEnableHaptics) return;

	const bool bSpraying = (PressureAlpha > AudioOnThreshold);

	if (bSpraying)
	{
		const float Amp = FMath::Clamp(PressureAlpha * SprayHapticAmpMax, 0.f, 1.f);
		const float Freq = FMath::Clamp(FMath::Lerp(0.35f, SprayHapticFreqMax, PressureAlpha), 0.f, 1.f);

		SetHandHaptics(true, Freq, Amp);
		SetHandHaptics(false, Freq, Amp);
	}
	else
	{
		StopHandHaptics(true);
		StopHandHaptics(false);
	}
}
