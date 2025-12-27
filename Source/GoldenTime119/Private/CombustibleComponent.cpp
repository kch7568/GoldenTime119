#include "CombustibleComponent.h"

#include "RoomActor.h"
#include "FireActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

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

    const FVector OwnerLocation = Owner->GetActorLocation();

    UE_LOG(LogTemp, Error, TEXT("[Comb] === BeginPlay START === Owner=%s"), *GetNameSafe(Owner));
    UE_LOG(LogTemp, Error, TEXT("[Comb] BEFORE - SteamTemplate=%s, SteamSound=%s"),
        *GetNameSafe(SteamTemplate), *GetNameSafe(SteamSound));

    EnsureComponentsCreated(Owner, RootComp);
    ApplyTemplatesAndSounds();

    UE_LOG(LogTemp, Error, TEXT("[Comb] AFTER - SteamPsc=%s, SteamAudio=%s"),
        *GetNameSafe(SteamPsc), *GetNameSafe(SteamAudio));

    if (IsValid(SteamPsc))
    {
        UE_LOG(LogTemp, Error, TEXT("[Comb] SteamPsc->Template=%s"), *GetNameSafe(SteamPsc->Template));
    }
    if (IsValid(SteamAudio))
    {
        UE_LOG(LogTemp, Error, TEXT("[Comb] SteamAudio->Sound=%s"), *GetNameSafe(SteamAudio->Sound));
    }

    if (!Owner->GetActorLocation().Equals(OwnerLocation, 1.f))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Comb] Owner location shifted! Restoring..."));
        Owner->SetActorLocation(OwnerLocation);
    }

    SetComponentTickEnabled(true);
}

void UCombustibleComponent::ApplyTemplatesAndSounds()
{
    if (IsValid(SmokePsc) && SmokeTemplate && SmokePsc->Template != SmokeTemplate)
    {
        SmokePsc->SetTemplate(SmokeTemplate);
        UE_LOG(LogTemp, Warning, TEXT("[Comb] SmokeTemplate applied: %s"), *GetNameSafe(SmokeTemplate));
    }

    if (IsValid(SteamPsc) && SteamTemplate && SteamPsc->Template != SteamTemplate)
    {
        SteamPsc->SetTemplate(SteamTemplate);
        UE_LOG(LogTemp, Warning, TEXT("[Comb] SteamTemplate applied: %s"), *GetNameSafe(SteamTemplate));
    }

    if (IsValid(SteamAudio) && SteamSound && SteamAudio->Sound != SteamSound)
    {
        SteamAudio->SetSound(SteamSound);
        UE_LOG(LogTemp, Warning, TEXT("[Comb] SteamSound applied: %s"), *GetNameSafe(SteamSound));
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
}

void UCombustibleComponent::OnComponentCreated()
{
    Super::OnComponentCreated();

    // 컴포넌트가 처음 생성될 때 호출됨
    bComponentsNeedRecreation = false;
}

void UCombustibleComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    // 레벨 전환 시 컴포넌트가 파괴되면 플래그 설정
    bComponentsNeedRecreation = true;

    Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UCombustibleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 사운드 정리
    if (IsValid(SteamAudio) && SteamAudio->IsPlaying())
    {
        SteamAudio->Stop();
    }

    // 레벨 전환이 아닌 경우에만 완전히 파괴
    if (EndPlayReason == EEndPlayReason::Destroyed)
    {
        if (IsValid(SteamAudio))
        {
            SteamAudio->DestroyComponent();
            SteamAudio = nullptr;
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

void UCombustibleComponent::SetOwningRoom(ARoomActor* InRoom)
{
    OwningRoom = InRoom;
}

void UCombustibleComponent::AddIgnitionPressure(const FGuid& SourceFireId, float Pressure)
{
    if (Pressure <= KINDA_SMALL_NUMBER) return;

    PendingPressure += Pressure;

    if (const UWorld* World = GetWorld())
    {
        LastInputTime = World->GetTimeSeconds();
    }
}

void UCombustibleComponent::AddHeat(float HeatDelta)
{
    if (HeatDelta <= KINDA_SMALL_NUMBER) return;

    PendingHeat += HeatDelta;

    if (const UWorld* World = GetWorld())
    {
        LastInputTime = World->GetTimeSeconds();
    }
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

    // 최적화: 활동이 없으면 조기 종료
    const bool bHasActivity = (PendingPressure > KINDA_SMALL_NUMBER) ||
        (PendingHeat > KINDA_SMALL_NUMBER) ||
        (PendingWater01 > KINDA_SMALL_NUMBER) ||
        IsBurning();

    if (!bHasActivity && Ignition.IgnitionProgress01 <= KINDA_SMALL_NUMBER)
    {
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

    // 3) 수증기 사운드 관리 - ONE SHOT 방식
    const bool bWaterHittingFire = IsBurning() && (PendingWater01 > 0.05f);


    if (PendingWater01 > 0.01f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Steam] DEBUG: Owner=%s IsBurning=%d PendingWater=%.3f bWaterHittingFire=%d"),
            *GetNameSafe(GetOwner()), IsBurning(), PendingWater01, bWaterHittingFire);
        UE_LOG(LogTemp, Warning, TEXT("[Steam] DEBUG: SteamPsc Valid=%d, SteamAudio Valid=%d, SteamSound Valid=%d"),
            IsValid(SteamPsc), IsValid(SteamAudio), SteamSound != nullptr);
    }

    if (IsValid(SteamAudio) && SteamSound)
    {
        // 물이 처음 닿기 시작했을 때만 재생
        if (bWaterHittingFire && !bWasWaterSoundPlaying)
        {
            if (!SteamAudio->IsPlaying())
            {
                SteamAudio->Play();
                bWasWaterSoundPlaying = true;
                UE_LOG(LogTemp, Warning, TEXT("[Steam] Sound STARTED (ONE-SHOT) on %s"), *GetNameSafe(GetOwner()));
            }
        }
        // 물이 완전히 끊기면 플래그 리셋
        else if (!bWaterHittingFire && bWasWaterSoundPlaying)
        {
            bWasWaterSoundPlaying = false;

            // 사운드가 아직 재생 중이면 페이드아웃
            if (SteamAudio->IsPlaying())
            {
                SteamAudio->FadeOut(0.5f, 0.f);
                UE_LOG(LogTemp, Warning, TEXT("[Steam] Sound FADING OUT on %s"), *GetNameSafe(GetOwner()));
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
                UE_LOG(LogTemp, Warning, TEXT("[Steam] VFX ACTIVATED! Steam01=%.2f"), Steam01);
            }
        }
    }

    // 6) 점화 시도
    TryIgnite();

    // 7) 소화 판정
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

    UE_LOG(LogComb, Warning, TEXT("[Comb] IGNITE Owner=%s Room=%s Type=%d"),
        *GetNameSafe(GetOwner()), *GetNameSafe(Room), static_cast<int32>(CombustibleType));

    AFireActor* NewFire = Room->SpawnFireForCombustible(this, CombustibleType);
    if (!IsValid(NewFire))
    {
        Ignition.IgnitionProgress01 = 0.5f;
        return;
    }

    ActiveFire = NewFire;
    bIsBurning = true;
    Ignition.IgnitionProgress01 = 0.f;
}

void UCombustibleComponent::OnExtinguished()
{
    bIsBurning = false;
    ActiveFire = nullptr;

    // 소화 시 사운드 플래그도 리셋
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

void UCombustibleComponent::AddWaterContact(float Amount01, bool bTriggerSound)
{
    PendingWater01 = FMath::Clamp(PendingWater01 + Amount01, 0.f, 1.5f);
}
