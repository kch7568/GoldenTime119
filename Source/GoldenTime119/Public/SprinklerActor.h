#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "SprinklerActor.generated.h"

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
    float TraceDistance = 500.0f; // 아래로 레이를 쏠 거리

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprinkler Settings")
    float ExtinguishRadius = 200.0f; // 불을 끌 반경 (Overlap 사용 시)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprinkler Settings")
    float CheckInterval = 0.2f; // 감지 주기 (초)

    bool bIsSprinkling = false;
    float TimerAcc = 0.0f;

    // 밸브가 호출할 함수
    void ActivateWater();

    // 실제로 불을 찾는 로직
    void CheckAndExtinguishFire();

    // --- 스프링클러 사운드 관련 변수 ---

    // 소리 재생을 담당할 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UAudioComponent* SprinklerAudio;

    // 에디터에서 할당할 사운드 에셋
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
    USoundBase* WaterSound;
};