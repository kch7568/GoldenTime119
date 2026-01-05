// ============================ FireballActor.cpp ============================
#include "FireballActor.h"
#include "CombustibleComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogFireball, Log, All);

AFireballActor::AFireballActor()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    DamageSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DamageSphere"));
    DamageSphere->SetupAttachment(Root);
    DamageSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DamageSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
    DamageSphere->SetSphereRadius(100.f);

    FireballPSC = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("FireballPSC"));
    FireballPSC->SetupAttachment(Root);
    FireballPSC->bAutoActivate = false;

    ExplosionAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("ExplosionAudio"));
    ExplosionAudio->SetupAttachment(Root);
    ExplosionAudio->bAutoActivate = false;

    RoarAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("RoarAudio"));
    RoarAudio->SetupAttachment(Root);
    RoarAudio->bAutoActivate = false;
}

void AFireballActor::InitFireball(float InRadius, float InDuration, float InBlastIntensity)
{
    MaxRadius = FMath::Max(100.f, InRadius);
    TotalDuration = FMath::Max(1.f, InDuration);
    BlastIntensity = FMath::Max(0.1f, InBlastIntensity);

    ExpandEndTime = TotalDuration * ExpandPhaseRatio;
    RisingEndTime = TotalDuration * (ExpandPhaseRatio + RisingPhaseRatio);

    UE_LOG(LogFireball, Warning, TEXT("[Fireball] Init: Radius=%.1f Duration=%.1f Blast=%.1f"),
        MaxRadius, TotalDuration, BlastIntensity);
}

void AFireballActor::BeginPlay()
{
    Super::BeginPlay();

    StartLocation = GetActorLocation();
    CurrentRadius = MaxRadius * 0.1f;

    if (IsValid(FireballPSC) && FireballTemplate)
    {
        FireballPSC->SetTemplate(FireballTemplate);
        FireballPSC->ActivateSystem(true);
    }

    if (IsValid(ExplosionAudio) && ExplosionSound)
    {
        ExplosionAudio->SetSound(ExplosionSound);
        ExplosionAudio->Play();
    }

    if (IsValid(RoarAudio) && FireballRoarSound)
    {
        RoarAudio->SetSound(FireballRoarSound);
        RoarAudio->Play();
    }

    ApplyBlastWave();
    TryIgniteNearby();

    SetPhase(EFireballPhase::Expanding);

    UE_LOG(LogFireball, Warning, TEXT("[Fireball] BeginPlay at %s"), *StartLocation.ToString());
}

void AFireballActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    ElapsedTime += DeltaTime;

    switch (Phase)
    {
    case EFireballPhase::Expanding:
        UpdateExpanding(DeltaTime);
        break;
    case EFireballPhase::Rising:
        UpdateRising(DeltaTime);
        break;
    case EFireballPhase::Dissipating:
        UpdateDissipating(DeltaTime);
        break;
    case EFireballPhase::Finished:
        Destroy();
        return;
    }

    ApplyRadiationDamage(DeltaTime);
    UpdateVFX();

#if WITH_EDITOR
    DrawDebugSphere(GetWorld(), GetActorLocation(), CurrentRadius, 24, FColor::Orange, false, -1.f, 0, 3.f);
#endif
}

void AFireballActor::UpdateExpanding(float DeltaTime)
{
    const float ExpandProgress = FMath::Clamp(ElapsedTime / ExpandEndTime, 0.f, 1.f);
    const float EasedProgress = 1.f - FMath::Pow(1.f - ExpandProgress, 3.f);
    CurrentRadius = MaxRadius * EasedProgress;

    FVector NewLoc = StartLocation + FVector(0, 0, RiseSpeed * DeltaTime * 0.5f);
    SetActorLocation(NewLoc);

    if (ElapsedTime >= ExpandEndTime)
    {
        SetPhase(EFireballPhase::Rising);
    }
}

void AFireballActor::UpdateRising(float DeltaTime)
{
    const float RisingProgress = FMath::Clamp((ElapsedTime - ExpandEndTime) / (RisingEndTime - ExpandEndTime), 0.f, 1.f);
    CurrentRadius = MaxRadius * FMath::Lerp(1.f, 1.1f, RisingProgress);

    FVector CurrentLoc = GetActorLocation();
    FVector NewLoc = CurrentLoc + FVector(0, 0, RiseSpeed * DeltaTime);
    SetActorLocation(NewLoc);

    if (ElapsedTime >= RisingEndTime)
    {
        SetPhase(EFireballPhase::Dissipating);
    }
}

void AFireballActor::UpdateDissipating(float DeltaTime)
{
    const float DissipProgress = FMath::Clamp((ElapsedTime - RisingEndTime) / (TotalDuration - RisingEndTime), 0.f, 1.f);
    CurrentRadius = MaxRadius * FMath::Lerp(1.1f, 0.1f, DissipProgress);

    FVector CurrentLoc = GetActorLocation();
    FVector NewLoc = CurrentLoc + FVector(0, 0, RiseSpeed * DeltaTime * (1.f - DissipProgress) * 0.3f);
    SetActorLocation(NewLoc);

    if (ElapsedTime >= TotalDuration)
    {
        SetPhase(EFireballPhase::Finished);
    }
}

void AFireballActor::ApplyBlastWave()
{
    const float BlastRange = MaxRadius * BlastDamageRangeMultiplier;

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    GetWorld()->OverlapMultiByChannel(
        Overlaps,
        GetActorLocation(),
        FQuat::Identity,
        ECC_WorldDynamic,
        FCollisionShape::MakeSphere(BlastRange),
        Params
    );

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* HitActor = Overlap.GetActor();
        if (!IsValid(HitActor)) continue;
        if (DamagedActors.Contains(HitActor)) continue;

        const float Dist = FVector::Dist(GetActorLocation(), HitActor->GetActorLocation());
        const float DistAlpha = 1.f - FMath::Clamp(Dist / BlastRange, 0.f, 1.f);
        const float Damage = BaseBlastDamage * BlastIntensity * DistAlpha;

        if (Damage > 0.f)
        {
            UGameplayStatics::ApplyDamage(HitActor, Damage, nullptr, this, nullptr);
            DamagedActors.Add(HitActor);
            OnFireballDamage.Broadcast(HitActor, Damage);

            UE_LOG(LogFireball, Log, TEXT("[Fireball] BlastDamage to %s: %.1f"), *GetNameSafe(HitActor), Damage);
        }
    }

    UE_LOG(LogFireball, Warning, TEXT("[Fireball] BlastWave applied to %d actors"), DamagedActors.Num());
}

void AFireballActor::ApplyRadiationDamage(float DeltaTime)
{
    if (Phase == EFireballPhase::Finished)
        return;

    const float RadRange = CurrentRadius * RadiationDamageRangeMultiplier;

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    GetWorld()->OverlapMultiByChannel(
        Overlaps,
        GetActorLocation(),
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(RadRange),
        Params
    );

    float RadiationIntensity = 1.f;
    if (Phase == EFireballPhase::Expanding)
        RadiationIntensity = 1.2f;
    else if (Phase == EFireballPhase::Dissipating)
        RadiationIntensity = FMath::Lerp(1.f, 0.2f, (ElapsedTime - RisingEndTime) / (TotalDuration - RisingEndTime));

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* HitActor = Overlap.GetActor();
        if (!IsValid(HitActor)) continue;

        const float Dist = FVector::Dist(GetActorLocation(), HitActor->GetActorLocation());
        const float DistAlpha = 1.f - FMath::Clamp(Dist / RadRange, 0.f, 1.f);
        const float DamagePerSec = BaseRadiationDamage * RadiationIntensity * DistAlpha;
        const float Damage = DamagePerSec * DeltaTime;

        if (Damage > 0.1f)
        {
            UGameplayStatics::ApplyDamage(HitActor, Damage, nullptr, this, nullptr);
        }
    }
}

void AFireballActor::TryIgniteNearby()
{
    const float IgniteRange = MaxRadius * 1.5f;

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    GetWorld()->OverlapMultiByChannel(
        Overlaps,
        GetActorLocation(),
        FQuat::Identity,
        ECC_WorldDynamic,
        FCollisionShape::MakeSphere(IgniteRange),
        Params
    );

    int32 IgnitedCount = 0;

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* HitActor = Overlap.GetActor();
        if (!IsValid(HitActor)) continue;

        UCombustibleComponent* Comb = HitActor->FindComponentByClass<UCombustibleComponent>();
        if (!IsValid(Comb)) continue;
        if (Comb->IsBurning()) continue;

        const float Dist = FVector::Dist(GetActorLocation(), HitActor->GetActorLocation());
        const float DistAlpha = 1.f - FMath::Clamp(Dist / IgniteRange, 0.f, 1.f);
        const float ChanceThisActor = IgnitionChance * DistAlpha;

        if (FMath::FRand() < ChanceThisActor)
        {
            Comb->Ignition.IgnitionProgress01 = Comb->Ignition.IgniteThreshold + 0.1f;
            IgnitedCount++;

            UE_LOG(LogFireball, Log, TEXT("[Fireball] Ignited %s (Chance=%.2f)"), *GetNameSafe(HitActor), ChanceThisActor);
        }
        else
        {
            Comb->AddIgnitionPressure(FGuid(), DistAlpha * 0.5f);
        }
    }

    if (IgnitedCount > 0)
    {
        UE_LOG(LogFireball, Warning, TEXT("[Fireball] Ignited %d nearby combustibles"), IgnitedCount);
    }
}

void AFireballActor::UpdateVFX()
{
    const float ScaleFactor = CurrentRadius / 100.f;

    if (IsValid(FireballPSC))
    {
        FireballPSC->SetWorldScale3D(FVector(ScaleFactor));

        float Intensity = 1.f;
        if (Phase == EFireballPhase::Dissipating)
        {
            Intensity = FMath::Lerp(1.f, 0.f, (ElapsedTime - RisingEndTime) / (TotalDuration - RisingEndTime));
        }
        FireballPSC->SetFloatParameter(TEXT("Intensity"), Intensity);
    }

    if (IsValid(DamageSphere))
    {
        DamageSphere->SetSphereRadius(CurrentRadius);
    }

    if (IsValid(RoarAudio))
    {
        float Volume = 1.f;
        if (Phase == EFireballPhase::Dissipating)
        {
            Volume = FMath::Lerp(1.f, 0.f, (ElapsedTime - RisingEndTime) / (TotalDuration - RisingEndTime));
        }
        RoarAudio->SetVolumeMultiplier(Volume);
    }
}

void AFireballActor::SetPhase(EFireballPhase NewPhase)
{
    EFireballPhase OldPhase = Phase;
    Phase = NewPhase;

    UE_LOG(LogFireball, Log, TEXT("[Fireball] Phase: %d -> %d (Time=%.2f Radius=%.1f)"),
        (int32)OldPhase, (int32)NewPhase, ElapsedTime, CurrentRadius);

    if (NewPhase == EFireballPhase::Dissipating)
    {
        if (IsValid(RoarAudio))
        {
            RoarAudio->FadeOut(TotalDuration - RisingEndTime, 0.f);
        }
    }
    else if (NewPhase == EFireballPhase::Finished)
    {
        if (IsValid(FireballPSC))
        {
            FireballPSC->DeactivateSystem();
        }
    }
}