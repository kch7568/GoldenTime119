#pragma once

#include "CoreMinimal.h"
#include "CombustibleType.generated.h"

UENUM(BlueprintType)
enum class ECombustibleType : uint8
{
    Normal      UMETA(DisplayName = "Normal"),
    Oil         UMETA(DisplayName = "Oil"),
    Electric    UMETA(DisplayName = "Electric"),
    Explosive   UMETA(DisplayName = "Explosive"),
};
