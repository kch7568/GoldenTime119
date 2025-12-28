// FireHose.cpp
#include "FireHose.h"
#include "CombustibleComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogHose, Log, All);

AFireHose::AFireHose()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    NozzlePoint = CreateDefaultSubobject<USceneComponent>(TEXT("NozzlePoint"));
    NozzlePoint->SetupAttachment(Root);

    WaterPsc = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("WaterPSC"));
    WaterPsc->SetupAttachment(NozzlePoint);
    WaterPsc->bAutoActivate = false;
}

void AFireHose::BeginPlay()
{
    Super::BeginPlay();
    SetMode(EHoseMode::Focused);
    SetupInputBindings();
}

void AFireHose::SetupInputBindings()
{
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        if (UInputComponent* InputComp = PC->InputComponent)
        {
            InputComp->BindAction("HoseMode_Focused", IE_Pressed, this, &AFireHose::OnFocusedModePressed);
            InputComp->BindAction("HoseMode_Spray", IE_Pressed, this, &AFireHose::OnSprayModePressed);
            InputComp->BindAction("HoseFire", IE_Pressed, this, &AFireHose::OnFirePressed);
            InputComp->BindAction("HoseFire", IE_Released, this, &AFireHose::OnFireReleased);

            UE_LOG(LogHose, Warning, TEXT("[Hose] Input bindings setup complete"));
        }
    }
}

void AFireHose::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bIsFiring)
    {
        TraceAlongWaterPath(DeltaSeconds);
    }
}

void AFireHose::SetMode(EHoseMode NewMode)
{
    CurrentMode = NewMode;
    UpdateWaterVFX();

    UE_LOG(LogHose, Warning, TEXT("[Hose] Mode changed to: %s"),
        CurrentMode == EHoseMode::Focused ? TEXT("Focused") : TEXT("Spray"));
}

void AFireHose::StartFiring()
{
    bIsFiring = true;

    if (IsValid(WaterPsc))
    {
        WaterPsc->ActivateSystem(true);
    }

    UE_LOG(LogHose, Warning, TEXT("[Hose] Start Firing - Mode: %s"),
        CurrentMode == EHoseMode::Focused ? TEXT("Focused") : TEXT("Spray"));
}

void AFireHose::StopFiring()
{
    bIsFiring = false;

    if (IsValid(WaterPsc))
    {
        WaterPsc->DeactivateSystem();
    }

    UE_LOG(LogHose, Warning, TEXT("[Hose] Stop Firing"));
}

void AFireHose::UpdateWaterVFX()
{
    if (!IsValid(WaterPsc)) return;

    UParticleSystem* Template = (CurrentMode == EHoseMode::Focused)
        ? FocusedWaterTemplate
        : SprayWaterTemplate;

    if (Template)
    {
        WaterPsc->SetTemplate(Template);
    }
}

void AFireHose::CalculateWaterPath(TArray<FVector>& OutPoints)
{
    OutPoints.Empty();

    FVector StartPos = NozzlePoint->GetComponentLocation();
    FVector Forward = NozzlePoint->GetForwardVector();

    float Range = (CurrentMode == EHoseMode::Focused) ? FocusedRange : SprayRange;
    float Gravity = WaterGravity;

    if (CurrentMode == EHoseMode::Focused)
    {
        Gravity *= 0.3f;
    }

    for (int32 i = 0; i <= TraceSegments; i++)
    {
        float T = (float)i / (float)TraceSegments;
        float Distance = Range * T;
        float Drop = Gravity * T * T;

        FVector Point = StartPos + (Forward * Distance) - FVector(0, 0, Drop);
        OutPoints.Add(Point);
    }
}

void AFireHose::TraceAlongWaterPath(float DeltaSeconds)
{
    TArray<FVector> WaterPath;
    CalculateWaterPath(WaterPath);

    float Radius = (CurrentMode == EHoseMode::Focused) ? FocusedRadius : SprayRadius;
    float WaterAmount = (CurrentMode == EHoseMode::Focused) ? FocusedWaterAmount : SprayWaterAmount;
    float Range = (CurrentMode == EHoseMode::Focused) ? FocusedRange : SprayRange;

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
            DrawDebugLine(GetWorld(), SegmentStart, SegmentEnd, FColor::Cyan, false, 0.0f, 0, 2.0f);
            DrawDebugSphere(GetWorld(), SegmentCenter, Radius, 8, FColor::Blue, false, 0.0f, 0, 1.0f);
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
                    // 거리 감소 완화 + 최소값 보장
                    float Distance = FVector::Dist(WaterPath[0], SegmentCenter);
                    float DistanceRatio = 1.f - FMath::Clamp(Distance / Range, 0.f, 1.f);
                    DistanceRatio = FMath::Max(0.3f, DistanceRatio);  // 최소 30% 보장

                    float FinalAmount = WaterAmount * DistanceRatio * DeltaSeconds;

                    Comb->AddWaterContact(FinalAmount);
                    HitActors.Add(HitActor);

                    if (bDebugDraw)
                    {
                        DrawDebugSphere(GetWorld(), HitActor->GetActorLocation(), 50.f, 8, FColor::Green, false, 0.1f);
                    }

                    UE_LOG(LogHose, Warning, TEXT("[Hose] WATER HIT: %s Amount=%.3f IsBurning=%d"),
                        *GetNameSafe(HitActor), FinalAmount, Comb->IsBurning());
                }
            }
        }
    }
}

void AFireHose::OnFocusedModePressed()
{
    SetMode(EHoseMode::Focused);
}

void AFireHose::OnSprayModePressed()
{
    SetMode(EHoseMode::Spray);
}

void AFireHose::OnFirePressed()
{
    StartFiring();
}

void AFireHose::OnFireReleased()
{
    StopFiring();
}