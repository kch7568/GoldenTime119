// FireHose_VR.cpp
#include "FireHose_VR.h"
#include "CombustibleComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

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

    WaterPsc = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("WaterPSC"));
    WaterPsc->SetupAttachment(WaterSpawnPoint);
    WaterPsc->bAutoActivate = false;

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

    CurrentMode = EHoseMode_VR::Focused;
    BarrelRotation = 0.f;
    UpdateWaterVFX();

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

    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;

    EnableInput(PC);

    if (InputComponent)
    {
        InputComponent->BindAction("Fire", IE_Pressed, this, &AFireHose_VR::StartFiring);
        InputComponent->BindAction("Fire", IE_Released, this, &AFireHose_VR::StopFiring);
        InputComponent->BindAction("ToggleMode", IE_Pressed, this, &AFireHose_VR::ToggleMode);
        bInputBound = true;
    }
}

void AFireHose_VR::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bIsGrabbedLever && IsValid(GrabbingLeverController)) UpdateVRLeverFromController();
    if (bIsGrabbedBarrel && IsValid(GrabbingBarrelController)) UpdateVRBarrelFromController();

    UpdatePressure(DeltaSeconds);
    UpdateBarrelRotation(DeltaSeconds);
    UpdateMode();

    UpdateAudio(DeltaSeconds);

    const bool bShouldFire = (PressureAlpha > 0.05f);
    if (bShouldFire)
    {
        TraceAlongWaterPath(DeltaSeconds);
    }
}

// ============================================================
// IGrabInteractable
// ============================================================

void AFireHose_VR::OnGrabbed_Implementation(USceneComponent* GrabbingController, bool bIsLeftHand)
{
    if (bIsLeftHand)
    {
        FVector HandLocation = GrabbingController->GetComponentLocation();
        FVector LeverLocation = LeverMesh->GetComponentLocation();
        FVector BarrelLocation = BarrelMesh->GetComponentLocation();

        float DistToLever = FVector::Dist(HandLocation, LeverLocation);
        float DistToBarrel = FVector::Dist(HandLocation, BarrelLocation);

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
}

void AFireHose_VR::OnLeverGrabbed(USceneComponent* GrabbingController)
{
    bIsGrabbedLever = true;
    GrabbingLeverController = GrabbingController;
    LeverGrabStartLocation = GrabbingController->GetComponentLocation();
}

void AFireHose_VR::OnLeverReleased()
{
    bIsGrabbedLever = false;
    GrabbingLeverController = nullptr;
    TargetPressure = 0.f;
}

void AFireHose_VR::OnBarrelGrabbed(USceneComponent* GrabbingController)
{
    bIsGrabbedBarrel = true;
    GrabbingBarrelController = GrabbingController;
    bIsBarrelFirstTick = true;
    CurrentGrabRotationSum = 0.f;
    bModeSwappedInThisGrab = false;

    RotationAtGrabStart = BarrelRotation;

    FVector CurrentHandLoc = GrabbingController->GetComponentLocation();
    PreviousLocalBarrelHandPos = BarrelPivot->GetComponentTransform().InverseTransformPosition(CurrentHandLoc);
}

void AFireHose_VR::OnBarrelReleased()
{
    bIsGrabbedBarrel = false;
    GrabbingBarrelController = nullptr;

    if (!bModeSwappedInThisGrab)
    {
        TargetBarrelRotation = RotationAtGrabStart;
    }
    else
    {
        TargetBarrelRotation = BarrelRotation;
    }
}

void AFireHose_VR::UpdateVRLeverFromController()
{
    if (!IsValid(GrabbingLeverController) || !IsValid(LeverPivot)) return;

    FVector ControllerLocalPos =
        LeverPivot->GetComponentTransform().InverseTransformPosition(GrabbingLeverController->GetComponentLocation());

    float PullDistance = -ControllerLocalPos.Z;

    float NewPressure = FMath::GetMappedRangeValueClamped(
        FVector2D(0.f, 15.f),
        FVector2D(0.f, 1.0f),
        PullDistance
    );

    SetLeverPull(NewPressure);
}

void AFireHose_VR::UpdateVRBarrelFromController()
{
    if (!IsValid(GrabbingBarrelController) || !IsValid(BarrelPivot)) return;

    FVector CurrentHandLoc = GrabbingBarrelController->GetComponentLocation();
    FVector LocalHandPos = BarrelPivot->GetComponentTransform().InverseTransformPosition(CurrentHandLoc);

    if (bIsBarrelFirstTick)
    {
        PreviousLocalBarrelHandPos = LocalHandPos;
        bIsBarrelFirstTick = false;
        return;
    }

    float MoveDistance = FMath::Abs(LocalHandPos.Y - PreviousLocalBarrelHandPos.Y);
    float RotationToAdd = MoveDistance * BarrelSensitivity;

    if (!bModeSwappedInThisGrab)
    {
        float Remaining = RotationThresholdPerGrab - CurrentGrabRotationSum;
        float ActualAdd = FMath::Min(RotationToAdd, Remaining);

        CurrentGrabRotationSum += ActualAdd;
        BarrelRotation += ActualAdd;

        if (CurrentGrabRotationSum >= RotationThresholdPerGrab)
        {
            ToggleMode();
            bModeSwappedInThisGrab = true;
        }

        UpdateBarrelVisual();
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

void AFireHose_VR::SetMode(EHoseMode_VR NewMode)
{
    if (CurrentMode != NewMode)
    {
        CurrentMode = NewMode;
        TargetBarrelRotation = (NewMode == EHoseMode_VR::Focused) ? 0.f : 180.f;
        UpdateWaterVFX();
        ApplyModeFilter();
    }
}

void AFireHose_VR::ToggleMode()
{
    SetMode(CurrentMode == EHoseMode_VR::Focused ? EHoseMode_VR::Spray : EHoseMode_VR::Focused);
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
    float InterpSpeed = (TargetPressure > PressureAlpha) ? PressureIncreaseSpeed : PressureDecreaseSpeed;
    PressureAlpha = FMath::FInterpTo(PressureAlpha, TargetPressure, DeltaSeconds, InterpSpeed);

    if (TargetPressure <= 0.0f && PressureAlpha < 0.05f)
    {
        PressureAlpha = 0.0f;
    }

    PressureAlpha = FMath::Clamp(PressureAlpha, 0.f, 1.f);
    LeverPullAmount = PressureAlpha;
    UpdateLeverVisual();

    if (PressureAlpha > 0.05f)
    {
        WaterPsc->SetFloatParameter(TEXT("Pressure"), PressureAlpha);

        if (!bWaterVFXActive)
        {
            bWaterVFXActive = true;
            WaterPsc->ActivateSystem(true);
        }
    }
    else
    {
        WaterPsc->SetFloatParameter(TEXT("Pressure"), 0.f);

        if (bWaterVFXActive)
        {
            bWaterVFXActive = false;
            WaterPsc->DeactivateSystem();
        }
    }
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

    float RotationAngle = LeverPullAmount * LeverMaxRotation;

    FRotator NewRotation = FRotator::ZeroRotator;
    NewRotation.Pitch = RotationAngle;
    LeverPivot->SetRelativeRotation(NewRotation);
}

void AFireHose_VR::UpdateWaterVFX()
{
    if (!IsValid(WaterPsc)) return;

    UParticleSystem* Template = (CurrentMode == EHoseMode_VR::Focused)
        ? FocusedWaterTemplate
        : SprayWaterTemplate;

    if (Template)
    {
        WaterPsc->SetTemplate(Template);
    }
}

void AFireHose_VR::UpdateMode()
{
    int32 RotationStep = FMath::FloorToInt(BarrelRotation / 90.f);
    EHoseMode_VR NewMode = (RotationStep % 2 == 0) ? EHoseMode_VR::Focused : EHoseMode_VR::Spray;

    if (NewMode != CurrentMode)
    {
        CurrentMode = NewMode;
        UpdateWaterVFX();
        ApplyModeFilter();
    }
}

FVector AFireHose_VR::GetNozzleLocation() const
{
    if (IsValid(WaterSpawnPoint))
    {
        return WaterSpawnPoint->GetComponentLocation();
    }
    if (IsValid(BarrelMesh))
    {
        return BarrelMesh->GetComponentLocation();
    }
    return GetActorLocation();
}

FVector AFireHose_VR::GetNozzleForward() const
{
    if (IsValid(WaterSpawnPoint))
    {
        return WaterSpawnPoint->GetForwardVector();
    }
    if (IsValid(BarrelMesh))
    {
        return BarrelMesh->GetForwardVector();
    }
    return GetActorForwardVector();
}

void AFireHose_VR::CalculateWaterPath(TArray<FVector>& OutPoints)
{
    OutPoints.Empty();

    FVector StartPos = GetNozzleLocation();
    FVector Forward = GetNozzleForward();

    float Range = (CurrentMode == EHoseMode_VR::Focused) ? FocusedRange : SprayRange;
    Range *= PressureAlpha;

    for (int32 i = 0; i <= TraceSegments; i++)
    {
        float T = (float)i / (float)TraceSegments;
        float Distance = Range * T;

        FVector Point = StartPos + (Forward * Distance);
        OutPoints.Add(Point);
    }
}

void AFireHose_VR::TraceAlongWaterPath(float DeltaSeconds)
{
    TArray<FVector> WaterPath;
    CalculateWaterPath(WaterPath);

    TryPlayImpactOneShot(WaterPath);

    float Radius = (CurrentMode == EHoseMode_VR::Focused) ? FocusedRadius : SprayRadius;
    float WaterAmount = (CurrentMode == EHoseMode_VR::Focused) ? FocusedWaterAmount : SprayWaterAmount;
    float Range = (CurrentMode == EHoseMode_VR::Focused) ? FocusedRange : SprayRange;

    WaterAmount *= PressureAlpha;
    Range *= PressureAlpha;

    TSet<AActor*> HitActors;

    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);

    for (int32 i = 0; i < WaterPath.Num() - 1; i++)
    {
        FVector SegmentCenter = (WaterPath[i] + WaterPath[i + 1]) * 0.5f;

        TArray<AActor*> OutActors;
        bool bHit = UKismetSystemLibrary::SphereOverlapActors(
            GetWorld(),
            SegmentCenter,
            Radius,
            ObjectTypes,
            AActor::StaticClass(),
            ActorsToIgnore,
            OutActors
        );

        if (bHit)
        {
            for (AActor* HitActor : OutActors)
            {
                if (!IsValid(HitActor)) continue;
                if (HitActors.Contains(HitActor)) continue;

                UCombustibleComponent* Comb = HitActor->FindComponentByClass<UCombustibleComponent>();
                if (Comb)
                {
                    float Dist = FVector::Dist(WaterPath[0], SegmentCenter);
                    float DistanceRatio = 1.f - FMath::Clamp(Dist / FMath::Max(1.f, Range), 0.f, 1.f);
                    DistanceRatio = FMath::Max(0.3f, DistanceRatio);

                    float FinalAmount = WaterAmount * DistanceRatio * DeltaSeconds;

                    Comb->AddWaterContact(FinalAmount);
                    HitActors.Add(HitActor);
                }
            }
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

    const float Now = GetWorld()->GetTimeSeconds();
    if (Now - LastImpactPlayTime < ImpactMinIntervalSec) return;

    const FVector EndA = WaterPath.Last();
    const FVector EndB = EndA + (GetNozzleForward() * 60.f);

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HoseImpactTrace), false, this);

    const bool bHit = GetWorld()->LineTraceSingleByChannel(
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
