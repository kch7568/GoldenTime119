#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SprinklerActor.h" // 스프링클러 참조를 위해 포함
#include "GrabInteractable.h" //vr에서 손으로 밸브 돌리기위해 포함
#include "ValveActor.generated.h"

UCLASS()
class GOLDENTIME119_API AValveActor : public AActor, public IGrabInteractable
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

public :
    // VR 인터페이스 함수
    virtual void OnGrabbed_Implementation(USceneComponent* GrabbingController, bool bIsLeftHand) override;
    virtual void OnReleased_Implementation(USceneComponent* GrabbingController, bool bIsLeftHand) override;
private:
    // 물리적 회전을 추적하기 위한 핵심 변수들
    bool bIsValveBeingTurned = false;        // 현재 손으로 돌리고 있는지 여부
    UPROPERTY()
    USceneComponent* CurrentGrabbingController = nullptr; // 잡고 있는 컨트롤러

    FRotator InitialHandRotation;            // 잡는 순간의 손 회전값
    FRotator InitialValveRotation;           // 잡는 순간의 밸브 회전값
};