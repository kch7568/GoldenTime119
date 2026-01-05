// ============================ FireballActor.h ============================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "FireballActor.generated.h"

class UCombustibleComponent;

UENUM(BlueprintType)
enum class EFireballPhase : uint8
{
    Expanding,
    Rising,
    Dissipating,
    Finished
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFireballDamage, AActor*, DamagedActor, float, DamageAmount);

UCLASS()
class GOLDENTIME119_API AFireballActor : public AActor
{
    GENERATED_BODY()

public:
    AFireballActor();

    void InitFireball(float InRadius, float InDuration, float InBlastIntensity);

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fireball")
    float MaxRadius = 500.f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fireball")
    float TotalDuration = 5.f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fireball")
    float BlastIntensity = 1.f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fireball|Runtime")
    float CurrentRadius = 0.f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fireball|Runtime")
    EFireballPhase Phase = EFireballPhase::Expanding;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fireball|Timing")
    float ExpandPhaseRatio = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fireball|Timing")
    float RisingPhaseRatio = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fireball|Movement")
    float RiseSpeed = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fireball|Damage")
    float RadiationDamageRangeMultiplier = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fireball|Damage")
    float BaseRadiationDamage = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fireball|Damage")
    float BlastDamageRangeMultiplier = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fireball|Damage")
    float BaseBlastDamage = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fireball|Damage")
    float IgnitionChance = 0.8f;

    UPROPERTY(EditAnywhere, Category = "Fireball|VFX")
    TObjectPtr<UParticleSystem> FireballTemplate;

    UPROPERTY(EditAnywhere, Category = "Fireball|Audio")
    TObjectPtr<USoundBase> ExplosionSound;

    UPROPERTY(EditAnywhere, Category = "Fireball|Audio")
    TObjectPtr<USoundBase> FireballRoarSound;

    UPROPERTY(BlueprintAssignable, Category = "Fireball|Events")
    FOnFireballDamage OnFireballDamage;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USphereComponent> DamageSphere;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UParticleSystemComponent> FireballPSC;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UAudioComponent> ExplosionAudio;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UAudioComponent> RoarAudio;

    float ElapsedTime = 0.f;
    float ExpandEndTime = 0.f;
    float RisingEndTime = 0.f;

    FVector StartLocation;

    UPROPERTY()
    TSet<AActor*> DamagedActors;

    void UpdateExpanding(float DeltaTime);
    void UpdateRising(float DeltaTime);
    void UpdateDissipating(float DeltaTime);

    void ApplyBlastWave();
    void ApplyRadiationDamage(float DeltaTime);
    void TryIgniteNearby();

    void UpdateVFX();
    void SetPhase(EFireballPhase NewPhase);
};