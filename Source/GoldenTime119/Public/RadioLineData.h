#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "RadioSubtitleInfomation.h"   // ✅ 여기서 공용 struct 가져옴

#include "RadioLineData.generated.h"

UCLASS(BlueprintType)
class GOLDENTIME119_API URadioLineData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	USoundBase* VoiceSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	float PreDelay = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	float PostDelay = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	FRadioSubtitleInfomation Subtitle;
};
