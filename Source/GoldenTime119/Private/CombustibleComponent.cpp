// ============================ CombustibleComponent.cpp ============================
#include "CombustibleComponent.h"

#include "RoomActor.h"
#include "FireActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogComb, Log, All);

UCombustibleComponent::UCombustibleComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UCombustibleComponent::BeginPlay()
{
    Super::BeginPlay();

    EnsureFuelInitialized();

    AActor* Owner = GetOwner();
    if (!IsValid(Owner)) return;

    USceneComponent* RootComp = Owner->GetRootComponent();
    if (!IsValid(RootComp)) return;

    EnsureComponentsCreated(Owner, RootComp);
    ApplyTemplatesAndSounds();

    SetComponentTickEnabled(true);
}

void UCombustibleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(SteamAudio) && SteamAudio->IsPlaying())
    {
        SteamAudio->Stop();
    }
    if (IsValid(SmolderAudio) && SmolderAudio->IsPlaying())
    {
        SmolderAudio->Stop();
    }

    if (EndPlayReason == EEndPlayReason::Destroyed)
    {
        if (IsValid(SteamAudio))
        {
            SteamAudio->DestroyComponent();
            SteamAudio = nullptr;
        }
        if (IsValid(SmolderAudio))
        {
            SmolderAudio->DestroyComponent();
            SmolderAudio = nullptr;
        }
        if (IsValid(SteamPsc))
        {
            SteamPsc->DeactivateSystem();
            SteamPsc->DestroyComponent();
            SteamPsc = nullptr;
        }
        if (IsValid(SmokePsc))
        {
            SmokePsc->DeactivateSystem();
            SmokePsc->DestroyComponent();
            SmokePsc = nullptr;
        }
    }

    Super::EndPlay(EndPlayReason);
}

void UCombustibleComponent::ApplyTemplatesAndSounds()
{
    if (IsValid(SmokePsc) && SmokeTemplate && SmokePsc->Template != SmokeTemplate)
    {
        SmokePsc->SetTemplate(SmokeTemplate);
    }

    if (IsValid(SteamPsc) && SteamTemplate && SteamPsc->Template != SteamTemplate)
    {
        SteamPsc->SetTemplate(SteamTemplate);
    }

    if (IsValid(SteamAudio) && SteamSound && SteamAudio->Sound != SteamSound)
    {
        SteamAudio->SetSound(SteamSound);
    }

    if (IsValid(SmolderAudio) && SmolderLoopSound && SmolderAudio->Sound != SmolderLoopSound)
    {
        SmolderAudio->SetSound(SmolderLoopSound);
    }
}

void UCombustibleComponent::EnsureComponentsCreated(AActor* Owner, USceneComponent* RootComp)
{
    // Smoke PSC
    if (!IsValid(SmokePsc))
    {
        SmokePsc = NewObject<UParticleSystemComponent>(Owner, UParticleSystemComponent::StaticClass(),
            FName(*FString::Printf(TEXT("SmokePSC_%d"), FMath::Rand())));
        if (SmokePsc)
        {
            SmokePsc->SetupAttachment(RootComp);
            SmokePsc->bAutoActivate = true;
            SmokePsc->RegisterComponent();
            SmokePsc->SetRelativeLocation(FVector::ZeroVector);
            if (SmokeTemplate)
            {
                SmokePsc->SetTemplate(SmokeTemplate);
            }
        }
    }

    // Steam PSC
    if (!IsValid(SteamPsc))
    {
        SteamPsc = NewObject<UParticleSystemComponent>(Owner, UParticleSystemComponent::StaticClass(),
            FName(*FString::Printf(TEXT("SteamPSC_%d"), FMath::Rand())));
        if (SteamPsc)
        {
            SteamPsc->SetupAttachment(RootComp);
            SteamPsc->bAutoActivate = false;
            SteamPsc->RegisterComponent();
            SteamPsc->SetRelativeLocation(FVector::ZeroVector);
            SteamPsc->DeactivateSystem();
            if (SteamTemplate)
            {
                SteamPsc->SetTemplate(SteamTemplate);
            }
        }
    }

    // Steam Audio
    if (!IsValid(SteamAudio))
    {
        SteamAudio = NewObject<UAudioComponent>(Owner, UAudioComponent::StaticClass(),
            FName(*FString::Printf(TEXT("SteamAudio_%d"), FMath::Rand())));
        if (SteamAudio)
        {
            SteamAudio->SetupAttachment(RootComp);
            SteamAudio->bAutoActivate = false;
            SteamAudio->bStopWhenOwnerDestroyed = true;
            SteamAudio->RegisterComponent();
            SteamAudio->SetRelativeLocation(FVector::ZeroVector);
            if (SteamSound)
            {
                SteamAudio->SetSound(SteamSound);
            }
        }
    }

    // Smolder Audio (훈소 루프)
    if (!IsValid(SmolderAudio))
    {
        SmolderAudio = NewObject<UAudioComponent>(Owner, UAudioComponent::StaticClass(),
            FName(*FString::Printf(TEXT("SmolderAudio_%d"), FMath::Rand())));
        if (SmolderAudio)
        {
            SmolderAudio->SetupAttachment(RootComp);
            SmolderAudio->bAutoActivate = false;
            SmolderAudio->bStopWhenOwnerDestroyed = true;
            SmolderAudio->RegisterComponent();
            SmolderAudio->SetRelativeLocation(FVector::ZeroVector);
            if (SmolderLoopSound)
            {
                SmolderAudio->SetSound(SmolderLoopSound);
            }
        }
    }
}

void UCombustibleComponent::OnComponentCreated()
{
    Super::OnComponentCreated();
    bComponentsNeedRecreation = false;
}

void UCombustibleComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    bComponentsNeedRecreation = true;
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UCombustibleComponent::SetOwningRoom(ARoomActor* InRoom)
{
    OwningRoom = InRoom;
}

void UCombustibleComponent::AddIgnitionPressure(const FGuid& /*SourceFireId*/, float Pressure)
{
    if (Pressure <= KINDA_SMALL_NUMBER) return;
    PendingPressure += Pressure;
}

void UCombustibleComponent::AddHeat(float HeatDelta)
{
    if (HeatDelta <= KINDA_SMALL_NUMBER) return;
    PendingHeat += HeatDelta;
}

void UCombustibleComponent::ConsumeFuel(float ConsumeAmount)
{
    if (ConsumeAmount <= 0.f) return;
    Fuel.FuelCurrent = FMath::Max(0.f, Fuel.FuelCurrent - ConsumeAmount);
}

bool UCombustibleComponent::CanIgniteNow() const
{
    if (IsBurning()) return false;
    if (CombustibleType == ECombustibleType::Electric && !bElectricIgnitionTriggered) return false;
    if (Fuel.FuelCurrent <= KINDA_SMALL_NUMBER) return false;
    if (!OwningRoom.IsValid()) return false;
    return true;
}

void UCombustibleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 레벨 전환 후 컴포넌트 재생성 필요 여부 체크
    if (bComponentsNeedRecreation)
    {
        AActor* Owner = GetOwner();
        if (IsValid(Owner) && IsValid(Owner->GetRootComponent()))
        {
            EnsureComponentsCreated(Owner, Owner->GetRootComponent());
            ApplyTemplatesAndSounds();
            bComponentsNeedRecreation = false;
        }
    }

    const bool bHasActivity =
        (PendingPressure > KINDA_SMALL_NUMBER) ||
        (PendingHeat > KINDA_SMALL_NUMBER) ||
        (PendingWater01 > KINDA_SMALL_NUMBER) ||
        IsBurning();

    if (!bHasActivity && Ignition.IgnitionProgress01 <= KINDA_SMALL_NUMBER)
    {
        // 훈소 사운드도 자연스럽게 꺼지게
        UpdateSmolderAudio(DeltaTime);
        return;
    }

    // 1) 점화 진행도 업데이트
    UpdateIgnitionProgress(DeltaTime);

    // 2) 물에 의한 냉각/진압 적용
    const bool bWasWaterActive = PendingWater01 > 0.05f;

    if (PendingWater01 > KINDA_SMALL_NUMBER)
    {
        Ignition.IgnitionProgress01 = FMath::Max(0.f,
            Ignition.IgnitionProgress01 - PendingWater01 * WaterCoolPerSec * DeltaTime
        );

        if (IsBurning())
        {
            ExtinguishAlpha01 = FMath::Clamp(
                ExtinguishAlpha01 + PendingWater01 * WaterExtinguishPerSec * DeltaTime,
                0.f, 1.f
            );
        }

        PendingWater01 = FMath::Max(0.f, PendingWater01 - WaterDecayPerSec * DeltaTime);
    }
    else
    {
        if (ExtinguishAlpha01 > KINDA_SMALL_NUMBER)
        {
            ExtinguishAlpha01 = FMath::Max(0.f, ExtinguishAlpha01 - 0.05f * DeltaTime);
        }
    }

    // 3) 수증기 사운드 (기존 one-shot-ish 흐름 유지)
    const bool bWaterHittingFire = IsBurning() && (PendingWater01 > 0.05f);

    if (IsValid(SteamAudio) && SteamSound)
    {
        if (bWaterHittingFire && !bWasWaterSoundPlaying)
        {
            if (!SteamAudio->IsPlaying())
            {
                SteamAudio->Play();
                bWasWaterSoundPlaying = true;
            }
        }
        else if (!bWaterHittingFire && bWasWaterSoundPlaying)
        {
            bWasWaterSoundPlaying = false;

            if (SteamAudio->IsPlaying())
            {
                SteamAudio->FadeOut(0.5f, 0.f);
            }
        }
    }

    // 4) 연기 알파
    const float TargetSmokeAlpha = FMath::GetMappedRangeValueClamped(
        FVector2D(SmokeStartProgress, SmokeFullProgress),
        FVector2D(0.f, 1.f),
        Ignition.IgnitionProgress01
    );

    SmokeAlpha01 = FMath::FInterpTo(SmokeAlpha01, TargetSmokeAlpha, DeltaTime, 2.0f);

    if (IsValid(SmokePsc))
    {
        SmokePsc->SetFloatParameter(TEXT("Smoke01"), SmokeAlpha01);
    }

    // 5) Steam VFX
    const float Steam01 = bWaterHittingFire ? FMath::Clamp(PendingWater01, 0.f, 1.f) : 0.f;

    if (IsValid(SteamPsc))
    {
        SteamPsc->SetFloatParameter(TEXT("Steam01"), Steam01);

        if (Steam01 <= 0.01f)
        {
            if (SteamPsc->IsActive())
            {
                SteamPsc->DeactivateSystem();
            }
        }
        else
        {
            if (!SteamPsc->IsActive())
            {
                SteamPsc->ActivateSystem(true);
            }
        }
    }

    // 6) 훈소 사운드(불꽃 전 단계)
    UpdateSmolderAudio(DeltaTime);

    // 7) 점화 시도
    TryIgnite();

    // 8) 소화 판정
    if (IsBurning() && (ExtinguishAlpha01 >= 1.f))
    {
        if (IsValid(ActiveFire))
        {
            ActiveFire->Extinguish();
        }
        ExtinguishAlpha01 = 0.f;
    }
}

void UCombustibleComponent::UpdateIgnitionProgress(float DeltaTime)
{
    const float PressureImpulse = PendingPressure;
    const float HeatImpulse = PendingHeat;

    PendingPressure = 0.f;
    PendingHeat = 0.f;

    const float InputImpulse = (PressureImpulse + HeatImpulse * 0.25f) * Ignition.Flammability;

    if (InputImpulse > KINDA_SMALL_NUMBER)
    {
        Ignition.IgnitionProgress01 = FMath::Clamp(
            Ignition.IgnitionProgress01 + InputImpulse * Ignition.IgnitionSpeed,
            0.f, 1.25f
        );
    }
    else if (Ignition.IgnitionProgress01 > KINDA_SMALL_NUMBER)
    {
        Ignition.IgnitionProgress01 = FMath::Max(0.f,
            Ignition.IgnitionProgress01 - Ignition.IgnitionDecayPerSec * DeltaTime
        );
    }
}

void UCombustibleComponent::TryIgnite()
{
    if (!CanIgniteNow()) return;
    if (Ignition.IgnitionProgress01 < Ignition.IgniteThreshold) return;

    OnIgnited();
}

void UCombustibleComponent::OnIgnited()
{
    ARoomActor* Room = OwningRoom.Get();
    if (!IsValid(Room)) return;

    AFireActor* NewFire = Room->SpawnFireForCombustible(this, CombustibleType);
    if (!IsValid(NewFire))
    {
        Ignition.IgnitionProgress01 = 0.5f;
        return;
    }

    ActiveFire = NewFire;
    bIsBurning = true;
    Ignition.IgnitionProgress01 = 0.f;

    // 불꽃이 생기면 훈소는 즉시 정리
    if (IsValid(SmolderAudio) && SmolderAudio->IsPlaying())
    {
        SmolderAudio->FadeOut(SmolderFadeOut, 0.f);
    }
}

void UCombustibleComponent::OnExtinguished()
{
    bIsBurning = false;
    ActiveFire = nullptr;
    bWasWaterSoundPlaying = false;
}

AFireActor* UCombustibleComponent::ForceIgnite(bool bAllowElectric)
{
    if (!CanIgniteNow()) return nullptr;
    if (IsBurning()) return ActiveFire;

    if (CombustibleType == ECombustibleType::Electric)
    {
        if (!bAllowElectric || !bElectricIgnitionTriggered)
            return nullptr;
    }

    ARoomActor* Room = OwningRoom.Get();
    if (!IsValid(Room)) return nullptr;

    AActor* OwnerActor = GetOwner();
    if (!IsValid(OwnerActor)) return nullptr;

    return Room->IgniteActor(OwnerActor);
}

void UCombustibleComponent::EnsureFuelInitialized()
{
    if (Fuel.FuelInitial <= 0.f)
    {
        Fuel.FuelInitial = 12.f;
    }

    Fuel.FuelCurrent = FMath::Clamp(Fuel.FuelCurrent, 0.f, Fuel.FuelInitial);

    if (Fuel.FuelCurrent <= 0.f)
    {
        Fuel.FuelCurrent = Fuel.FuelInitial;
    }
}

void UCombustibleComponent::AddWaterContact(float Amount01, bool /*bTriggerSound*/)
{
    PendingWater01 = FMath::Clamp(PendingWater01 + Amount01, 0.f, 1.5f);
}

void UCombustibleComponent::UpdateSmolderAudio(float /*DeltaTime*/)
{
    if (!IsValid(SmolderAudio) || !SmolderLoopSound)
        return;

    // 훈소 정의:
    // - 불꽃이 아직 없음
    // - 점화 진행도가 일정 이상
    const bool bSmoldering = (!IsBurning() && (Ignition.IgnitionProgress01 >= SmolderStartProgress));

    if (bSmoldering)
    {
        if (!SmolderAudio->IsPlaying())
        {
            SmolderAudio->Play();
            SmolderAudio->FadeIn(SmolderFadeIn, 1.f);
        }

        // 진행도에 따른 볼륨(원하면 삭제 가능)
        const float Den = FMath::Max(0.001f, (Ignition.IgniteThreshold - SmolderStartProgress));
        const float Vol = FMath::Clamp((Ignition.IgnitionProgress01 - SmolderStartProgress) / Den, 0.f, 1.f);
        SmolderAudio->SetVolumeMultiplier(Vol);
    }
    else
    {
        // 히스테리시스: 많이 내려갔거나, 불이 붙으면 종료
        const bool bShouldStop = IsBurning() || (Ignition.IgnitionProgress01 <= SmolderStopProgress);

        if (bShouldStop && SmolderAudio->IsPlaying())
        {
            SmolderAudio->FadeOut(SmolderFadeOut, 0.f);
        }
    }
}
