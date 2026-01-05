// ============================ GasTankActor.cpp ============================
#include "GasTankActor.h"
#include "PressureVesselComponent.h"
#include "CombustibleComponent.h"
#include "Components/StaticMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGasTank, Log, All);

AGasTankActor::AGasTankActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    TankMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TankMesh"));
    TankMesh->SetupAttachment(Root);

    Combustible = CreateDefaultSubobject<UCombustibleComponent>(TEXT("Combustible"));
    Combustible->CombustibleType = ECombustibleType::Explosive;
    Combustible->Ignition.Flammability = 0.8f;
    Combustible->Fuel.FuelInitial = 5.f;

    PressureVessel = CreateDefaultSubobject<UPressureVesselComponent>(TEXT("PressureVessel"));
}

void AGasTankActor::BeginPlay()
{
    Super::BeginPlay();

    ApplyTankTypeParameters();

    if (IsValid(PressureVessel))
    {
        PressureVessel->OnBLEVE.AddDynamic(this, &AGasTankActor::OnBLEVETriggered);
    }

    UE_LOG(LogGasTank, Warning, TEXT("[GasTank] BeginPlay: %s Type=%d"),
        *GetName(), (int32)TankType);
}

void AGasTankActor::ApplyTankTypeParameters()
{
    if (!IsValid(PressureVessel))
        return;

    switch (TankType)
    {
    case EGasTankType::SmallCanister:
        PressureVessel->VesselCapacityLiters = 3.f;
        PressureVessel->LiquidFillLevel01 = 0.8f;
        PressureVessel->BasePressure = 6.f;
        PressureVessel->BurstPressure = 20.f;
        PressureVessel->BaseBurstPressure = 20.f;
        PressureVessel->CriticalPressure = 16.f;
        PressureVessel->SafetyValveActivationPressure = 12.f;
        PressureVessel->VesselStrength = 0.8f;
        PressureVessel->FireballRadiusMultiplier = 0.6f;
        PressureVessel->BlastWaveIntensity = 0.5f;
        break;

    case EGasTankType::PortableTank:
        PressureVessel->VesselCapacityLiters = 15.f;
        PressureVessel->LiquidFillLevel01 = 0.7f;
        PressureVessel->BasePressure = 8.f;
        PressureVessel->BurstPressure = 25.f;
        PressureVessel->BaseBurstPressure = 25.f;
        PressureVessel->CriticalPressure = 20.f;
        PressureVessel->SafetyValveActivationPressure = 15.f;
        PressureVessel->VesselStrength = 1.0f;
        PressureVessel->FireballRadiusMultiplier = 1.0f;
        PressureVessel->BlastWaveIntensity = 1.0f;
        break;

    case EGasTankType::IndustrialTank:
        PressureVessel->VesselCapacityLiters = 75.f;
        PressureVessel->LiquidFillLevel01 = 0.65f;
        PressureVessel->BasePressure = 10.f;
        PressureVessel->BurstPressure = 30.f;
        PressureVessel->BaseBurstPressure = 30.f;
        PressureVessel->CriticalPressure = 24.f;
        PressureVessel->SafetyValveActivationPressure = 18.f;
        PressureVessel->VesselStrength = 1.2f;
        PressureVessel->FireballRadiusMultiplier = 1.5f;
        PressureVessel->BlastWaveIntensity = 1.5f;
        break;

    case EGasTankType::StorageTank:
        PressureVessel->VesselCapacityLiters = 500.f;
        PressureVessel->LiquidFillLevel01 = 0.6f;
        PressureVessel->BasePressure = 12.f;
        PressureVessel->BurstPressure = 35.f;
        PressureVessel->BaseBurstPressure = 35.f;
        PressureVessel->CriticalPressure = 28.f;
        PressureVessel->SafetyValveActivationPressure = 20.f;
        PressureVessel->VesselStrength = 1.5f;
        PressureVessel->FireballRadiusMultiplier = 2.5f;
        PressureVessel->BlastWaveIntensity = 2.0f;
        break;
    }

    UE_LOG(LogGasTank, Log, TEXT("[GasTank] Applied type %d: Capacity=%.1fL Burst=%.1f bar"),
        (int32)TankType, PressureVessel->VesselCapacityLiters, PressureVessel->BurstPressure);
}

void AGasTankActor::OnBLEVETriggered(FVector ExplosionLocation)
{
    if (bHasExploded)
        return;

    bHasExploded = true;

    UE_LOG(LogGasTank, Error, TEXT("[GasTank] ====== BLEVE! ====== %s at %s"),
        *GetName(), *ExplosionLocation.ToString());

    if (IsValid(TankMesh))
    {
        TankMesh->SetVisibility(false);
        TankMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    SetLifeSpan(10.0f);
}

void AGasTankActor::SetTankType(EGasTankType NewType)
{
    TankType = NewType;
    ApplyTankTypeParameters();
}

float AGasTankActor::GetPressureRatio() const
{
    if (IsValid(PressureVessel))
    {
        return PressureVessel->GetPressureRatio01();
    }
    return 0.f;
}

bool AGasTankActor::IsInDanger() const
{
    if (!IsValid(PressureVessel))
        return false;

    return PressureVessel->VesselState == EPressureVesselState::Critical ||
        PressureVessel->VesselState == EPressureVesselState::Venting;
}

void AGasTankActor::ForceBLEVE()
{
    if (IsValid(PressureVessel))
    {
        PressureVessel->ForceRupture();
    }
}