// RealtimeVoiceComponent.cpp
#include "RealtimeVoiceComponent.h"

#include "IWebSocket.h"
#include "WebSocketsModule.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"

DEFINE_LOG_CATEGORY_STATIC(LogRealtimeVoice, Log, All);

URealtimeVoiceComponent::URealtimeVoiceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	BaseInstructions =
		TEXT("You are an emergency radio operator. ")
		TEXT("Answer concisely with actionable guidance. ")
		TEXT("If you are unsure, say so briefly.");
}

void URealtimeVoiceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bEnableVerboseLog)
	{
		UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime] BeginPlay owner=%s comp=%s"),
			*NowShort(), *GetNameSafe(GetOwner()), *GetNameSafe(this));
		DebugDumpState(TEXT("BeginPlay"));
	}
}

void URealtimeVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bEnableVerboseLog)
	{
		UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime] EndPlay owner=%s reason=%d"),
			*NowShort(), *GetNameSafe(GetOwner()), (int32)EndPlayReason);
		DebugDumpState(TEXT("EndPlay"));
	}

	Disconnect();
	Super::EndPlay(EndPlayReason);
}

bool URealtimeVoiceComponent::IsConnected() const
{
	return Socket.IsValid() && Socket->IsConnected();
}

FString URealtimeVoiceComponent::BuildWebSocketUrl() const
{
	return FString::Printf(TEXT("wss://api.openai.com/v1/realtime?model=%s"), *RealtimeModel);
}

FString URealtimeVoiceComponent::ResolveKeyPath(const FString& InPath) const
{
	if (FPaths::IsRelative(InPath))
	{
		return FPaths::ConvertRelativePathToFull(FPlatformProcess::UserDir() / InPath);
	}
	return InPath;
}

FString URealtimeVoiceComponent::MaskKeyForLog(const FString& Key)
{
	if (Key.Len() <= 10)
		return TEXT("<too-short>");
	return FString::Printf(TEXT("%s...%s (len=%d)"),
		*Key.Left(6), *Key.Right(4), Key.Len());
}

FString URealtimeVoiceComponent::NowShort()
{
	return FDateTime::Now().ToString(TEXT("%H:%M:%S.%s"));
}

FString URealtimeVoiceComponent::TruncateForLog(const FString& S, int32 MaxLen)
{
	if (S.Len() <= MaxLen) return S;
	return S.Left(MaxLen) + TEXT("...<truncated>");
}

FString URealtimeVoiceComponent::JsonToString(const TSharedPtr<FJsonObject>& Obj, bool bPretty)
{
	FString Out;
	if (!Obj.IsValid())
		return TEXT("<null json>");

	if (bPretty)
	{
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	}
	else
	{
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	}
	return Out;
}

void URealtimeVoiceComponent::DebugDumpState(const TCHAR* Tag) const
{
	if (!bEnableVerboseLog) return;

	UE_LOG(LogRealtimeVoice, Log,
		TEXT("[%s][Realtime][%s] State: Connected=%d SessionCreated=%d Model=%s In=%dHz/%dch Out=%dHz/%dch Voice=%s VADDisabled=%d Gate=%d AllowAudio=%d DynCtxLen=%d BaseInstrLen=%d Counters: out=%lld in=%lld appends=%lld audioDeltas=%lld bytesAppended=%lld bytesOut=%lld"),
		*NowShort(), Tag,
		IsConnected() ? 1 : 0,
		bSessionCreated ? 1 : 0,
		*RealtimeModel,
		InputSampleRate, InputNumChannels,
		OutputSampleRate, OutputNumChannels,
		*VoiceName,
		bDisableVADForPTT ? 1 : 0,
		bGateOutputAudioToCreateResponse ? 1 : 0,
		bAllowServerAudio ? 1 : 0,
		DynamicContext.Len(),
		BaseInstructions.Len(),
		(long long)OutgoingEventCounter,
		(long long)IncomingEventCounter,
		(long long)AppendCounter,
		(long long)AudioDeltaCounter,
		(long long)TotalAppendedPcmBytes,
		(long long)TotalReceivedOutAudioBytes
	);
}

FString URealtimeVoiceComponent::LoadApiKeyMaybe(FString& OutResolvedPath, FString& OutError) const
{
	OutError.Reset();
	OutResolvedPath.Reset();

	if (!ApiKey.IsEmpty())
	{
		if (bEnableVerboseLog)
		{
			UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime] Using ApiKey from property: %s"),
				*NowShort(), *MaskKeyForLog(ApiKey));
		}
		return ApiKey;
	}

	const FString AbsPath = ResolveKeyPath(KeyFilePath);
	OutResolvedPath = AbsPath;

	FString Key;
	if (!FFileHelper::LoadFileToString(Key, *AbsPath))
	{
		OutError = FString::Printf(TEXT("Failed to load key file. Path=%s"), *AbsPath);
		return TEXT("");
	}

	Key.TrimStartAndEndInline();

	if (Key.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Key file is empty. Path=%s"), *AbsPath);
		return TEXT("");
	}

	if (bEnableVerboseLog)
	{
		UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime] Loaded ApiKey from file: %s | path=%s"),
			*NowShort(), *MaskKeyForLog(Key), *AbsPath);
	}

	return Key;
}

void URealtimeVoiceComponent::Connect()
{
	if (IsConnected())
	{
		if (bEnableVerboseLog)
		{
			UE_LOG(LogRealtimeVoice, Warning, TEXT("[%s][Realtime] Connect() called but already connected."), *NowShort());
		}
		return;
	}

	ConnectStartTimeSec = FPlatformTime::Seconds();

	FString ResolvedPath, KeyError;
	const FString Key = LoadApiKeyMaybe(ResolvedPath, KeyError);
	if (Key.IsEmpty())
	{
		const FString Msg = FString::Printf(TEXT("OpenAI API Key missing. %s"), *KeyError);
		UE_LOG(LogRealtimeVoice, Error, TEXT("[%s][Realtime] %s"), *NowShort(), *Msg);
		OnError.Broadcast(Msg);
		return;
	}

	const FString Url = BuildWebSocketUrl();

	if (bEnableVerboseLog)
	{
		UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime] Connecting... url=%s"), *NowShort(), *Url);
		UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime] Key source: %s"),
			*NowShort(), ApiKey.IsEmpty() ? *ResolvedPath : TEXT("<ApiKey property>"));
		DebugDumpState(TEXT("PreConnect"));
	}

	FWebSocketsModule& WsModule = FWebSocketsModule::Get();

	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Key));

	Socket = WsModule.CreateWebSocket(Url, TEXT(""), Headers);

	Socket->OnConnected().AddUObject(this, &URealtimeVoiceComponent::HandleWsConnected);
	Socket->OnConnectionError().AddUObject(this, &URealtimeVoiceComponent::HandleWsConnectionError);
	Socket->OnClosed().AddUObject(this, &URealtimeVoiceComponent::HandleWsClosed);
	Socket->OnMessage().AddUObject(this, &URealtimeVoiceComponent::HandleWsMessage);

	Socket->Connect();
}

void URealtimeVoiceComponent::Disconnect()
{
	if (bEnableVerboseLog)
	{
		UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime] Disconnect() called. SocketValid=%d Connected=%d"),
			*NowShort(),
			Socket.IsValid() ? 1 : 0,
			IsConnected() ? 1 : 0);
	}

	if (Socket.IsValid())
	{
		Socket->Close();
		Socket.Reset();
	}

	bAllowServerAudio = false;
	bDidStartAudio = false;

	bSessionCreated = false;
	bPendingInitialSessionUpdate = false;

	OnConnected.Broadcast(false);

	if (bEnableVerboseLog)
	{
		DebugDumpState(TEXT("Disconnected"));
	}
}

void URealtimeVoiceComponent::HandleWsConnected()
{
	const double ElapsedMs = (FPlatformTime::Seconds() - ConnectStartTimeSec) * 1000.0;

	UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime] WS Connected. %.1f ms"), *NowShort(), ElapsedMs);

	OutgoingEventCounter = 0;
	IncomingEventCounter = 0;
	AppendCounter = 0;
	AudioDeltaCounter = 0;
	TotalAppendedPcmBytes = 0;
	TotalReceivedOutAudioBytes = 0;

	bDidStartAudio = false;
	bAllowServerAudio = false;

	// 세션 라이프사이클
	bSessionCreated = false;
	bPendingInitialSessionUpdate = true; // session.created 받은 뒤 update 보냄

	OnConnected.Broadcast(true);

	// NOTE: 여기서 session.update를 즉시 보내지 않습니다.
	DebugDumpState(TEXT("PostConnected"));
}

void URealtimeVoiceComponent::HandleWsConnectionError(const FString& Error)
{
	UE_LOG(LogRealtimeVoice, Error, TEXT("[%s][Realtime] WS ConnectionError: %s"), *NowShort(), *Error);
	OnError.Broadcast(Error);
	OnConnected.Broadcast(false);
	DebugDumpState(TEXT("ConnError"));
}

void URealtimeVoiceComponent::HandleWsClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	UE_LOG(LogRealtimeVoice, Warning, TEXT("[%s][Realtime] WS Closed. code=%d clean=%d reason=%s"),
		*NowShort(), StatusCode, bWasClean ? 1 : 0, *Reason);

	OnConnected.Broadcast(false);

	bAllowServerAudio = false;
	bDidStartAudio = false;

	bSessionCreated = false;
	bPendingInitialSessionUpdate = false;

	DebugDumpState(TEXT("Closed"));
}

void URealtimeVoiceComponent::SendJsonEvent(const TSharedPtr<FJsonObject>& Obj, const TCHAR* DebugTag)
{
	if (!IsConnected())
	{
		if (bEnableVerboseLog)
		{
			UE_LOG(LogRealtimeVoice, Warning, TEXT("[%s][Realtime] SendJsonEvent(%s) skipped: not connected."), *NowShort(), DebugTag);
		}
		return;
	}

	++OutgoingEventCounter;

	FString Out = JsonToString(Obj, false);

	if (bEnableVerboseLog)
	{
		FString TypeStr = TEXT("<no type>");
		if (Obj.IsValid() && Obj->HasTypedField<EJson::String>(TEXT("type")))
		{
			TypeStr = Obj->GetStringField(TEXT("type"));
		}

		UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime][TX #%lld][%s] type=%s bytes=%d"),
			*NowShort(), (long long)OutgoingEventCounter, DebugTag, *TypeStr, Out.Len());

		if (bLogOutgoingJson)
		{
			UE_LOG(LogRealtimeVoice, Verbose, TEXT("[%s][Realtime][TX JSON #%lld][%s]\n%s"),
				*NowShort(), (long long)OutgoingEventCounter, DebugTag, *TruncateForLog(Out, 4000));
		}
	}

	Socket->Send(Out);
}

FString URealtimeVoiceComponent::BuildInstructionsMerged() const
{
	FString Instr = BaseInstructions;
	if (!DynamicContext.IsEmpty())
	{
		Instr += TEXT("\n\n[GAME_STATE]\n");
		Instr += DynamicContext;
	}
	return Instr;
}

void URealtimeVoiceComponent::SendSessionUpdate(const TCHAR* ReasonTag)
{
	const TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("type"), TEXT("session.update"));

	const TSharedPtr<FJsonObject> Session = MakeShared<FJsonObject>();
	Session->SetStringField(TEXT("type"), TEXT("realtime"));
	Session->SetStringField(TEXT("model"), RealtimeModel);

	// output modalities
	{
		TArray<TSharedPtr<FJsonValue>> Modalities;
		Modalities.Add(MakeShared<FJsonValueString>(TEXT("audio")));
		Session->SetArrayField(TEXT("output_modalities"), Modalities);
	}

	// audio object
	{
		const TSharedPtr<FJsonObject> Audio = MakeShared<FJsonObject>();

		// input
		{
			const TSharedPtr<FJsonObject> In = MakeShared<FJsonObject>();
			const TSharedPtr<FJsonObject> InFormat = MakeShared<FJsonObject>();
			InFormat->SetStringField(TEXT("type"), TEXT("audio/pcm"));
			InFormat->SetNumberField(TEXT("rate"), InputSampleRate);
			In->SetObjectField(TEXT("format"), InFormat);

			// 수동 PTT: VAD 사용 안 함
			In->SetField(TEXT("turn_detection"), MakeShared<FJsonValueNull>());

			Audio->SetObjectField(TEXT("input"), In);
		}

		// output
		{
			const TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
			const TSharedPtr<FJsonObject> OutFormat = MakeShared<FJsonObject>();
			OutFormat->SetStringField(TEXT("type"), TEXT("audio/pcm"));
			Out->SetObjectField(TEXT("format"), OutFormat);
			Out->SetStringField(TEXT("voice"), VoiceName);
			Audio->SetObjectField(TEXT("output"), Out);
		}

		Session->SetObjectField(TEXT("audio"), Audio);
	}

	// instructions
	const FString Instr = BuildInstructionsMerged();
	Session->SetStringField(TEXT("instructions"), Instr);

	Root->SetObjectField(TEXT("session"), Session);

	if (bEnableVerboseLog)
	{
		UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime] SendSessionUpdate(%s) model=%s in=%dHz/%dch out=%dHz/%dch voice=%s instrLen=%d"),
			*NowShort(),
			ReasonTag,
			*RealtimeModel,
			InputSampleRate, InputNumChannels,
			OutputSampleRate, OutputNumChannels,
			*VoiceName,
			Instr.Len());
	}

	SendJsonEvent(Root, TEXT("session.update"));
}

void URealtimeVoiceComponent::UpdateDynamicContext(const FString& NewContext)
{
	DynamicContext = NewContext;

	if (bEnableVerboseLog)
	{
		UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime] UpdateDynamicContext len=%d (connected=%d sessionCreated=%d)"),
			*NowShort(), DynamicContext.Len(), IsConnected() ? 1 : 0, bSessionCreated ? 1 : 0);
	}

	// session.created 이후에만 update (그 전엔 pending으로 둠)
	if (IsConnected() && bSessionCreated)
	{
		SendSessionUpdate(TEXT("UpdateDynamicContext"));
	}
	else
	{
		// 연결은 됐는데 세션이 아직이면, created 이후에 한번 최신 컨텍스트로 update 하도록
		if (IsConnected())
		{
			bPendingInitialSessionUpdate = true;
		}
	}
}

void URealtimeVoiceComponent::SetAllowServerAudio(bool bAllow)
{
	bAllowServerAudio = bAllow;

	if (bEnableVerboseLog)
	{
		UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime] SetAllowServerAudio=%d"), *NowShort(), bAllow ? 1 : 0);
	}
}

void URealtimeVoiceComponent::BeginUserTurn(bool bCancelOngoingResponse, bool bClearOutputAudio)
{
	if (bEnableVerboseLog)
	{
		UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime] BeginUserTurn cancel=%d clearOut=%d connected=%d"),
			*NowShort(), bCancelOngoingResponse ? 1 : 0, bClearOutputAudio ? 1 : 0, IsConnected() ? 1 : 0);
	}

	if (!IsConnected())
		return;

	// 새 턴 시작(PTT 다운) 시 서버 오디오 잠금
	if (bGateOutputAudioToCreateResponse)
	{
		bAllowServerAudio = false;
	}

	// 새 턴 시작이니 입력 누적 카운터 리셋(빈 commit 방지용)
	AppendCounter = 0;
	TotalAppendedPcmBytes = 0;

	if (bCancelOngoingResponse)
	{
		const TSharedPtr<FJsonObject> Cancel = MakeShared<FJsonObject>();
		Cancel->SetStringField(TEXT("type"), TEXT("response.cancel"));
		SendJsonEvent(Cancel, TEXT("response.cancel"));
	}

	{
		const TSharedPtr<FJsonObject> ClearIn = MakeShared<FJsonObject>();
		ClearIn->SetStringField(TEXT("type"), TEXT("input_audio_buffer.clear"));
		SendJsonEvent(ClearIn, TEXT("input_audio_buffer.clear"));
	}

	if (bClearOutputAudio)
	{
		const TSharedPtr<FJsonObject> ClearOut = MakeShared<FJsonObject>();
		ClearOut->SetStringField(TEXT("type"), TEXT("output_audio_buffer.clear"));
		SendJsonEvent(ClearOut, TEXT("output_audio_buffer.clear"));
		bDidStartAudio = false;
	}
}

void URealtimeVoiceComponent::AppendInputAudioPCM16(const TArray<uint8>& Pcm16Bytes)
{
	if (!IsConnected())
	{
		if (bEnableVerboseLog)
		{
			UE_LOG(LogRealtimeVoice, Warning, TEXT("[%s][Realtime] AppendInputAudioPCM16 skipped: not connected."), *NowShort());
		}
		return;
	}

	if (Pcm16Bytes.Num() <= 0)
		return;

	++AppendCounter;
	TotalAppendedPcmBytes += Pcm16Bytes.Num();

	const FString B64 = FBase64::Encode(Pcm16Bytes);

	const TSharedPtr<FJsonObject> Ev = MakeShared<FJsonObject>();
	Ev->SetStringField(TEXT("type"), TEXT("input_audio_buffer.append"));
	Ev->SetStringField(TEXT("audio"), B64);

	if (bEnableVerboseLog)
	{
		const bool bLogThis = (LogEveryNAppends > 0) ? ((AppendCounter % LogEveryNAppends) == 0) : false;
		if (bLogThis)
		{
			UE_LOG(LogRealtimeVoice, Log,
				TEXT("[%s][Realtime] Append #%lld pcmBytes=%d totalPcmBytes=%lld b64Len=%d (~%.2fs @ %dHz mono)"),
				*NowShort(),
				(long long)AppendCounter,
				Pcm16Bytes.Num(),
				(long long)TotalAppendedPcmBytes,
				B64.Len(),
				(InputSampleRate > 0) ? (double)(Pcm16Bytes.Num() / 2) / (double)InputSampleRate : 0.0,
				InputSampleRate);
		}
	}

	SendJsonEvent(Ev, TEXT("input_audio_buffer.append"));
}

void URealtimeVoiceComponent::CommitInputAudio()
{
	if (!IsConnected())
	{
		if (bEnableVerboseLog)
		{
			UE_LOG(LogRealtimeVoice, Warning, TEXT("[%s][Realtime] CommitInputAudio skipped: not connected."), *NowShort());
		}
		return;
	}

	// 빈 버퍼 commit 방지 (이게 "응답 안 옴" 체감 원인 1순위)
	if (TotalAppendedPcmBytes <= 0)
	{
		const FString Msg = TEXT("CommitInputAudio skipped: no audio appended.");
		UE_LOG(LogRealtimeVoice, Warning, TEXT("[%s][Realtime] %s"), *NowShort(), *Msg);
		OnError.Broadcast(Msg);
		return;
	}

	const TSharedPtr<FJsonObject> Ev = MakeShared<FJsonObject>();
	Ev->SetStringField(TEXT("type"), TEXT("input_audio_buffer.commit"));

	if (bEnableVerboseLog)
	{
		UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime] CommitInputAudio() bytes=%lld"), *NowShort(), (long long)TotalAppendedPcmBytes);
	}

	SendJsonEvent(Ev, TEXT("input_audio_buffer.commit"));
}

void URealtimeVoiceComponent::CreateResponse()
{
	if (!IsConnected())
	{
		if (bEnableVerboseLog)
		{
			UE_LOG(LogRealtimeVoice, Warning, TEXT("[%s][Realtime] CreateResponse skipped: not connected."), *NowShort());
		}
		return;
	}

	// (옵션) instructions를 response.create에 포함해서 적용 여부를 강제 검증
	if (bIncludeInstructionsOnResponseCreate)
	{
		const TSharedPtr<FJsonObject> Ev = MakeShared<FJsonObject>();
		Ev->SetStringField(TEXT("type"), TEXT("response.create"));

		const TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();

		{
			TArray<TSharedPtr<FJsonValue>> Modalities;
			Modalities.Add(MakeShared<FJsonValueString>(TEXT("audio")));
			Resp->SetArrayField(TEXT("output_modalities"), Modalities);
		}

		Resp->SetStringField(TEXT("instructions"), BuildInstructionsMerged());

		Ev->SetObjectField(TEXT("response"), Resp);

		if (bEnableVerboseLog)
		{
			UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime] CreateResponse() with instructions"), *NowShort());
		}

		SendJsonEvent(Ev, TEXT("response.create+instr"));
	}
	else
	{
		const TSharedPtr<FJsonObject> Ev = MakeShared<FJsonObject>();
		Ev->SetStringField(TEXT("type"), TEXT("response.create"));

		if (bEnableVerboseLog)
		{
			UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime] CreateResponse()"), *NowShort());
		}

		SendJsonEvent(Ev, TEXT("response.create"));
	}

	// 내가 응답을 요청한 턴부터만 서버 오디오 수신 허용
	if (bGateOutputAudioToCreateResponse)
	{
		bAllowServerAudio = true;
	}
}

void URealtimeVoiceComponent::HandleWsMessage(const FString& Message)
{
	++IncomingEventCounter;

	if (bEnableVerboseLog && bLogIncomingJson)
	{
		UE_LOG(LogRealtimeVoice, Verbose, TEXT("[%s][Realtime][RX RAW #%lld] %s"),
			*NowShort(), (long long)IncomingEventCounter, *TruncateForLog(Message, 5000));
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		if (bEnableVerboseLog)
		{
			UE_LOG(LogRealtimeVoice, Warning, TEXT("[%s][Realtime][RX #%lld] JSON parse failed. len=%d"),
				*NowShort(), (long long)IncomingEventCounter, Message.Len());
		}
		return;
	}

	if (bEnableVerboseLog && bLogIncomingSummary)
	{
		FString TypeStr = TEXT("<no type>");
		if (Root->HasTypedField<EJson::String>(TEXT("type")))
		{
			TypeStr = Root->GetStringField(TEXT("type"));
		}

		FString Extra;

		if (TypeStr == TEXT("error") && Root->HasField(TEXT("error")))
		{
			const TSharedPtr<FJsonObject> Err = Root->GetObjectField(TEXT("error"));
			if (Err.IsValid() && Err->HasTypedField<EJson::String>(TEXT("message")))
			{
				Extra = Err->GetStringField(TEXT("message"));
			}
		}
		else if (TypeStr.EndsWith(TEXT(".delta")))
		{
			if (Root->HasTypedField<EJson::String>(TEXT("delta")))
			{
				Extra = FString::Printf(TEXT("deltaLen=%d"), Root->GetStringField(TEXT("delta")).Len());
			}
			else if (Root->HasTypedField<EJson::String>(TEXT("transcript")))
			{
				Extra = FString::Printf(TEXT("transcriptLen=%d"), Root->GetStringField(TEXT("transcript")).Len());
			}
		}

		UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime][RX #%lld] type=%s %s"),
			*NowShort(), (long long)IncomingEventCounter, *TypeStr,
			Extra.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("| %s"), *Extra));
	}

	HandleServerEvent(Root);
}

void URealtimeVoiceComponent::HandleServerEvent(const TSharedPtr<FJsonObject>& Root)
{
	const FString Type = Root->HasTypedField<EJson::String>(TEXT("type")) ? Root->GetStringField(TEXT("type")) : TEXT("<no type>");

	// session lifecycle: created 이후에만 session.update 전송 (프롬프트/포맷 안정화)
	if (Type == TEXT("session.created"))
	{
		bSessionCreated = true;

		if (bEnableVerboseLog)
		{
			UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime] session.created received"), *NowShort());
		}

		if (bPendingInitialSessionUpdate)
		{
			bPendingInitialSessionUpdate = false;
			SendSessionUpdate(TEXT("AfterSessionCreated"));
		}
		return;
	}

	if (Type == TEXT("session.updated"))
	{
		// 로깅만
		if (bEnableVerboseLog)
		{
			UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime] session.updated"), *NowShort());
		}
		// TextEvent도 그대로 흘려주고 싶으면 아래로 내려도 됨
	}

	// ---- Output audio delta (PCM16 b64) ----
	if (Type == TEXT("response.output_audio.delta"))
	{
		if (bGateOutputAudioToCreateResponse && !bAllowServerAudio)
		{
			if (bEnableVerboseLog)
			{
				UE_LOG(LogRealtimeVoice, VeryVerbose, TEXT("[%s][Realtime] output_audio.delta ignored (gate closed)"), *NowShort());
			}
			return;
		}

		++AudioDeltaCounter;

		const FString DeltaB64 = Root->GetStringField(TEXT("delta"));
		TArray<uint8> Bytes;
		const bool bOk = FBase64::Decode(DeltaB64, Bytes);

		if (!bOk)
		{
			UE_LOG(LogRealtimeVoice, Warning, TEXT("[%s][Realtime] output_audio.delta base64 decode failed. deltaLen=%d"),
				*NowShort(), DeltaB64.Len());
			return;
		}

		TotalReceivedOutAudioBytes += Bytes.Num();

		const bool bLogThis = (LogEveryNAudioDeltas > 0) ? ((AudioDeltaCounter % LogEveryNAudioDeltas) == 0) : false;
		if (bEnableVerboseLog && bLogThis)
		{
			UE_LOG(LogRealtimeVoice, Log,
				TEXT("[%s][Realtime] AudioDelta #%lld bytes=%d totalOutBytes=%lld"),
				*NowShort(),
				(long long)AudioDeltaCounter,
				Bytes.Num(),
				(long long)TotalReceivedOutAudioBytes);
		}

		if (!bDidStartAudio)
		{
			bDidStartAudio = true;
			OnAudioStarted.Broadcast();
		}

		// NOTE: 서버 델타가 실제로 몇 Hz/채널인지는 이벤트에 따로 안 오므로,
		// 로컬 힌트(OutputSampleRate/NumChannels)를 내려줍니다.
		OnOutputAudioDelta.Broadcast(Bytes, OutputSampleRate, OutputNumChannels);
		return;
	}

	if (Type == TEXT("response.output_audio.done"))
	{
		if (bEnableVerboseLog)
		{
			UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime] output_audio.done"), *NowShort());
		}

		if (bDidStartAudio)
		{
			bDidStartAudio = false;
			OnAudioEnded.Broadcast();
		}

		OnOutputAudioDone.Broadcast();
		return;
	}

	// ---- Helpful text-ish events (로깅/디버그용) ----
	if (Type == TEXT("response.output_text.delta") ||
		Type == TEXT("response.output_text.done") ||
		Type == TEXT("response.output_audio_transcript.delta") ||
		Type == TEXT("response.output_audio_transcript.done") ||
		Type == TEXT("conversation.item.input_audio_transcription.completed") ||
		Type == TEXT("session.updated"))
	{
		FString Payload = Type;

		if (Root->HasTypedField<EJson::String>(TEXT("delta")))
		{
			Payload += TEXT(" | ");
			Payload += Root->GetStringField(TEXT("delta"));
		}
		if (Root->HasTypedField<EJson::String>(TEXT("transcript")))
		{
			Payload += TEXT(" | ");
			Payload += Root->GetStringField(TEXT("transcript"));
		}

		if (bEnableVerboseLog)
		{
			UE_LOG(LogRealtimeVoice, Log, TEXT("[%s][Realtime] TextEvent: %s"),
				*NowShort(), *TruncateForLog(Payload, 1200));
		}

		OnTextEvent.Broadcast(Payload);
		return;
	}

	// ---- Error ----
	if (Type == TEXT("error"))
	{
		FString Msg = TEXT("Realtime error");
		if (Root->HasField(TEXT("error")))
		{
			const TSharedPtr<FJsonObject> ErrObj = Root->GetObjectField(TEXT("error"));
			if (ErrObj.IsValid() && ErrObj->HasTypedField<EJson::String>(TEXT("message")))
			{
				Msg = ErrObj->GetStringField(TEXT("message"));
			} 
		}

		UE_LOG(LogRealtimeVoice, Error, TEXT("[%s][Realtime] ERROR event: %s"), *NowShort(), *Msg);
		OnError.Broadcast(Msg);
		return;
	}

	if (bEnableVerboseLog && bLogIncomingSummary)
	{
		UE_LOG(LogRealtimeVoice, VeryVerbose, TEXT("[%s][Realtime] Unhandled event type=%s"), *NowShort(), *Type);
	}
}
