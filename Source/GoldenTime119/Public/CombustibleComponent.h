// ============================ CombustibleComponent.h ============================
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombustibleType.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "CombustibleComponent.generated.h"

class ARoomActor;
class AFireActor;

USTRUCT(BlueprintType)
struct FCombustibleIgnitionParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ignition") float IgnitionProgress01 = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ignition") float IgnitionSpeed = 0.55f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ignition") float IgnitionDecayPerSec = 0.08f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ignition") float IgniteThreshold = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ignition") float Flammability = 1.0f;
};

USTRUCT(BlueprintType)
struct FCombustibleFuelParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel") float FuelInitial = 12.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel") float FuelCurrent = 12.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel") float FuelConsumeMul = 1.0f;

    FORCEINLINE float FuelRatio01_Cpp() const
    {
        return (FuelInitial > 0.f) ? FMath::Clamp(FuelCurrent / FuelInitial, 0.f, 1.f) : 0.f;
    }
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GOLDENTIME119_API UCombustibleComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCombustibleComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combustible")
    ECombustibleType CombustibleType = ECombustibleType::Normal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combustible|Electric")
    bool bElectricIgnitionTriggered = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combustible|Electric")
    FName ElectricNetId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combustible|Ignition")
    FCombustibleIgnitionParams Ignition;

    UFUNCTION(BlueprintCallable, Category = "Combustible|Fuel")
    void EnsureFuelInitialized();

    // ===== VFX =====
    UPROPERTY(VisibleAnywhere, Category = "VFX")
    TObjectPtr<UParticleSystemComponent> SmokePsc = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "VFX")
    TObjectPtr<UParticleSystemComponent> SteamPsc = nullptr;

    UPROPERTY(EditAnywhere, Category = "VFX")
    TObjectPtr<UParticleSystem> SmokeTemplate = nullptr;

    UPROPERTY(EditAnywhere, Category = "VFX")
    TObjectPtr<UParticleSystem> SteamTemplate = nullptr;

    // ===== Steam Audio (기존) =====
    UPROPERTY(VisibleAnywhere, Category = "Audio")
    TObjectPtr<UAudioComponent> SteamAudio = nullptr;

    UPROPERTY(EditAnywhere, Category = "Audio")
    TObjectPtr<USoundBase> SteamSound = nullptr;

    UPROPERTY(EditAnywhere, Category = "Audio")
    float SteamSoundCooldown = 0.5f;

    // ===== Smolder Audio (신규) =====
    UPROPERTY(VisibleAnywhere, Category = "Audio")
    TObjectPtr<UAudioComponent> SmolderAudio = nullptr;

    // 1_Smolder_Loop
    UPROPERTY(EditAnywhere, Category = "Audio")
    TObjectPtr<USoundBase> SmolderLoopSound = nullptr;

    // 훈소 시작/종료 조건(히스테리시스)
    UPROPERTY(EditAnywhere, Category = "Audio")
    float SmolderStartProgress = 0.20f;

    UPROPERTY(EditAnywhere, Category = "Audio")
    float SmolderStopProgress = 0.10f;

    UPROPERTY(EditAnywhere, Category = "Audio")
    float SmolderFadeIn = 0.15f;

    UPROPERTY(EditAnywhere, Category = "Audio")
    float SmolderFadeOut = 0.25f;

    // ===== Fuel =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combustible|Fuel")
    FCombustibleFuelParams Fuel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combustible|Smoke")
    float SmokeStartProgress = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combustible|Smoke")
    float SmokeFullProgress = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combustible|Suppression")
    float WaterCoolPerSec = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combustible|Suppression")
    float WaterExtinguishPerSec = 0.25f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combustible|Runtime")
    float SmokeAlpha01 = 0.f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combustible|Runtime")
    float ExtinguishAlpha01 = 0.f;

    // 물(또는 소화약제) 입력
    void AddWaterContact(float Amount01, bool bTriggerSound = true);

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combustible|Runtime")
    bool bIsBurning = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combustible|Runtime")
    TObjectPtr<AFireActor> ActiveFire = nullptr;

    void SetOwningRoom(ARoomActor* InRoom);
    ARoomActor* GetOwningRoom() const { return OwningRoom.Get(); }

    UFUNCTION(BlueprintCallable, Category = "Combustible")
    bool IsBurning() const { return bIsBurning && ActiveFire != nullptr; }

    UFUNCTION(BlueprintCallable, Category = "Combustible|Fuel")
    float GetFuelRatio01() const { return Fuel.FuelRatio01_Cpp(); }

    void AddIgnitionPressure(const FGuid& SourceFireId, float Pressure);
    void AddHeat(float HeatDelta);
    void ConsumeFuel(float ConsumeAmount);

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "Combustible|Debug")
    AFireActor* ForceIgnite(bool bAllowElectric = true);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
    virtual void OnComponentCreated() override;
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
    // Room
    UPROPERTY() TWeakObjectPtr<ARoomActor> OwningRoom = nullptr;

    // 누적 입력
    float PendingPressure = 0.f;
    float PendingHeat = 0.f;

    //물
    UPROPERTY(VisibleInstanceOnly, Category = "Combustible|Suppression")
    float PendingWater01 = 0.f;

    float SteamSoundTimer = 0.f;
    static constexpr float WaterDecayPerSec = 1.25f;

private:
    bool CanIgniteNow() const;
    void UpdateIgnitionProgress(float DeltaTime);
    void TryIgnite();

    void OnIgnited();
    void OnExtinguished();

    void EnsureComponentsCreated(AActor* Owner, USceneComponent* RootComp);
    void ApplyTemplatesAndSounds();

    // 훈소 사운드 관리
    void UpdateSmolderAudio(float DeltaTime);

private:
    bool bComponentsNeedRecreation = false;
    bool bWasWaterSoundPlaying = false;
};
