#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SprinklerActor.h" // 스프링클러 참조를 위해 포함
#include "ValveActor.generated.h"

UCLASS()
class GOLDENTIME119_API AValveActor : public AActor
{
    GENERATED_BODY()

public:
    AValveActor();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* PipeMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* ValveMesh;

    // 에디터에서 물을 뿜을 스프링클러를 직접 지정할 변수
    UPROPERTY(EditInstanceOnly, Category = "Settings")
    ASprinklerActor* TargetSprinkler;

    UPROPERTY(EditAnywhere, Category = "Settings")
    float RotationSpeed = 90.0f;

    UPROPERTY(EditAnywhere, Category = "Settings")
    float TargetRotation = 180.0f;

private:
    bool bIsPressing = false;
    float CurrentRotationSum = 0.0f;
    bool bIsTriggered = false;

    void OnFKeyPressed();
    void OnFKeyReleased();
};