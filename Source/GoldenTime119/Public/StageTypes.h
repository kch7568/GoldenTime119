// StageTypes.h
#pragma once
#include "CoreMinimal.h"
#include "StageTypes.generated.h"

USTRUCT(BlueprintType)
struct FStageObjective
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Title;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Detail;
};
