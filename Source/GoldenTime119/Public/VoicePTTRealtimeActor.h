// VoicePTTRealtimeActor.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RadioLineData.h"            // FRadioSubtitleInfomation
#include "VoicePTTRealtimeActor.generated.h"

class UPTTAudioRecorderComponent;
class URealtimeVoiceComponent;
class ARadioManager;
class USoundBase;
class UAudioComponent;

UCLASS()
class GOLDENTIME119_API AVoicePTTRealtimeActor : public AActor
{
	GENERATED_BODY()

public:
	AVoicePTTRealtimeActor();

	// ====== BP API ======
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void StartPTT();

	UFUNCTION(BlueprintCallable, Category = "Voice")
	void StopPTT();

	// 상황 스냅샷 (원하면 외부에서 갱신)
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void UpdateGameStateForAI(const FString& NewSnapshot);

	// ====== Debug / Config ======
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Debug")
	bool bAutoConnectOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Debug")
	bool bBroadcastDebugText = true;

	// 삑 소리 녹음 유입 방지 딜레이
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Config", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float StartCaptureDelaySeconds = 0.08f;

	// StopPTT에서 Commit/Create를 바로 할지(즉시 응답 시작)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Config")
	bool bCommitAndCreateImmediatelyOnStop = true;

	// ====== SFX (Start / Loop / End) ======
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|SFX")
	TObjectPtr<USoundBase> SfxPTTStart = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|SFX")
	TObjectPtr<USoundBase> SfxPTTEnd = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|SFX")
	TObjectPtr<USoundBase> SfxPTTStaticLoop = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|SFX", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float StaticLoopVolume = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|SFX", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float StaticLoopPitch = 1.0f;

	// ====== Radio Busy 연동 ======
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Radio")
	bool bBlockPTTWhenRadioBusy = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|SFX")
	TObjectPtr<USoundBase> SfxChannelBusyWarning = nullptr;

	// ====== Realtime Subtitle (AI 응답용) ======
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Radio")
	FName AISpeakerId = "AI";

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Radio")
	FText AISpeakerName = FText::FromString(TEXT("지휘"));

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// flags
	bool bPTTActive = false;

	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voice", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPTTAudioRecorderComponent> PTT = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voice", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URealtimeVoiceComponent> Realtime = nullptr;

	// Radio
	UPROPERTY()
	TWeakObjectPtr<ARadioManager> RadioManager;

	// Static loop handle
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> StaticLoopAC = nullptr;

	// Start delay timer
	FTimerHandle StartCaptureTimer;

	// Cached snapshot
	FString CachedGameState;

	// ===== internal =====
	void EnsureComponentsBound();
	ARadioManager* GetRadioManagerCached();

	void StartCaptureInternal();

	// === Realtime transmission helpers ===
	void BeginRadioRealtime();    // StartTone + LoopNoise + SubtitleBegin
	void EndRadioRealtime();      // EndTone + SubtitleEnd + Busy false

	// PTT callbacks
	UFUNCTION()
	void HandlePcm16FrameReady(const TArray<uint8>& Pcm16BytesLE, int32 SampleRate, int32 NumChannels, float FrameDurationSec);

	UFUNCTION()
	void HandleCaptureFinalized(bool bSuccess, float TotalDurationSec, const FString& ErrorOrInfo);

	// Radio busy changed
	UFUNCTION()
	void HandleRadioBusyChanged(bool bBusy);

	// Realtime callbacks (server -> radio manager)
	UFUNCTION()
	void HandleRealtimeOutputAudioDelta(const TArray<uint8>& Pcm16LE, int32 SampleRate, int32 NumChannels);

	UFUNCTION()
	void HandleRealtimeOutputAudioDone();

	// SFX helpers
	void PlayPTTStartSfx();
	void PlayPTTEndSfx();
	void StartStaticLoop();
	void StopStaticLoop();
	void PlayBusyWarning();
};
