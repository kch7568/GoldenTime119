#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RadioLineData.h"
#include "RadioManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRadioSubtitleBegin, const FRadioSubtitleInfomation&, Subtitle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRadioSubtitleEnd, const FRadioSubtitleInfomation&, Subtitle);

UENUM(BlueprintType)
enum class ERadioPlayState : uint8
{
	Idle       UMETA(DisplayName = "Idle"),
	StartTone  UMETA(DisplayName = "StartTone"),
	PreDelay   UMETA(DisplayName = "PreDelay"),
	Voice      UMETA(DisplayName = "Voice"),
	PostDelay  UMETA(DisplayName = "PostDelay"),
	EndTone    UMETA(DisplayName = "EndTone")
};

UCLASS()
class GOLDENTIME119_API ARadioManager : public AActor
{
	GENERATED_BODY()

public:
	ARadioManager();

	// 월드 컨텍스트 기반 전역 접근
	UFUNCTION(BlueprintCallable, Category = "Radio", meta = (WorldContext = "WorldContextObject"))
	static ARadioManager* GetRadioManager(UObject* WorldContextObject);

	// 무전 라인 큐에 추가
	UFUNCTION(BlueprintCallable, Category = "Radio")
	void EnqueueRadioLine(URadioLineData* LineData);

	UFUNCTION(BlueprintCallable, Category = "Radio")
	bool IsBusy() const { return bIsPlaying; }

	// 자막 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Radio|Subtitle")
	FOnRadioSubtitleBegin OnSubtitleBegin;

	UPROPERTY(BlueprintAssignable, Category = "Radio|Subtitle")
	FOnRadioSubtitleEnd OnSubtitleEnd;

protected:
	virtual void BeginPlay() override;

	// 오디오 컴포넌트들
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Radio|Audio")
	class UAudioComponent* VoiceAudioComp;	// B2: 음성

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Radio|Audio")
	class UAudioComponent* LoopAudioComp;	// B1: 루프 노이즈

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Radio|Audio")
	class UAudioComponent* SfxAudioComp;	// A/C: 시작/종료음

	// 공통 사운드
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio|Config")
	USoundBase* StartToneSound;	// A

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio|Config")
	USoundBase* LoopNoiseSound;	// B1

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio|Config")
	USoundBase* EndToneSound;	// C

	// 상태
	UPROPERTY()
	TArray<URadioLineData*> Queue;

	UPROPERTY()
	URadioLineData* CurrentLine;

	UPROPERTY()
	ERadioPlayState PlayState = ERadioPlayState::Idle;

	UPROPERTY()
	bool bIsPlaying = false;

	FTimerHandle StateTimerHandle;

	// 내부 재생 흐름
	void TryPlayNextFromQueue();
	void PlayStartTone();
	void OnStartToneFinished();

	void PlayPreDelay();
	void OnPreDelayFinished();

	void PlayVoice();

	// UAudioComponent::OnAudioFinished 에 연결될 함수 (파라미터 없음)
	UFUNCTION()
	void OnVoiceFinished();

	void PlayPostDelay();
	void OnPostDelayFinished();

	void PlayEndTone();
	void OnEndToneFinished();

	void FinishCurrentAndContinue();
	void ClearStateTimer();
};
