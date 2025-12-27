// FireHose.cpp
#include "FireHose.h"
#include "CombustibleComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogHose, Log, All);

AFireHose::AFireHose()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    WaterPsc = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("WaterPSC"));
    WaterPsc->SetupAttachment(Root);
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
        ApplyWaterToTargets(DeltaSeconds);
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

    UParticleSystem* Template = nullptr;

    switch (CurrentMode)
    {
    case EHoseMode::Focused:
        Template = FocusedWaterTemplate;
        break;
    case EHoseMode::Spray:
        Template = SprayWaterTemplate;
        break;
    }

    if (Template)
    {
        WaterPsc->SetTemplate(Template);
    }
}

void AFireHose::ApplyWaterToTargets(float DeltaSeconds)
{
    float Range = (CurrentMode == EHoseMode::Focused) ? FocusedRange : SprayRange;
    float Radius = (CurrentMode == EHoseMode::Focused) ? FocusedRadius : SprayRadius;
    float WaterAmount = (CurrentMode == EHoseMode::Focused) ? FocusedWaterAmount : SprayWaterAmount;

    FVector Start = GetActorLocation();
    FVector Forward = GetActorForwardVector();
    FVector End = Start + Forward * Range;

    TArray<FHitResult> HitResults;
    TArray<AActor*> IgnoreActors;
    IgnoreActors.Add(this);

    bool bHit = UKismetSystemLibrary::SphereTraceMulti(
        GetWorld(),
        Start,
        End,
        Radius,
        UEngineTypes::ConvertToTraceType(ECC_WorldDynamic),
        false,
        IgnoreActors,
        EDrawDebugTrace::ForOneFrame,
        HitResults,
        true
    );

    if (bHit)
    {
        for (const FHitResult& Hit : HitResults)
        {
            AActor* HitActor = Hit.GetActor();
            if (!IsValid(HitActor)) continue;

            UCombustibleComponent* Comb = HitActor->FindComponentByClass<UCombustibleComponent>();
            if (Comb)
            {
                float Distance = FVector::Dist(Start, Hit.ImpactPoint);
                float DistanceRatio = 1.f - FMath::Clamp(Distance / Range, 0.f, 1.f);
                float FinalAmount = WaterAmount * DistanceRatio * DeltaSeconds;

                Comb->AddWaterContact(FinalAmount);

                UE_LOG(LogHose, Verbose, TEXT("[Hose] Water hit: %s Amount=%.3f"),
                    *GetNameSafe(HitActor), FinalAmount);
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