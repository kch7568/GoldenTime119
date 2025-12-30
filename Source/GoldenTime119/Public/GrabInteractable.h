// GrabInteractable.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GrabInteractable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UGrabInteractable : public UInterface
{
    GENERATED_BODY()
};

class GOLDENTIME119_API IGrabInteractable
{
    GENERATED_BODY()

public:
    // 잡았을 때 호출
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Grab")
    void OnGrabbed(USceneComponent* GrabbingController, bool bIsLeftHand);

    // 놓았을 때 호출
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Grab")
    void OnReleased(USceneComponent* GrabbingController, bool bIsLeftHand);

    // 잡을 수 있는지 확인
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Grab")
    bool CanBeGrabbed() const;

    // 잡을 때 붙을 위치 반환 (옵션)
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Grab")
    FTransform GetGrabTransform(bool bIsLeftHand) const;
};