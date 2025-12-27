#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "SprinklerActor.generated.h"

class UCombustibleComponent;

UCLASS()
class GOLDENTIME119_API ASprinklerActor : public AActor
{
    GENERATED_BODY()

public:
    ASprinklerActor();

protected:
    virtual void Tick(float DeltaTime) override;

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* SprinklerMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UParticleSystemComponent* WaterVFX;

    // --- 스프링클러 기능 관련 변수 ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprinkler Settings")
    float TraceDistance = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprinkler Settings")
    float ExtinguishRadius = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprinkler Settings")
    float CheckInterval = 0.2f;

    // 물 입력 강도 (0..1, 틱마다 가연물에 전달)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprinkler Settings")
    float WaterIntensity = 1.0f;

    // 물이 닿을 때 수증기 효과음 트리거 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprinkler Settings")
    bool bTriggerSteamSound = true;

    bool bIsSprinkling = false;
    float TimerAcc = 0.0f;

    // 밸브가 호출할 함수
    void ActivateWater();

    // 실제로 물을 뿌리는 로직 (가연물 탐지 + 물 입력)
    void CheckAndApplyWater();

    // --- 스프링클러 사운드 관련 변수 ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UAudioComponent* SprinklerAudio;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
    USoundBase* WaterSound;
};