#pragma once

#include "CoreMinimal.h"
#include "RadioSubtitleInfomation.generated.h"

USTRUCT(BlueprintType)
struct GOLDENTIME119_API FRadioSubtitleInfomation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	FName SpeakerId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	FText SpeakerName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio", meta = (MultiLine = true))
	FText SubtitleText;
};
