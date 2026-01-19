// ============================ GasTankActor.cpp ============================
#include "GasTankActor.h"
#include "PressureVesselComponent.h"
#include "CombustibleComponent.h"
#include "FireActor.h"
#include "RoomActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogGasTank, Log, All);

AGasTankActor::AGasTankActor()
{
    PrimaryActorTick.bCanEverTick = true; // 누출 피치 업데이트용

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    TankMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TankMesh"));
    TankMesh->SetupAttachment(Root);

    Combustible = CreateDefaultSubobject<UCombustibleComponent>(TEXT("Combustible"));
    Combustible->CombustibleType = ECombustibleType::Explosive;
    Combustible->Ignition.Flammability = 0.8f;
    Combustible->Fuel.FuelInitial = 5.f;

    PressureVessel = CreateDefaultSubobject<UPressureVesselComponent>(TEXT("PressureVessel"));

    // ===== Audio Component 생성 =====
    GasLeakAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("GasLeakAudio"));
    GasLeakAudioComponent->SetupAttachment(Root);
    GasLeakAudioComponent->bAutoActivate = false;
    GasLeakAudioComponent->bAutoDestroy = false;
    GasLeakAudioComponent->SetPitchMultiplier(GasLeakPitchMin);
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

void AGasTankActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateGasLeakAudio(DeltaSeconds);
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

// ===== 가스 누출 오디오 업데이트 =====
void AGasTankActor::UpdateGasLeakAudio(float DeltaSeconds)
{
    if (!IsValid(PressureVessel) || !IsValid(GasLeakAudioComponent))
    {
        return;
    }

    const bool bLeakState =
        (PressureVessel->VesselState == EPressureVesselState::Venting) ||
        (PressureVessel->VesselState == EPressureVesselState::Critical);

    if (!bLeakState)
    {
        if (GasLeakAudioComponent->IsPlaying())
        {
            GasLeakAudioComponent->FadeOut(0.25f, 0.0f);
            UE_LOG(LogGasTank, Verbose, TEXT("[GasTank] Leak stop: %s"), *GetName());
        }
        return;
    }

    // 누출 상태일 때 재생 시작
    if (!GasLeakAudioComponent->IsPlaying())
    {
        if (GasLeakSound)
        {
            GasLeakAudioComponent->SetSound(GasLeakSound);
        }
        GasLeakAudioComponent->SetPitchMultiplier(GasLeakPitchMin);
        GasLeakAudioComponent->Play();
        UE_LOG(LogGasTank, Verbose, TEXT("[GasTank] Leak start: %s"), *GetName());
    }

    // 압력 비율 기반 피치 계산 (0.0 ~ 1.0)
    const float Ratio = FMath::Clamp(PressureVessel->GetPressureRatio01(), 0.0f, 1.0f);

    // 기본 선형 보간
    float TargetPitch = FMath::Lerp(GasLeakPitchMin, GasLeakPitchMax, Ratio);

    // 폭발 직전 급격한 상승 가중
    if (Ratio > GasLeakRapidIncreaseStartRatio)
    {
        const float ExtraAlpha = (Ratio - GasLeakRapidIncreaseStartRatio) / (1.0f - GasLeakRapidIncreaseStartRatio);
        // 급가속 느낌을 주기 위해 지수 가중
        const float Boost = FMath::Clamp(FMath::Pow(ExtraAlpha, 2.0f), 0.0f, 1.0f);
        TargetPitch = FMath::Lerp(TargetPitch, GasLeakPitchMax, Boost);
    }

    const float CurrentPitch = GasLeakAudioComponent->PitchMultiplier;
    const float NewPitch = FMath::FInterpTo(CurrentPitch, TargetPitch, DeltaSeconds, GasLeakPitchInterpSpeed);
    GasLeakAudioComponent->SetPitchMultiplier(NewPitch);
}

void AGasTankActor::OnBLEVETriggered(FVector ExplosionLocation)
{
    if (bHasExploded)
        return;

    bHasExploded = true;

    UE_LOG(LogGasTank, Error, TEXT("[GasTank] ====== BLEVE! ====== %s at %s"),
        *GetName(), *ExplosionLocation.ToString());

    // 누출 루프 정지
    if (IsValid(GasLeakAudioComponent) && GasLeakAudioComponent->IsPlaying())
    {
        GasLeakAudioComponent->Stop();
    }

    // 폭발 사운드 재생 (원샷)
    if (ExplosionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, ExplosionLocation);
    }

    // 1) 탱크 메시 즉시 숨기기
    if (IsValid(TankMesh))
    {
        TankMesh->SetVisibility(false);
        TankMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // 2) 이 액터에 Attach된 모든 FireActor 찾아서 제거
    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors);

    for (AActor* Attached : AttachedActors)
    {
        AFireActor* Fire = Cast<AFireActor>(Attached);
        if (IsValid(Fire))
        {
            UE_LOG(LogGasTank, Error, TEXT("[GasTank] Found attached FireActor: %s"), *Fire->GetName());

            Fire->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

            ARoomActor* Room = Fire->LinkedRoom.Get();
            if (IsValid(Room))
            {
                Room->UnregisterFire(Fire->FireID);
            }

            if (IsValid(Fire->FirePsc))
            {
                Fire->FirePsc->DeactivateSystem();
                Fire->FirePsc->SetVisibility(false);
            }

            Fire->SetActorHiddenInGame(true);
            Fire->SetActorTickEnabled(false);
            Fire->Destroy();
        }
    }

    // 3) Combustible의 ActiveFire도 체크 (혹시 있으면)
    if (IsValid(Combustible))
    {
        AFireActor* Fire = Combustible->ActiveFire.Get();
        if (IsValid(Fire))
        {
            UE_LOG(LogGasTank, Error, TEXT("[GasTank] Found Combustible->ActiveFire: %s"), *Fire->GetName());

            Fire->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

            ARoomActor* Room = Fire->LinkedRoom.Get();
            if (IsValid(Room))
            {
                Room->UnregisterFire(Fire->FireID);
            }

            if (IsValid(Fire->FirePsc))
            {
                Fire->FirePsc->DeactivateSystem();
                Fire->FirePsc->SetVisibility(false);
            }

            Fire->SetActorHiddenInGame(true);
            Fire->SetActorTickEnabled(false);
            Fire->Destroy();
        }

        Combustible->bIsBurning = false;
        Combustible->ActiveFire = nullptr;
        Combustible->Fuel.FuelCurrent = 0.f;
    }

    // 4) 가스탱크 즉시 삭제
    Destroy();
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
