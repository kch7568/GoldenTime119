// RealtimeVoiceComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RealtimeVoiceComponent.generated.h"

// ===== Delegates =====
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRealtimeConnected, bool, bConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRealtimeError, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRealtimeTextEvent, const FString&, Payload);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnRealtimeOutputAudioDelta,
	const TArray<uint8>&, Pcm16LE,
	int32, SampleRate,
	int32, NumChannels
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRealtimeOutputAudioDone);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GOLDENTIME119_API URealtimeVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URealtimeVoiceComponent();

	// ===== Lifecycle =====
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ===== Connection =====
	UFUNCTION(BlueprintCallable, Category = "Realtime")
	void Connect();

	UFUNCTION(BlueprintCallable, Category = "Realtime")
	void Disconnect();

	UFUNCTION(BlueprintCallable, Category = "Realtime")
	bool IsConnected() const;

	// ===== Session / Context =====
	UFUNCTION(BlueprintCallable, Category = "Realtime")
	void UpdateDynamicContext(const FString& NewContext);

	// Turn reset: cancel old response + clear input/output buffers
	UFUNCTION(BlueprintCallable, Category = "Realtime")
	void BeginUserTurn(bool bCancelOngoingResponse, bool bClearOutputAudio);

	// input audio streaming
	UFUNCTION(BlueprintCallable, Category = "Realtime")
	void AppendInputAudioPCM16(const TArray<uint8>& Pcm16Bytes);

	UFUNCTION(BlueprintCallable, Category = "Realtime")
	void CommitInputAudio();

	UFUNCTION(BlueprintCallable, Category = "Realtime")
	void CreateResponse();

	// ===== Safety gate =====
	// CreateResponse()를 호출한 턴에서만 output_audio.delta를 처리하도록 게이트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Realtime|Safety")
	bool bGateOutputAudioToCreateResponse = true;

	UFUNCTION(BlueprintCallable, Category = "Realtime|Safety")
	void SetAllowServerAudio(bool bAllow);

	UFUNCTION(BlueprintCallable, Category = "Realtime|Safety")
	bool IsServerAudioAllowed() const { return bAllowServerAudio; }

	// ===== Events =====
	UPROPERTY(BlueprintAssignable, Category = "Realtime|Events")
	FOnRealtimeConnected OnConnected;

	UPROPERTY(BlueprintAssignable, Category = "Realtime|Events")
	FOnRealtimeError OnError;

	UPROPERTY(BlueprintAssignable, Category = "Realtime|Events")
	FOnRealtimeTextEvent OnTextEvent;

	UPROPERTY(BlueprintAssignable, Category = "Realtime|Audio")
	FOnRealtimeOutputAudioDelta OnOutputAudioDelta;

	UPROPERTY(BlueprintAssignable, Category = "Realtime|Audio")
	FOnRealtimeOutputAudioDone OnOutputAudioDone;

	// (옵션) 디버깅용: 오디오 시작/끝
	UPROPERTY(BlueprintAssignable, Category = "Realtime|Audio")
	FOnRealtimeOutputAudioDone OnAudioStarted;
	UPROPERTY(BlueprintAssignable, Category = "Realtime|Audio")
	FOnRealtimeOutputAudioDone OnAudioEnded;

	// ===== Config =====
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Realtime|Config")
	FString RealtimeModel = TEXT("gpt-4o-realtime-preview");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Realtime|Config")
	FString VoiceName = TEXT("alloy");

	// input format (서버 선언은 실제로 보내는 포맷과 반드시 동일해야 함)
	// ※ 수동 PTT + PCM16 스트리밍은 보통 24kHz mono로 고정하는 게 가장 안정적
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Realtime|Audio")
	int32 InputSampleRate = 24000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Realtime|Audio")
	int32 InputNumChannels = 1;

	// output format (힌트 값)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Realtime|Audio")
	int32 OutputSampleRate = 24000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Realtime|Audio")
	int32 OutputNumChannels = 1;

	// PTT 모드에서는 VAD를 끄는 게 일반적
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Realtime|Audio")
	bool bDisableVADForPTT = true;

	// System instructions
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Realtime|Prompt", meta = (MultiLine = true))
	FString BaseInstructions;

	// Dynamic context (게임 상태)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Realtime|Prompt", meta = (MultiLine = true))
	FString DynamicContext;

	// 디버그/안정화용:
	// response.create에 instructions를 같이 실어 "프롬프트 적용"을 강제 검증
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Realtime|Prompt")
	bool bIncludeInstructionsOnResponseCreate = true;

	// API Key
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Realtime|Auth")
	FString ApiKey;

	// 상대경로면 UserDir 기준
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Realtime|Auth")
	FString KeyFilePath = TEXT("Documents/key/API_DoNotMoveOrCopy.txt");

	// ===== Logging =====
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Realtime|Debug")
	bool bEnableVerboseLog = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Realtime|Debug")
	bool bLogOutgoingJson = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Realtime|Debug")
	bool bLogIncomingJson = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Realtime|Debug")
	bool bLogIncomingSummary = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Realtime|Debug")
	int32 LogEveryNAppends = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Realtime|Debug")
	int32 LogEveryNAudioDeltas = 30;

private:
	// ===== Websocket =====
	TSharedPtr<class IWebSocket> Socket;
	double ConnectStartTimeSec = 0.0;

	// ===== Counters =====
	int64 OutgoingEventCounter = 0;
	int64 IncomingEventCounter = 0;
	int64 AppendCounter = 0;
	int64 AudioDeltaCounter = 0;
	int64 TotalAppendedPcmBytes = 0;
	int64 TotalReceivedOutAudioBytes = 0;

	// ===== State =====
	bool bDidStartAudio = false;
	bool bAllowServerAudio = false;

	// session.update 타이밍 안정화: session.created 이후에만 update
	bool bSessionCreated = false;
	bool bPendingInitialSessionUpdate = false;

	// ===== Utils =====
	FString BuildWebSocketUrl() const;
	FString ResolveKeyPath(const FString& InPath) const;
	static FString MaskKeyForLog(const FString& Key);
	static FString NowShort();
	static FString TruncateForLog(const FString& S, int32 MaxLen);
	static FString JsonToString(const TSharedPtr<FJsonObject>& Obj, bool bPretty);

	void DebugDumpState(const TCHAR* Tag) const;

	FString LoadApiKeyMaybe(FString& OutResolvedPath, FString& OutError) const;

	// ===== WS handlers =====
	void HandleWsConnected();
	void HandleWsConnectionError(const FString& Error);
	void HandleWsClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	void HandleWsMessage(const FString& Message);

	// ===== Protocol helpers =====
	void SendJsonEvent(const TSharedPtr<FJsonObject>& Obj, const TCHAR* DebugTag);
	void SendSessionUpdate(const TCHAR* ReasonTag);

	void HandleServerEvent(const TSharedPtr<FJsonObject>& Root);

	// ===== Helpers =====
	FString BuildInstructionsMerged() const;
};
