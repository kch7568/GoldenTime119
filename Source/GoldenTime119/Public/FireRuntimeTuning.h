// ============================ FireRuntimeTuning.h ============================
#pragma once

#include "CoreMinimal.h"
#include "FireRuntimeTuning.generated.h"

USTRUCT(BlueprintType)
struct FFireRuntimeTuning
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float FuelRatio01 = 1.f;
    UPROPERTY(BlueprintReadOnly) float Intensity01 = 1.f;

    UPROPERTY(BlueprintReadOnly) float SpreadRadius = 350.f;
    UPROPERTY(BlueprintReadOnly) float SpreadInterval = 1.f;

    UPROPERTY(BlueprintReadOnly) float ConsumePerSecond = 1.f;
    UPROPERTY(BlueprintReadOnly) float InfluenceScale = 1.f;
};
