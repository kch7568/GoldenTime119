// RadioLineData.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RadioLineData.generated.h"

// === 여기로 USTRUCT를 옮김 ===
USTRUCT(BlueprintType)
struct FRadioSubtitleInfomation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	FName SpeakerId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	FText SpeakerName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio", meta = (MultiLine = true))
	FText SubtitleText;
};

// === DataAsset ===
UCLASS(BlueprintType)
class GOLDENTIME119_API URadioLineData : public UDataAsset
{
	GENERATED_BODY()

public:
	// 이 무전에서 재생할 음성 클립 (B2)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	USoundBase* VoiceSound;

	// 무전 시작 전에 줄 지연 (선택사항)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	float PreDelay = 0.0f;

	// 음성 끝난 뒤, 종료음 재생 전 잠깐 쉼
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	float PostDelay = 0.2f;

	// 자막 정보 (화자 / 텍스트)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	FRadioSubtitleInfomation Subtitle;
};
