#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

#include "RadioSubtitleInfomation.h" // ✅ 공용 struct

#include "RadioManager.generated.h"

class UAudioComponent;
class USoundBase;
class USoundWaveProcedural;
class URadioLineData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRadioBusyChanged, bool, bBusy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRadioSubtitleBegin, const FRadioSubtitleInfomation&, Subtitle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRadioSubtitleEnd, const FRadioSubtitleInfomation&, Subtitle);

UENUM(BlueprintType)
enum class ERadioPlayState : uint8
{
	Idle,

	// Clip queue pipeline
	StartTone,
	PreDelay,
	Voice,
	PostDelay,
	EndTone,

	// Realtime pipeline
	RealtimeStartTone,
	RealtimeVoice,
	RealtimeEndTone,
};

UCLASS()
class GOLDENTIME119_API ARadioManager : public AActor
{
	GENERATED_BODY()

public:
	ARadioManager();
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Radio")
	static ARadioManager* GetRadioManager(UObject* WorldContextObject);

	// =======================
	// Existing: clip queue
	// =======================
	UFUNCTION(BlueprintCallable, Category = "Radio|Queue")
	void EnqueueRadioLine(URadioLineData* LineData);

	UFUNCTION(BlueprintCallable, Category = "Radio|State")
	bool IsBusy() const { return bIsPlaying || bRealtimeActive || CurrentLine != nullptr || Queue.Num() > 0; }

	// =======================
	// New: Realtime streaming
	// =======================
	UFUNCTION(BlueprintCallable, Category = "Radio|Realtime")
	bool BeginRealtimeTransmission(const FRadioSubtitleInfomation& SubtitleInfo, bool bInterruptIfBusy);

	UFUNCTION(BlueprintCallable, Category = "Radio|Realtime")
	void AppendRealtimePcm16(const TArray<uint8>& Pcm16LE, int32 SampleRate, int32 NumChannels);

	UFUNCTION(BlueprintCallable, Category = "Radio|Realtime")
	void EndRealtimeTransmission(bool bFlushAndStop);

	UFUNCTION(BlueprintCallable, Category = "Radio|Realtime")
	bool IsRealtimeTransmitting() const { return bRealtimeActive; }

	// ===== Events =====
	UPROPERTY(BlueprintAssignable, Category = "Radio|Events")
	FOnRadioBusyChanged OnBusyChanged;

	UPROPERTY(BlueprintAssignable, Category = "Radio|Events")
	FOnRadioSubtitleBegin OnSubtitleBegin;

	UPROPERTY(BlueprintAssignable, Category = "Radio|Events")
	FOnRadioSubtitleEnd OnSubtitleEnd;

	// ===== Audio assets =====
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio|Audio")
	USoundBase* StartToneSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio|Audio")
	USoundBase* EndToneSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio|Audio")
	USoundBase* LoopNoiseSound = nullptr;

	// ===== Realtime tuning =====
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio|Realtime|Tuning")
	float RealtimeFlushIntervalSec = 0.02f; // 20ms

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio|Realtime|Tuning")
	float RealtimePrebufferMs = 120.0f; // 80~150ms

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio|Realtime|Tuning")
	int32 RealtimeDefaultSampleRate = 24000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio|Realtime|Tuning")
	int32 RealtimeDefaultNumChannels = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio|Realtime|Tuning")
	bool bRealtimeForceMono = true;

private:
	// ===== Components =====
	UPROPERTY(VisibleAnywhere)
	UAudioComponent* VoiceAudioComp = nullptr;

	UPROPERTY(VisibleAnywhere)
	UAudioComponent* LoopAudioComp = nullptr;

	UPROPERTY(VisibleAnywhere)
	UAudioComponent* SfxAudioComp = nullptr;

	// ===== Clip queue state =====
	UPROPERTY()
	TArray<URadioLineData*> Queue;

	UPROPERTY()
	URadioLineData* CurrentLine = nullptr;

	bool bIsPlaying = false;
	ERadioPlayState PlayState = ERadioPlayState::Idle;

	FTimerHandle StateTimerHandle;

	void TryPlayNextFromQueue();
	void PlayStartTone();
	void OnStartToneFinished();
	void PlayPreDelay();
	void OnPreDelayFinished();
	void PlayVoice();

	UFUNCTION()
	void OnVoiceFinished();

	void PlayPostDelay();
	void OnPostDelayFinished();
	void PlayEndTone();
	void OnEndToneFinished();
	void FinishCurrentAndContinue();
	void ClearStateTimer();

	// ===== Realtime state =====
	bool bRealtimeActive = false;
	bool bRealtimeVoiceStarted = false;

	FRadioSubtitleInfomation RealtimeSubtitle;

	UPROPERTY()
	USoundWaveProcedural* RealtimeWave = nullptr;

	int32 RealtimeSampleRate = 0;
	int32 RealtimeNumChannels = 0;

	TArray<uint8> PendingPcm;

	FTimerHandle RealtimeFlushTimerHandle;

	void InterruptAllPlayback_Internal(bool bClearQueue);

	void PlayRealtimeStartTone();
	void OnRealtimeStartToneFinished();
	void PlayRealtimeEndTone();
	void OnRealtimeEndToneFinished();

	USoundWaveProcedural* EnsureRealtimeWave(int32 SampleRate, int32 NumChannels);

	void StartFlushTimerIfNeeded();
	void StopFlushTimer();

	// ✅ 헤더/CPP 시그니처 일치
	void FlushPendingPcm(bool bForceAll);

	void StartRealtimeVoiceIfNeeded();
	void StopRealtimeVoice_Internal();

	int32 GetUseChannels(int32 InChannels) const;
	int32 CalcPrebufferBytes(int32 SampleRate, int32 NumChannels) const;
};
