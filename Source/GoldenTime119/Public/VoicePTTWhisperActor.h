#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "VoicePTTWhisperActor.generated.h"

class UPTTAudioRecorderComponent;
class URealtimeVoiceComponent;
class ARadioManager;
class USoundBase;
class UAudioComponent;

UCLASS()
class GOLDENTIME119_API AVoicePTTWhisperActor : public AActor
{
	GENERATED_BODY()

public:
	AVoicePTTWhisperActor();

	UFUNCTION(BlueprintCallable, Category = "Voice")
	void StartPTT();

	UFUNCTION(BlueprintCallable, Category = "Voice")
	void StopPTT();

	// ===== Realtime Context API =====
	// PTT 키 다운 직전에 “상황변수” 갱신하고 싶을 때 호출
	// 예: "{ \"Suppression\": 23, \"Victims\": 2 }"
	UFUNCTION(BlueprintCallable, Category = "Voice|Realtime")
	void UpdateRealtimeGameState(const FString& ContextTextOrJson);

	// ===== Config =====
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Config")
	float StartCaptureDelaySeconds = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Radio")
	bool bBlockPTTWhenRadioBusy = true;

	// ===== SFX =====
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|SFX")
	TObjectPtr<USoundBase> SfxChannelBusyWarning = nullptr;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool bPTTActive = false;
	bool bCaptureStarted = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voice", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPTTAudioRecorderComponent> PTT = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voice", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URealtimeVoiceComponent> Realtime = nullptr;

	UPROPERTY()
	TObjectPtr<ARadioManager> RadioManager = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> StaticLoopAC = nullptr;

	FTimerHandle StartCaptureTimer;

	void EnsureComponentsBound();

	void StartCaptureInternal();

	// PTT Recorder callback
	UFUNCTION()
	void HandleWavReady(bool bSuccess, const FString& WavPathOrError);

	// Radio busy changed
	UFUNCTION()
	void HandleRadioBusyChanged(bool bBusy);

	// SFX helpers
	void PlayPTTStartSfx();
	void PlayPTTEndSfx();
	void StartStaticLoop();
	void StopStaticLoop();
	void PlayBusyWarning();

	// WAV -> PCM16 bytes (data chunk) 추출
	static bool ExtractPcm16FromWavFile(const FString& WavPath, TArray<uint8>& OutPcm16Bytes, int32& OutSampleRate, int32& OutNumChannels, FString& OutError);
};
