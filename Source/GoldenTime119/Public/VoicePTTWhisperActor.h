#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoicePTTWhisperActor.generated.h"

class UPTTAudioRecorderComponent;
class UWhisperSTTComponent;
class ARadioManager;
class USoundBase;
class UAudioComponent;

UENUM(BlueprintType)
enum class EGTVoiceCommand : uint8
{
	None UMETA(DisplayName = "None"),
	FireStatusAndSuppression UMETA(DisplayName = "화재 상황 + 진압률"),
	VictimAndPeople UMETA(DisplayName = "요구조자 + 사람")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnGTVoiceCommandDetected,
	EGTVoiceCommand, Command,
	const FString&, RawText,
	float, Confidence
);

UCLASS()
class GOLDENTIME119_API AVoicePTTWhisperActor : public AActor
{
	GENERATED_BODY()

public:
	AVoicePTTWhisperActor();

	// ====== BP Events ======
	UPROPERTY(BlueprintAssignable, Category = "Voice|Events")
	FOnGTVoiceCommandDetected OnCommandDetected;

	// ====== BP API ======
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void StartPTT();

	UFUNCTION(BlueprintCallable, Category = "Voice")
	void StopPTT();

	// ====== Debug / Config ======
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Debug")
	bool bBroadcastRawTextAlways = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinConfidenceToAccept = 0.60f;

	// 삑 소리 녹음 유입 방지 딜레이
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Config", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float StartCaptureDelaySeconds = 0.08f;

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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// capturing flag (중복 방지)
	bool bPTTActive = false;
	bool bCaptureStarted = false;

	// Components (BP에서 꼬여도 런타임에 찾아서 재결합할 거라 private 유지 OK)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voice", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPTTAudioRecorderComponent> PTT = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voice", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWhisperSTTComponent> Whisper = nullptr;

	// Radio Manager
	UPROPERTY()
	TObjectPtr<ARadioManager> RadioManager = nullptr;

	// Static loop handle
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> StaticLoopAC = nullptr;

	// Start delay timer
	FTimerHandle StartCaptureTimer;

	// ====== 핵심: BP 꼬임 방지용 “컴포넌트 재탐색/재결합” ======
	void EnsureComponentsBound();

	// Pipeline binding
	UFUNCTION()
	void HandleWavReady(bool bSuccess, const FString& WavPathOrError);

	UFUNCTION()
	void HandleWhisperFinished(bool bSuccess, const FString& TextOrError);

	// Radio busy changed
	UFUNCTION()
	void HandleRadioBusyChanged(bool bBusy);

	// Internal start capture
	void StartCaptureInternal();

	// SFX helpers
	void PlayPTTStartSfx();
	void PlayPTTEndSfx();
	void StartStaticLoop();
	void StopStaticLoop();
	void PlayBusyWarning();

	// Parsing helpers
	static FString NormalizeKo(const FString& In);
	static bool TryExtractPercent(const FString& S, float& OutPercent);
	static bool TryExtractCount(const FString& S, int32& OutCount);

	static void ScoreCommand_TwoOnly(
		const FString& Normalized,
		EGTVoiceCommand& OutCmd,
		float& OutConfidence
	);
};
