#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombustibleType.h"
#include "CombustibleComponent.generated.h"

UCLASS(ClassGroup = (Fire), meta = (BlueprintSpawnableComponent))
class GOLDENTIME119_API UCombustibleComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire")
    ECombustibleType CombustibleType = ECombustibleType::Normal;

    // Electric 타입일 때만 사용: 트리거가 발생해야 점화/확산 허용
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Fire|Electric")
    bool bElectricIgnitionTriggered = false;

    // (선택) 전기 연결망 그룹. 같은 NetId끼리 우선 확산하도록 사용 가능
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire|Electric")
    FName ElectricNetId = NAME_None;

    UFUNCTION(BlueprintCallable, Category = "Fire|Electric")
    void TriggerElectricIgnition() { bElectricIgnitionTriggered = true; }
};
