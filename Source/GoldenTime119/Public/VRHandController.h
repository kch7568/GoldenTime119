// VRHandController.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MotionControllerComponent.h"
#include "Components/SphereComponent.h"
#include "GrabInteractable.h"
#include "VRHandController.generated.h"

UENUM(BlueprintType)
enum class EHandType : uint8
{
    Left    UMETA(DisplayName = "Left"),
    Right   UMETA(DisplayName = "Right")
};

UCLASS()
class GOLDENTIME119_API AVRHandController : public AActor
{
    GENERATED_BODY()

public:
    AVRHandController();

    // ============================================================
    // 컴포넌트
    // ============================================================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
    TObjectPtr<UMotionControllerComponent> MotionController;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
    TObjectPtr<USphereComponent> GrabSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
    TObjectPtr<USkeletalMeshComponent> HandMesh;

    // ============================================================
    // 설정
    // ============================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR|Setup")
    EHandType HandType = EHandType::Right;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR|Setup")
    float GrabRadius = 10.f;

    // ============================================================
    // 상태
    // ============================================================

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "VR|State")
    TObjectPtr<AActor> GrabbedActor;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "VR|State")
    bool bIsGrabbing = false;

    // ============================================================
    // 함수
    // ============================================================

    UFUNCTION(BlueprintCallable, Category = "VR|Grab")
    void TryGrab();

    UFUNCTION(BlueprintCallable, Category = "VR|Grab")
    void TryRelease();

    UFUNCTION(BlueprintCallable, Category = "VR|Grab")
    AActor* GetNearestGrabbableActor();

    UFUNCTION(BlueprintPure, Category = "VR")
    bool IsLeftHand() const { return HandType == EHandType::Left; }

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    void SetupMotionController();
};