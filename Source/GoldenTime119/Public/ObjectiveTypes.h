#pragma once

#include "CoreMinimal.h"
#include "ObjectiveTypes.generated.h"

UENUM(BlueprintType)
enum class EObjectiveStatus : uint8
{
	Inactive	UMETA(DisplayName = "Inactive"),
	Active		UMETA(DisplayName = "Active"),
	Completed	UMETA(DisplayName = "Completed"),
	Failed		UMETA(DisplayName = "Failed")
};

USTRUCT(BlueprintType)
struct GOLDENTIME119_API FObjectiveState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	FName ObjectiveId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	FText Detail;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	EObjectiveStatus Status = EObjectiveStatus::Inactive;

	// 정렬/우선순위가 필요할 때 사용 (무전기/UI 표시 순서)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	int32 SortOrder = 0;

	// 확장 메타: "TargetRoom=Kitchen" 같은 문자열 키-값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	TMap<FName, FString> Meta;

	bool IsValid() const { return ObjectiveId != NAME_None; }
};
