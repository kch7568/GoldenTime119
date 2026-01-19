// ============================ GasTankActor.h ============================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GasTankActor.generated.h"

class UPressureVesselComponent;
class UCombustibleComponent;
class UStaticMeshComponent;
class UAudioComponent;
class USoundBase;

UENUM(BlueprintType)
enum class EGasTankType : uint8
{
    SmallCanister,
    PortableTank,
    IndustrialTank,
    StorageTank
};

UCLASS()
class GOLDENTIME119_API AGasTankActor : public AActor
{
    GENERATED_BODY()

public:
    AGasTankActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> TankMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UPressureVesselComponent> PressureVessel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCombustibleComponent> Combustible;

    // ===== Audio Components / Assets =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
    TObjectPtr<UAudioComponent> GasLeakAudioComponent;

    // 가스 새는 루프 사운드 (Cue/Mono SFX 등)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
    TObjectPtr<USoundBase> GasLeakSound;

    // BLEVE 폭발 사운드 (원샷)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
    TObjectPtr<USoundBase> ExplosionSound;

    // 피치 설정 (0.0 → 2.0까지 올라가도록 노출)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
    float GasLeakPitchMin = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
    float GasLeakPitchMax = 2.0f;

    // "폭발 직전 급증" 시작 지점 (압력 비율 0~1)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
    float GasLeakRapidIncreaseStartRatio = 0.8f;

    // 피치 보간 속도
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
    float GasLeakPitchInterpSpeed = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GasTank")
    EGasTankType TankType = EGasTankType::PortableTank;

    UFUNCTION(BlueprintCallable, Category = "GasTank")
    void SetTankType(EGasTankType NewType);

    UFUNCTION(BlueprintCallable, Category = "GasTank")
    float GetPressureRatio() const;

    UFUNCTION(BlueprintCallable, Category = "GasTank")
    bool IsInDanger() const;

    UFUNCTION(BlueprintCallable, Category = "GasTank")
    void ForceBLEVE();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    UFUNCTION()
    void OnBLEVETriggered(FVector ExplosionLocation);

    void ApplyTankTypeParameters();
    void UpdateGasLeakAudio(float DeltaSeconds);

    bool bHasExploded = false;
};
