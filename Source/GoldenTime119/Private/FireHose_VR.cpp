// FireHose_VR.cpp
#include "FireHose_VR.h"
#include "CombustibleComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogHoseVR, Log, All);

AFireHose_VR::AFireHose_VR()
{
    PrimaryActorTick.bCanEverTick = true;

    // Root
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // BodyMesh
    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(Root);

    // BarrelPivot
    BarrelPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BarrelPivot"));
    BarrelPivot->SetupAttachment(BodyMesh);

    // BarrelMesh
    BarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrelMesh"));
    BarrelMesh->SetupAttachment(BarrelPivot);

    // WaterSpawnPoint
    WaterSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("WaterSpawnPoint"));
    WaterSpawnPoint->SetupAttachment(BarrelMesh);

    // WaterPsc
    WaterPsc = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("WaterPSC"));
    WaterPsc->SetupAttachment(WaterSpawnPoint);
    WaterPsc->bAutoActivate = false;

    // LeverPivot
    LeverPivot = CreateDefaultSubobject<USceneComponent>(TEXT("LeverPivot"));
    LeverPivot->SetupAttachment(BodyMesh);

    // LeverMesh
    LeverMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeverMesh"));
    LeverMesh->SetupAttachment(LeverPivot);
}

void AFireHose_VR::BeginPlay()
{
    Super::BeginPlay();

    UpdateWaterVFX();

    if (bEnableKeyboardTest)
    {
        SetupKeyboardTest();
    }

    UE_LOG(LogHoseVR, Warning, TEXT("[HoseVR] BeginPlay - Ready"));
    UE_LOG(LogHoseVR, Warning, TEXT("[HoseVR] Keyboard Test: %s"), bEnableKeyboardTest ? TEXT("ON") : TEXT("OFF"));
    UE_LOG(LogHoseVR, Warning, TEXT("[HoseVR] Controls: LMB=Fire, M=ToggleMode"));
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
        UE_LOG(LogHoseVR, Warning, TEXT("[HoseVR] Keyboard input bound"));
    }
}

void AFireHose_VR::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // VR: 손 위치에 따라 레버/노즐 업데이트
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
    UpdateMode();

    bool bShouldFire = (bIsGrabbedBody || bTestFiring) && (PressureAlpha > 0.01f);

    if (bShouldFire)
    {
        TraceAlongWaterPath(DeltaSeconds);
    }
}

// ============================================================
// IGrabInteractable 인터페이스 구현
// ============================================================

void AFireHose_VR::OnGrabbed_Implementation(USceneComponent* GrabbingController, bool bIsLeftHand)
{
    if (bIsLeftHand)
    {
        // 왼손: 레버 또는 노즐 (거리에 따라 결정)
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
        // 오른손: 몸체
        OnBodyGrabbed();
    }
}

void AFireHose_VR::OnReleased_Implementation(USceneComponent* GrabbingController, bool bIsLeftHand)
{
    if (bIsLeftHand)
    {
        if (bIsGrabbedLever)
        {
            OnLeverReleased();
        }
        if (bIsGrabbedBarrel)
        {
            OnBarrelReleased();
        }
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
    else if (IsValid(BodyMesh))
    {
        return BodyMesh->GetComponentTransform();
    }
    return GetActorTransform();
}

// ============================================================
// VR용 함수들
// ============================================================

void AFireHose_VR::OnBodyGrabbed()
{
    bIsGrabbedBody = true;
    UE_LOG(LogHoseVR, Warning, TEXT("[HoseVR] Body Grabbed (Right Hand)"));
}

void AFireHose_VR::OnBodyReleased()
{
    bIsGrabbedBody = false;
    UE_LOG(LogHoseVR, Warning, TEXT("[HoseVR] Body Released"));
}

void AFireHose_VR::OnLeverGrabbed(USceneComponent* GrabbingController)
{
    bIsGrabbedLever = true;
    GrabbingLeverController = GrabbingController;
    LeverGrabStartLocation = GrabbingController->GetComponentLocation();

    UE_LOG(LogHoseVR, Warning, TEXT("[HoseVR] Lever Grabbed (Left Hand)"));
}

void AFireHose_VR::OnLeverReleased()
{
    bIsGrabbedLever = false;
    GrabbingLeverController = nullptr;
    TargetPressure = 0.f;

    UE_LOG(LogHoseVR, Warning, TEXT("[HoseVR] Lever Released - Pressure decreasing"));
}

void AFireHose_VR::OnBarrelGrabbed(USceneComponent* GrabbingController)
{
    bIsGrabbedBarrel = true;
    GrabbingBarrelController = GrabbingController;
    BarrelGrabStartYaw = GrabbingController->GetComponentRotation().Yaw;

    UE_LOG(LogHoseVR, Warning, TEXT("[HoseVR] Barrel Grabbed (Left Hand)"));
}

void AFireHose_VR::OnBarrelReleased()
{
    bIsGrabbedBarrel = false;
    GrabbingBarrelController = nullptr;

    UE_LOG(LogHoseVR, Warning, TEXT("[HoseVR] Barrel Released"));
}

void AFireHose_VR::UpdateVRLeverFromController()
{
    if (!IsValid(GrabbingLeverController)) return;

    FVector CurrentLocation = GrabbingLeverController->GetComponentLocation();
    FVector LeverPivotLocation = LeverPivot->GetComponentLocation();

    // 손이 피벗에서 얼마나 당겨졌는지 계산
    FVector PullDirection = LeverPivotLocation - LeverGrabStartLocation;
    PullDirection.Normalize();

    FVector CurrentPull = CurrentLocation - LeverGrabStartLocation;
    float PullDistance = FVector::DotProduct(CurrentPull, -GetActorUpVector());

    // 당김 거리를 0~1로 변환 (10cm = 100% 당김)
    float PullAmount = FMath::Clamp(PullDistance / 10.f, 0.f, 1.f);

    SetLeverPull(PullAmount);
}

void AFireHose_VR::UpdateVRBarrelFromController()
{
    if (!IsValid(GrabbingBarrelController)) return;

    float CurrentYaw = GrabbingBarrelController->GetComponentRotation().Yaw;
    float YawDelta = CurrentYaw - BarrelGrabStartYaw;

    // Yaw 변화를 노즐 회전으로 변환
    float NewRotation = BarrelRotation + YawDelta;
    NewRotation = FMath::Clamp(NewRotation, 0.f, 180.f);

    SetBarrelRotation(NewRotation);

    // 시작 Yaw 업데이트 (연속 회전 가능하도록)
    BarrelGrabStartYaw = CurrentYaw;
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

    // VR에서는 즉시 적용
    if (bIsGrabbedBarrel)
    {
        BarrelRotation = TargetBarrelRotation;
        UpdateBarrelVisual();
    }
}

// ============================================================
// 일반 제어 함수들
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

        UE_LOG(LogHoseVR, Warning, TEXT("[HoseVR] Mode: %s"),
            CurrentMode == EHoseMode_VR::Focused ? TEXT("Focused") : TEXT("Spray"));
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
    UE_LOG(LogHoseVR, Warning, TEXT("[HoseVR] Start Firing (Test Mode)"));
}

void AFireHose_VR::StopFiring()
{
    bTestFiring = false;
    TargetPressure = 0.f;
    UE_LOG(LogHoseVR, Warning, TEXT("[HoseVR] Stop Firing (Test Mode)"));
}

// ============================================================
// 내부 업데이트 함수들
// ============================================================

void AFireHose_VR::UpdatePressure(float DeltaSeconds)
{
    if (TargetPressure > PressureAlpha)
    {
        PressureAlpha = FMath::FInterpTo(PressureAlpha, TargetPressure, DeltaSeconds, PressureIncreaseSpeed);
    }
    else
    {
        PressureAlpha = FMath::FInterpTo(PressureAlpha, TargetPressure, DeltaSeconds, PressureDecreaseSpeed);
    }

    PressureAlpha = FMath::Clamp(PressureAlpha, 0.f, 1.f);

    // PC 테스트: 수압에 따라 레버 시각적 업데이트
    if (bTestFiring || !bIsGrabbedLever)
    {
        LeverPullAmount = PressureAlpha;
        UpdateLeverVisual();
    }

    if (IsValid(WaterPsc))
    {
        WaterPsc->SetFloatParameter(TEXT("Pressure"), PressureAlpha);

        if (PressureAlpha > 0.01f)
        {
            if (!WaterPsc->IsActive())
            {
                WaterPsc->ActivateSystem(true);
            }
        }
        else
        {
            if (WaterPsc->IsActive())
            {
                WaterPsc->DeactivateSystem();
            }
        }
    }
}

void AFireHose_VR::UpdateBarrelRotation(float DeltaSeconds)
{
    // PC 테스트: 부드러운 노즐 회전 애니메이션
    if (!bIsGrabbedBarrel)
    {
        BarrelRotation = FMath::FInterpTo(BarrelRotation, TargetBarrelRotation, DeltaSeconds, BarrelRotationSpeed);
        UpdateBarrelVisual();
    }
}

// 1. 헤드(노즐) 부분 업데이트: Z축(Yaw) 회전
void AFireHose_VR::UpdateBarrelVisual()
{
    if (!IsValid(BarrelPivot)) return;

    FRotator NewRotation = FRotator::ZeroRotator;
    // 기존 Roll(X)에서 Yaw(Z)로 변경
    NewRotation.Yaw = BarrelRotation;
    BarrelPivot->SetRelativeRotation(NewRotation);
}

// 2. 손잡이(레버) 부분 업데이트: Y축(Pitch) 회전 및 -70도 제한
void AFireHose_VR::UpdateLeverVisual()
{
    if (!IsValid(LeverPivot)) return;

    // LeverPullAmount(0~1)에 최대 회전각을 곱함
    // LeverMaxRotation 값을 70.0f로 설정하면 0 ~ -70도 사이를 움직이게 됩니다.
    float RotationAngle = LeverPullAmount * LeverMaxRotation;

    FRotator NewRotation = FRotator::ZeroRotator;
    // Y축 회전(Pitch) 적용
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
    EHoseMode_VR NewMode = (BarrelRotation < 90.f) ? EHoseMode_VR::Focused : EHoseMode_VR::Spray;

    if (NewMode != CurrentMode)
    {
        CurrentMode = NewMode;
        UpdateWaterVFX();

        UE_LOG(LogHoseVR, Warning, TEXT("[HoseVR] Mode Changed: %s (Barrel: %.1f)"),
            CurrentMode == EHoseMode_VR::Focused ? TEXT("Focused") : TEXT("Spray"), BarrelRotation);
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
    float Gravity = WaterGravity;

    Range *= PressureAlpha;

    if (CurrentMode == EHoseMode_VR::Focused)
    {
        Gravity *= 0.3f;
    }

    for (int32 i = 0; i <= TraceSegments; i++)
    {
        float T = (float)i / (float)TraceSegments;
        float Distance = Range * T;
        float Drop = Gravity * T * T * (2.f - PressureAlpha);

        FVector Point = StartPos + (Forward * Distance) - FVector(0, 0, Drop);
        OutPoints.Add(Point);
    }
}

void AFireHose_VR::TraceAlongWaterPath(float DeltaSeconds)
{
    TArray<FVector> WaterPath;
    CalculateWaterPath(WaterPath);

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
        FVector SegmentStart = WaterPath[i];
        FVector SegmentEnd = WaterPath[i + 1];
        FVector SegmentCenter = (SegmentStart + SegmentEnd) * 0.5f;

        if (bDebugDraw)
        {
            DrawDebugLine(GetWorld(), SegmentStart, SegmentEnd, FColor::Cyan, false, 0.0f, 0, 2.0f * PressureAlpha);
            DrawDebugSphere(GetWorld(), SegmentCenter, Radius * PressureAlpha, 8, FColor::Blue, false, 0.0f, 0, 1.0f);
        }

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

                    if (bDebugDraw)
                    {
                        DrawDebugSphere(GetWorld(), HitActor->GetActorLocation(), 50.f, 8, FColor::Green, false, 0.1f);
                    }

                    UE_LOG(LogHoseVR, Verbose, TEXT("[HoseVR] WATER HIT: %s Amount=%.3f Pressure=%.2f"),
                        *GetNameSafe(HitActor), FinalAmount, PressureAlpha);
                }
            }
        }
    }
}