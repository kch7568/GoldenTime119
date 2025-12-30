// VRPawn.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Camera/CameraComponent.h"
#include "MotionControllerComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "InputActionValue.h"
#include "VRPawn.generated.h"

class UInputMappingContext;
class UInputAction;

UCLASS()
class GOLDENTIME119_API AVRPawn : public APawn
{
    GENERATED_BODY()

public:
    AVRPawn();

    // ============================================================
    // VR 컴포넌트
    // ============================================================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
    TObjectPtr<USceneComponent> VRRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
    TObjectPtr<UCameraComponent> VRCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
    TObjectPtr<UMotionControllerComponent> LeftMotionController;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
    TObjectPtr<UMotionControllerComponent> RightMotionController;

    // ============================================================
    // 손 Grab 영역
    // ============================================================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR|Grab")
    TObjectPtr<USphereComponent> LeftGrabSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR|Grab")
    TObjectPtr<USphereComponent> RightGrabSphere;

    // ============================================================
    // 손 메시 (옵션)
    // ============================================================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR|Mesh")
    TObjectPtr<USkeletalMeshComponent> LeftHandMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR|Mesh")
    TObjectPtr<USkeletalMeshComponent> RightHandMesh;

    // ============================================================
    // Grab 설정
    // ============================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR|Grab")
    float GrabRadius = 10.f;

    // ============================================================
    // Grab 상태
    // ============================================================

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "VR|State")
    TObjectPtr<AActor> LeftGrabbedActor;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "VR|State")
    TObjectPtr<AActor> RightGrabbedActor;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "VR|State")
    bool bIsLeftGrabbing = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "VR|State")
    bool bIsRightGrabbing = false;

    // ============================================================
    // Enhanced Input
    // ============================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR|Input")
    TObjectPtr<UInputMappingContext> VRMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR|Input")
    TObjectPtr<UInputAction> IA_GrabLeft;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR|Input")
    TObjectPtr<UInputAction> IA_GrabRight;

    // ============================================================
    // 테스트용 (키보드)
    // ============================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR|Debug")
    bool bEnableKeyboardTest = true;

    // ============================================================
    // Grab 함수
    // ============================================================

    UFUNCTION(BlueprintCallable, Category = "VR|Grab")
    void TryGrabLeft();

    UFUNCTION(BlueprintCallable, Category = "VR|Grab")
    void TryReleaseLeft();

    UFUNCTION(BlueprintCallable, Category = "VR|Grab")
    void TryGrabRight();

    UFUNCTION(BlueprintCallable, Category = "VR|Grab")
    void TryReleaseRight();

    UFUNCTION(BlueprintCallable, Category = "VR|Grab")
    AActor* GetNearestGrabbableActor(USphereComponent* GrabSphere);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    void SetupEnhancedInput();
    void SetupKeyboardTest();

    bool bInputBound = false;
};