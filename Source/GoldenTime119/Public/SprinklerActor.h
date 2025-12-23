#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Particles/ParticleSystemComponent.h"
#include "SprinklerActor.generated.h"

UCLASS()
class GOLDENTIME119_API ASprinklerActor : public AActor
{
    GENERATED_BODY()

public:
    ASprinklerActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* SprinklerMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UParticleSystemComponent* WaterVFX;

    // 밸브가 호출할 함수
    void ActivateWater();
};