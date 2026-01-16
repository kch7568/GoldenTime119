#include "VoicePTTWhisperActor.h"

#include "PTTAudioRecorderComponent.h"
#include "RealtimeVoiceComponent.h"
#include "RadioManager.h"

#include "Components/SceneComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#include "TimerManager.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

static uint32 ReadLE32(const uint8* P)
{
	return (uint32)P[0] | ((uint32)P[1] << 8) | ((uint32)P[2] << 16) | ((uint32)P[3] << 24);
}
static uint16 ReadLE16(const uint8* P)
{
	return (uint16)P[0] | ((uint16)P[1] << 8);
}

AVoicePTTWhisperActor::AVoicePTTWhisperActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	PTT = CreateDefaultSubobject<UPTTAudioRecorderComponent>(TEXT("PTT"));
	Realtime = CreateDefaultSubobject<URealtimeVoiceComponent>(TEXT("Realtime"));
}

void AVoicePTTWhisperActor::BeginPlay()
{
	Super::BeginPlay();

	EnsureComponentsBound();

	if (!PTT || !Realtime)
	{
		UE_LOG(LogTemp, Error, TEXT("[VoicePTT] Missing components. PTT=%s Realtime=%s"),
			*GetNameSafe(PTT), *GetNameSafe(Realtime));
		return;
	}

	PTT->OnWavReady.AddUniqueDynamic(this, &AVoicePTTWhisperActor::HandleWavReady);

	// Radio busy
	RadioManager = nullptr;
	if (ARadioManager* RM = ARadioManager::GetRadioManager(this))
	{
		RadioManager = RM;
		RM->OnBusyChanged.AddUniqueDynamic(this, &AVoicePTTWhisperActor::HandleRadioBusyChanged);
	}

	// Realtime 연결(원하면 외부에서 수동 Connect해도 됨)
	Realtime->Connect();
}

void AVoicePTTWhisperActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(StartCaptureTimer);
	}

	StopStaticLoop();
	Super::EndPlay(EndPlayReason);
}

void AVoicePTTWhisperActor::EnsureComponentsBound()
{
	if (PTT && Realtime)
		return;

	if (!PTT)
	{
		if (UPTTAudioRecorderComponent* Found = FindComponentByClass<UPTTAudioRecorderComponent>())
		{
			PTT = Found;
		}
	}

	if (!Realtime)
	{
		if (URealtimeVoiceComponent* Found = FindComponentByClass<URealtimeVoiceComponent>())
		{
			Realtime = Found;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[VoicePTT] EnsureComponentsBound: this=%s PTT=%s Realtime=%s"),
		*GetNameSafe(this), *GetNameSafe(PTT), *GetNameSafe(Realtime));
}

void AVoicePTTWhisperActor::UpdateRealtimeGameState(const FString& ContextTextOrJson)
{
	EnsureComponentsBound();
	if (Realtime)
	{
		Realtime->UpdateDynamicContext(ContextTextOrJson);
	}
}

void AVoicePTTWhisperActor::StartPTT()
{
	EnsureComponentsBound();

	if (bPTTActive)
		return;

	if (bBlockPTTWhenRadioBusy)
	{
		ARadioManager* RM = RadioManager.Get();
		if (!RM)
		{
			RM = ARadioManager::GetRadioManager(this);
			RadioManager = RM;
		}

		if (RM && RM->IsBusy())
		{
			PlayBusyWarning();
			return;
		}
	}

	// Realtime: 새 턴 시작(입력/출력 버퍼 정리)
	if (Realtime && Realtime->IsConnected())
	{
		Realtime->BeginUserTurn(true, true);
	}

	PlayPTTStartSfx();
	StartStaticLoop();

	bPTTActive = true;
	bCaptureStarted = false;

	if (!PTT)
	{
		UE_LOG(LogTemp, Error, TEXT("[VoicePTT] StartPTT failed: PTT is null"));
		bPTTActive = false;
		return;
	}

	if (StartCaptureDelaySeconds <= 0.f)
	{
		StartCaptureInternal();
		return;
	}

	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(StartCaptureTimer);
		W->GetTimerManager().SetTimer(
			StartCaptureTimer,
			this,
			&AVoicePTTWhisperActor::StartCaptureInternal,
			StartCaptureDelaySeconds,
			false
		);
	}
}

void AVoicePTTWhisperActor::StartCaptureInternal()
{
	EnsureComponentsBound();

	if (!PTT)
	{
		UE_LOG(LogTemp, Error, TEXT("[VoicePTT] StartCaptureInternal: PTT is null"));
		bPTTActive = false;
		return;
	}

	bCaptureStarted = true;
	UE_LOG(LogTemp, Warning, TEXT("[VoicePTT] Calling PTT->StartPTT"));
	PTT->StartPTT();
}

void AVoicePTTWhisperActor::StopPTT()
{
	EnsureComponentsBound();

	const bool bWasActive = bPTTActive;
	bPTTActive = false;

	UE_LOG(LogTemp, Warning, TEXT("[VoicePTT] StopPTT: this=%s WasActive=%d PTT=%s"),
		*GetNameSafe(this),
		bWasActive ? 1 : 0,
		*GetNameSafe(PTT));

	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(StartCaptureTimer);
	}

	if (PTT)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoicePTT] Calling PTT->StopPTTAndSave"));
		PTT->StopPTTAndSave(TEXT(""));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[VoicePTT] StopPTT: PTT is null"));
	}

	StopStaticLoop();
	PlayPTTEndSfx();
}

void AVoicePTTWhisperActor::HandleRadioBusyChanged(bool bBusy)
{
	if (!bBusy)
		return;

	PlayBusyWarning();
	StopPTT();
}

void AVoicePTTWhisperActor::HandleWavReady(bool bSuccess, const FString& WavPathOrError)
{
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("[VoicePTT] WAV failed: %s"), *WavPathOrError);
		return;
	}

	EnsureComponentsBound();

	if (!Realtime || !Realtime->IsConnected())
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoicePTT] Realtime not connected. wav=%s"), *WavPathOrError);
		return;
	}

	TArray<uint8> Pcm16Bytes;
	int32 SampleRate = 0;
	int32 NumChannels = 0;
	FString Err;

	if (!ExtractPcm16FromWavFile(WavPathOrError, Pcm16Bytes, SampleRate, NumChannels, Err))
	{
		UE_LOG(LogTemp, Error, TEXT("[VoicePTT] WAV parse failed: %s"), *Err);
		return;
	}

	// (중요) session.update 입력 포맷과 SampleRate가 같아야 함.
	// 여기서는 RealtimeVoiceComponent 기본 InputSampleRate=48000로 맞춰둠.

	Realtime->AppendInputAudioPCM16(Pcm16Bytes);
	Realtime->CommitInputAudio(); // VAD 꺼져있으면 수동 :contentReference[oaicite:15]{index=15}
	Realtime->CreateResponse();   // VAD 꺼져있으면 수동 :contentReference[oaicite:16]{index=16}

	UE_LOG(LogTemp, Log, TEXT("[VoicePTT] Sent PCM16 to Realtime. bytes=%d sr=%d ch=%d"),
		Pcm16Bytes.Num(), SampleRate, NumChannels);
}

bool AVoicePTTWhisperActor::ExtractPcm16FromWavFile(
	const FString& WavPath,
	TArray<uint8>& OutPcm16Bytes,
	int32& OutSampleRate,
	int32& OutNumChannels,
	FString& OutError)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *WavPath))
	{
		OutError = FString::Printf(TEXT("Failed to read wav: %s"), *WavPath);
		return false;
	}

	if (Bytes.Num() < 44)
	{
		OutError = TEXT("WAV too small");
		return false;
	}

	// RIFF header
	if (!(Bytes[0] == 'R' && Bytes[1] == 'I' && Bytes[2] == 'F' && Bytes[3] == 'F' &&
		Bytes[8] == 'W' && Bytes[9] == 'A' && Bytes[10] == 'V' && Bytes[11] == 'E'))
	{
		OutError = TEXT("Not a RIFF/WAVE");
		return false;
	}

	int32 Cursor = 12;

	uint16 AudioFormat = 0;
	uint16 NumChannels = 0;
	uint32 SampleRate = 0;
	uint16 BitsPerSample = 0;

	int32 DataOffset = -1;
	int32 DataSize = 0;

	while (Cursor + 8 <= Bytes.Num())
	{
		const uint8* P = Bytes.GetData() + Cursor;
		const uint32 ChunkId = ReadLE32(P);
		const uint32 ChunkSize = ReadLE32(P + 4);
		Cursor += 8;

		if (Cursor + (int32)ChunkSize > Bytes.Num())
			break;

		// 'fmt '
		if (ChunkId == 0x20746D66)
		{
			const uint8* F = Bytes.GetData() + Cursor;
			if (ChunkSize < 16)
			{
				OutError = TEXT("fmt chunk too small");
				return false;
			}
			AudioFormat = ReadLE16(F + 0);
			NumChannels = ReadLE16(F + 2);
			SampleRate = ReadLE32(F + 4);
			BitsPerSample = ReadLE16(F + 14);
		}
		// 'data'
		else if (ChunkId == 0x61746164)
		{
			DataOffset = Cursor;
			DataSize = (int32)ChunkSize;
			break;
		}

		// chunk alignment: pad to even
		Cursor += (int32)ChunkSize;
		if (Cursor & 1) Cursor++;
	}

	if (DataOffset < 0 || DataSize <= 0)
	{
		OutError = TEXT("No data chunk found");
		return false;
	}

	if (AudioFormat != 1 || BitsPerSample != 16)
	{
		OutError = FString::Printf(TEXT("Unsupported WAV format. AudioFormat=%d Bits=%d (need PCM16)"), AudioFormat, BitsPerSample);
		return false;
	}

	OutSampleRate = (int32)SampleRate;
	OutNumChannels = (int32)NumChannels;

	// Realtime에 보내는 건 “PCM16 raw bytes” (WAV 헤더 제외)
	OutPcm16Bytes.Reset();
	OutPcm16Bytes.Append(Bytes.GetData() + DataOffset, DataSize);

	// 만약 스테레오라면 여기서 mono downmix가 필요하지만,
	// 당신 PTT는 이미 mono로 저장 중(DesiredNumChannels=1)이라 그대로 가정.

	return true;
}

// ------------------- SFX -------------------

void AVoicePTTWhisperActor::PlayPTTStartSfx()
{
	if (!SfxPTTStart) return;
	UGameplayStatics::PlaySound2D(this, SfxPTTStart);
}

void AVoicePTTWhisperActor::PlayPTTEndSfx()
{
	if (!SfxPTTEnd) return;
	UGameplayStatics::PlaySound2D(this, SfxPTTEnd);
}

void AVoicePTTWhisperActor::StartStaticLoop()
{
	if (!SfxPTTStaticLoop) return;
	if (StaticLoopAC && StaticLoopAC->IsPlaying()) return;

	StaticLoopAC = UGameplayStatics::SpawnSound2D(
		this,
		SfxPTTStaticLoop,
		StaticLoopVolume,
		StaticLoopPitch
	);

	if (StaticLoopAC)
	{
		StaticLoopAC->bIsUISound = true;
	}
}

void AVoicePTTWhisperActor::StopStaticLoop()
{
	if (!StaticLoopAC) return;
	StaticLoopAC->FadeOut(0.05f, 0.0f);
	StaticLoopAC = nullptr;
}

void AVoicePTTWhisperActor::PlayBusyWarning()
{
	if (!SfxChannelBusyWarning) return;
	UGameplayStatics::PlaySound2D(this, SfxChannelBusyWarning);
}
