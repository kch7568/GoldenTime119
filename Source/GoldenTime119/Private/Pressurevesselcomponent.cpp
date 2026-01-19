// ============================ PressureVesselComponent.cpp ============================
#include "PressureVesselComponent.h"
#include "CombustibleComponent.h"
#include "FireballActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogPressureVessel, Log, All);

UPressureVesselComponent::UPressureVesselComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UPressureVesselComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    if (IsValid(Owner))
    {
        UCombustibleComponent* Comb = Owner->FindComponentByClass<UCombustibleComponent>();
        if (IsValid(Comb))
        {
            LinkedCombustible = Comb;
            UE_LOG(LogPressureVessel, Warning, TEXT("[Vessel] LinkedCombustible found: %s"), *GetNameSafe(Owner));
        }
        else
        {
            UE_LOG(LogPressureVessel, Error, TEXT("[Vessel] No CombustibleComponent found on %s"), *GetNameSafe(Owner));
        }
    }

    EnsureComponentsCreated();

    InternalPressure = BasePressure;
    InternalTemperature = 25.0f;
    WallTemperature = 25.0f;

    UE_LOG(LogPressureVessel, Warning, TEXT("[Vessel] BeginPlay Owner=%s Capacity=%.1fL Fill=%.1f%%"),
        *GetNameSafe(Owner), VesselCapacityLiters, LiquidFillLevel01 * 100.f);
}

void UPressureVesselComponent::EnsureComponentsCreated()
{
    AActor* Owner = GetOwner();
    if (!IsValid(Owner)) return;

    USceneComponent* RootComp = Owner->GetRootComponent();
    if (!IsValid(RootComp)) return;

    if (!IsValid(SafetyValvePSC))
    {
        SafetyValvePSC = NewObject<UParticleSystemComponent>(Owner, UParticleSystemComponent::StaticClass(),
            FName(TEXT("SafetyValvePSC")));
        if (SafetyValvePSC)
        {
            SafetyValvePSC->SetupAttachment(RootComp);
            SafetyValvePSC->bAutoActivate = false;
            SafetyValvePSC->RegisterComponent();
            SafetyValvePSC->SetRelativeLocation(FVector(0, 0, 50.f));
            if (SafetyValveVentTemplate)
            {
                SafetyValvePSC->SetTemplate(SafetyValveVentTemplate);
            }
        }
    }

    if (!IsValid(SafetyValveAudioComp))
    {
        SafetyValveAudioComp = NewObject<UAudioComponent>(Owner, UAudioComponent::StaticClass(),
            FName(TEXT("SafetyValveAudio")));
        if (SafetyValveAudioComp)
        {
            SafetyValveAudioComp->SetupAttachment(RootComp);
            SafetyValveAudioComp->bAutoActivate = false;
            SafetyValveAudioComp->RegisterComponent();
            if (SafetyValveSound)
            {
                SafetyValveAudioComp->SetSound(SafetyValveSound);
            }
            // 초기 피치는 최소값으로 설정
            SafetyValveAudioComp->SetPitchMultiplier(SafetyValvePitchMin);
        }
    }

    if (!IsValid(CriticalWarningAudioComp))
    {
        CriticalWarningAudioComp = NewObject<UAudioComponent>(Owner, UAudioComponent::StaticClass(),
            FName(TEXT("CriticalWarningAudio")));
        if (CriticalWarningAudioComp)
        {
            CriticalWarningAudioComp->SetupAttachment(RootComp);
            CriticalWarningAudioComp->bAutoActivate = false;
            CriticalWarningAudioComp->RegisterComponent();
            if (CriticalWarningSound)
            {
                CriticalWarningAudioComp->SetSound(CriticalWarningSound);
            }
        }
    }
}

void UPressureVesselComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (VesselState == EPressureVesselState::Ruptured)
        return;

    DetectHeatFromCombustible(DeltaTime);
    UpdatePressureAndTemperature(DeltaTime);
    UpdateSafetyValve(DeltaTime);
    UpdateVesselState();
    CheckBLEVECondition();
}

void UPressureVesselComponent::DetectHeatFromCombustible(float DeltaTime)
{
    if (!IsValid(LinkedCombustible))
        return;

    float HeatInput = 0.f;

    const float IgnitionProgress = LinkedCombustible->Ignition.IgnitionProgress01;
    HeatInput += IgnitionProgress * HeatPerIgnitionProgress;

    if (LinkedCombustible->IsBurning())
    {
        HeatInput += BurningHeatBonus;
    }

    if (HeatInput > 0.f)
    {
        UE_LOG(LogPressureVessel, Verbose, TEXT("[Vessel] HeatInput=%.2f (Progress=%.2f, Burning=%d)"),
            HeatInput, IgnitionProgress, LinkedCombustible->IsBurning());
    }

    if (HeatInput > KINDA_SMALL_NUMBER)
    {
        AccumulatedHeat += HeatInput * ExternalHeatMultiplier * DeltaTime;
        LastHeatInputTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

        if (HeatingStartTime < 0.f)
        {
            HeatingStartTime = LastHeatInputTime;
            UE_LOG(LogPressureVessel, Warning, TEXT("[Vessel] Heating started!"));
        }
    }
}

void UPressureVesselComponent::UpdatePressureAndTemperature(float DeltaTime)
{
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    const bool bIsBeingHeated = (CurrentTime - LastHeatInputTime) < 1.0f;

    if (bIsBeingHeated && AccumulatedHeat > 0.f)
    {
        const float HeatThisFrame = AccumulatedHeat * DeltaTime * 0.1f;
        AccumulatedHeat *= 0.95f;

        WallTemperature += HeatThisFrame * TempRisePerHeat * (1.f - LiquidFillLevel01 * 0.3f);

        const float LiquidCooling = LiquidFillLevel01 * LiquidCoolingFactor;
        InternalTemperature += HeatThisFrame * TempRisePerHeat * (1.f - LiquidCooling);

        const float TempAboveBase = FMath::Max(0.f, InternalTemperature - 25.f);
        const float PressureFromTemp = TempAboveBase * PressureRisePerDegree;

        InternalPressure = BasePressure + PressureFromTemp;

        if (WallTemperature > WallWeakeningTemp)
        {
            const float WeakenRatio = FMath::Clamp((WallTemperature - WallWeakeningTemp) / 200.f, 0.f, 0.5f);
            BurstPressure = FMath::Max(15.f, BaseBurstPressure * VesselStrength * (1.f - WeakenRatio));
        }
    }
    else
    {
        WallTemperature = FMath::FInterpTo(WallTemperature, 25.f, DeltaTime, 0.02f);
        InternalTemperature = FMath::FInterpTo(InternalTemperature, 25.f, DeltaTime, 0.01f);
        InternalPressure = FMath::FInterpTo(InternalPressure, BasePressure, DeltaTime, 0.05f);
    }

    WallTemperature = FMath::Clamp(WallTemperature, -50.f, 800.f);
    InternalTemperature = FMath::Clamp(InternalTemperature, -50.f, 500.f);
    InternalPressure = FMath::Clamp(InternalPressure, 0.f, 50.f);
}

void UPressureVesselComponent::UpdateSafetyValve(float DeltaTime)
{
    const bool bShouldVent = !bSafetyValveFailed && (InternalPressure >= SafetyValveActivationPressure);

    if (bShouldVent)
    {
        const float VentAmount = SafetyValveReleaseRate * DeltaTime;
        InternalPressure = FMath::Max(SafetyValveActivationPressure * 0.9f, InternalPressure - VentAmount);

        SafetyValveVentStrength01 = FMath::Clamp(
            (InternalPressure - SafetyValveActivationPressure) / (BurstPressure - SafetyValveActivationPressure),
            0.f, 1.f
        );

        // 파티클
        if (IsValid(SafetyValvePSC))
        {
            if (!SafetyValvePSC->IsActive())
            {
                SafetyValvePSC->ActivateSystem(true);
            }
            SafetyValvePSC->SetFloatParameter(TEXT("VentStrength"), SafetyValveVentStrength01);
        }

        // 오디오: 가스 누출 루프 & 피치 업데이트
        if (IsValid(SafetyValveAudioComp))
        {
            if (!SafetyValveAudioComp->IsPlaying())
            {
                SafetyValveAudioComp->Play();
            }

            // 전체 압력 비율 0~1 (BLEVE 직전 1.0)
            const float PressureRatio = GetPressureRatio01();

            // 기본 선형 피치
            float TargetPitch = FMath::Lerp(SafetyValvePitchMin, SafetyValvePitchMax, PressureRatio);

            // RapidIncreaseStartRatio 이후에는 지수 가중으로 급상승
            if (PressureRatio > SafetyValveRapidIncreaseStartRatio)
            {
                const float ExtraAlpha = (PressureRatio - SafetyValveRapidIncreaseStartRatio) /
                    (1.0f - SafetyValveRapidIncreaseStartRatio);
                const float Boost = FMath::Clamp(FMath::Pow(ExtraAlpha, 2.0f), 0.0f, 1.0f);
                TargetPitch = FMath::Lerp(TargetPitch, SafetyValvePitchMax, Boost);
            }

            const float CurrentPitch = SafetyValveAudioComp->PitchMultiplier;
            const float NewPitch = FMath::FInterpTo(
                CurrentPitch,
                TargetPitch,
                DeltaTime,
                SafetyValvePitchInterpSpeed
            );

            SafetyValveAudioComp->SetPitchMultiplier(NewPitch);
        }

        OnSafetyValveVenting.Broadcast(SafetyValveVentStrength01);
    }
    else
    {
        SafetyValveVentStrength01 = 0.f;

        if (IsValid(SafetyValvePSC) && SafetyValvePSC->IsActive())
        {
            SafetyValvePSC->DeactivateSystem();
        }

        if (IsValid(SafetyValveAudioComp) && SafetyValveAudioComp->IsPlaying())
        {
            SafetyValveAudioComp->FadeOut(0.25f, 0.f);
        }
    }
}

void UPressureVesselComponent::UpdateVesselState()
{
    if (VesselState == EPressureVesselState::Ruptured)
        return;

    EPressureVesselState NewState = VesselState;

    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    const bool bIsBeingHeated = (CurrentTime - LastHeatInputTime) < 1.0f;

    if (InternalPressure >= CriticalPressure)
    {
        NewState = EPressureVesselState::Critical;
    }
    else if (SafetyValveVentStrength01 > 0.1f)
    {
        NewState = EPressureVesselState::Venting;
    }
    else if (bIsBeingHeated && WallTemperature > 100.f)
    {
        NewState = EPressureVesselState::Heating;
    }
    else
    {
        NewState = EPressureVesselState::Normal;
    }

    if (NewState != VesselState)
    {
        SetVesselState(NewState);
    }

    if (VesselState == EPressureVesselState::Critical)
    {
        if (IsValid(CriticalWarningAudioComp) && !CriticalWarningAudioComp->IsPlaying())
        {
            CriticalWarningAudioComp->Play();
        }
    }
    else
    {
        if (IsValid(CriticalWarningAudioComp) && CriticalWarningAudioComp->IsPlaying())
        {
            CriticalWarningAudioComp->Stop();
        }
    }
}

void UPressureVesselComponent::SetVesselState(EPressureVesselState NewState)
{
    EPressureVesselState OldState = VesselState;
    VesselState = NewState;

    UE_LOG(LogPressureVessel, Warning, TEXT("[Vessel] State: %d -> %d (Pressure=%.1f bar, WallTemp=%.1f C, InternalTemp=%.1f C)"),
        (int32)OldState, (int32)NewState, InternalPressure, WallTemperature, InternalTemperature);

    OnVesselStateChanged.Broadcast(NewState);
}

void UPressureVesselComponent::CheckBLEVECondition()
{
    if (VesselState == EPressureVesselState::Ruptured)
        return;

    bool bShouldRupture = false;

    if (InternalPressure >= BurstPressure)
    {
        bShouldRupture = true;
        UE_LOG(LogPressureVessel, Warning, TEXT("[Vessel] BLEVE Trigger: Pressure (%.1f >= %.1f)"),
            InternalPressure, BurstPressure);
    }
    else if (WallTemperature >= WallWeakeningTemp && InternalPressure >= CriticalPressure)
    {
        const float WeakenedBurst = BurstPressure * (WallTemperature - WallWeakeningTemp >= 0.f
            ? (1.f - (WallTemperature - WallWeakeningTemp) / 400.f)
            : 1.f);

        if (InternalPressure >= WeakenedBurst)
        {
            bShouldRupture = true;
            UE_LOG(LogPressureVessel, Warning, TEXT("[Vessel] BLEVE Trigger: Wall Weakened (WallTemp=%.1f, WeakenedBurst=%.1f)"),
                WallTemperature, WeakenedBurst);
        }
    }

    if (bShouldRupture)
    {
        ExecuteBLEVE();
    }
}

void UPressureVesselComponent::ExecuteBLEVE()
{
    SetVesselState(EPressureVesselState::Ruptured);

    AActor* Owner = GetOwner();
    if (!IsValid(Owner)) return;

    const FVector ExplosionLoc = Owner->GetActorLocation();

    UE_LOG(LogPressureVessel, Error, TEXT("[Vessel] ====== BLEVE EXPLOSION ====== Location=%s Pressure=%.1f WallTemp=%.1f"),
        *ExplosionLoc.ToString(), InternalPressure, WallTemperature);

    // 폭발 원샷 사운드
    if (ExplosionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, ExplosionLoc);
    }

    const float FuelMassKg = VesselCapacityLiters * LiquidFillLevel01 * 0.5f;
    const float FireballDiameter = 5.8f * FMath::Pow(FuelMassKg, 0.333f) * FireballRadiusMultiplier;
    const float FireballRadius = FireballDiameter * 50.f;
    const float FireballDuration = 0.45f * FMath::Pow(FuelMassKg, 0.333f);

    UE_LOG(LogPressureVessel, Warning, TEXT("[Vessel] Fireball: Diameter=%.1fm Duration=%.1fs FuelMass=%.1fkg"),
        FireballDiameter, FireballDuration, FuelMassKg);

    if (FireballClass && GetWorld())
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AFireballActor* Fireball = GetWorld()->SpawnActor<AFireballActor>(
            FireballClass, ExplosionLoc, FRotator::ZeroRotator, SpawnParams);

        if (IsValid(Fireball))
        {
            Fireball->InitFireball(FireballRadius, FireballDuration, BlastWaveIntensity);
        }
    }

    OnBLEVE.Broadcast(ExplosionLoc);

    if (IsValid(SafetyValvePSC))
        SafetyValvePSC->DeactivateSystem();
    if (IsValid(SafetyValveAudioComp))
        SafetyValveAudioComp->Stop();
    if (IsValid(CriticalWarningAudioComp))
        CriticalWarningAudioComp->Stop();
}

bool UPressureVesselComponent::IsBLEVEPossible() const
{
    if (VesselState == EPressureVesselState::Ruptured)
        return false;

    return (LiquidFillLevel01 >= 0.4f) && (WallTemperature > 100.f);
}

float UPressureVesselComponent::GetPressureRatio01() const
{
    return FMath::Clamp(InternalPressure / BurstPressure, 0.f, 1.f);
}

float UPressureVesselComponent::GetTimeToRupture() const
{
    if (VesselState == EPressureVesselState::Ruptured)
        return 0.f;

    if (InternalPressure <= SafetyValveActivationPressure)
        return -1.f;

    const float PressureGap = BurstPressure - InternalPressure;
    const float RiseRate = PressureRisePerDegree * TempRisePerHeat * 10.f;

    if (RiseRate <= 0.f)
        return -1.f;

    return PressureGap / RiseRate;
}

void UPressureVesselComponent::ForceRupture()
{
    InternalPressure = BurstPressure + 1.f;
    CheckBLEVECondition();
}
