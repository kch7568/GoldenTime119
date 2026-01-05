// ============================ GasTankActor.h ============================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GasTankActor.generated.h"

class UPressureVesselComponent;
class UCombustibleComponent;
class UStaticMeshComponent;

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

private:
    UFUNCTION()
    void OnBLEVETriggered(FVector ExplosionLocation);

    void ApplyTankTypeParameters();

    bool bHasExploded = false;
};