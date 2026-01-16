// VoicePTTRealtimeActor.cpp
#include "VoicePTTRealtimeActor.h"

#include "PTTAudioRecorderComponent.h"
#include "RealtimeVoiceComponent.h"
#include "RadioManager.h"

#include "Components/SceneComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#include "TimerManager.h"
#include "Engine/World.h"

AVoicePTTRealtimeActor::AVoicePTTRealtimeActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	PTT = CreateDefaultSubobject<UPTTAudioRecorderComponent>(TEXT("PTT"));
	Realtime = CreateDefaultSubobject<URealtimeVoiceComponent>(TEXT("Realtime"));
}

void AVoicePTTRealtimeActor::BeginPlay()
{
	Super::BeginPlay();

	EnsureComponentsBound();

	if (!PTT || !Realtime)
	{
		UE_LOG(LogTemp, Error, TEXT("[VoicePTT-RT] Missing components. PTT=%s Realtime=%s"),
			*GetNameSafe(PTT), *GetNameSafe(Realtime));
		return;
	}

	// Bind capture events
	PTT->OnPcm16FrameReady.AddUniqueDynamic(this, &AVoicePTTRealtimeActor::HandlePcm16FrameReady);
	PTT->OnCaptureFinalized.AddUniqueDynamic(this, &AVoicePTTRealtimeActor::HandleCaptureFinalized);

	// Bind realtime output -> RadioManager playback (필수)
	Realtime->OnOutputAudioDelta.AddUniqueDynamic(this, &AVoicePTTRealtimeActor::HandleRealtimeOutputAudioDelta);
	Realtime->OnOutputAudioDone.AddUniqueDynamic(this, &AVoicePTTRealtimeActor::HandleRealtimeOutputAudioDone);

	// Radio
	RadioManager.Reset();
	if (ARadioManager* RM = ARadioManager::GetRadioManager(this))
	{
		RadioManager = RM;
		RM->OnBusyChanged.AddUniqueDynamic(this, &AVoicePTTRealtimeActor::HandleRadioBusyChanged);
	}

	if (bAutoConnectOnBeginPlay)
	{
		Realtime->Connect();
	}
}

void AVoicePTTRealtimeActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(StartCaptureTimer);
	}

	StopStaticLoop();

	// 진행 중인 송출 정리
	EndRadioRealtime();

	Super::EndPlay(EndPlayReason);
}

void AVoicePTTRealtimeActor::EnsureComponentsBound()
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

	UE_LOG(LogTemp, Warning, TEXT("[VoicePTT-RT] EnsureComponentsBound: this=%s PTT=%s Realtime=%s"),
		*GetNameSafe(this), *GetNameSafe(PTT), *GetNameSafe(Realtime));
}

ARadioManager* AVoicePTTRealtimeActor::GetRadioManagerCached()
{
	ARadioManager* RM = RadioManager.Get();
	if (!RM)
	{
		RM = ARadioManager::GetRadioManager(this);
		RadioManager = RM;
	}
	return RM;
}

void AVoicePTTRealtimeActor::UpdateGameStateForAI(const FString& NewSnapshot)
{
	CachedGameState = NewSnapshot;
	if (Realtime && Realtime->IsConnected())
	{
		Realtime->UpdateDynamicContext(CachedGameState);
	}
}

void AVoicePTTRealtimeActor::StartPTT()
{
	EnsureComponentsBound();
	if (!PTT || !Realtime)
		return;

	if (bPTTActive)
		return;

	// Radio busy block (큐/무전 재생 중이면 막기)
	if (bBlockPTTWhenRadioBusy)
	{
		if (ARadioManager* RM = GetRadioManagerCached())
		{
			if (RM->IsBusy())
			{
				PlayBusyWarning();
				return;
			}
		}
	}

	// UI/SFX
	PlayPTTStartSfx();
	StartStaticLoop();

	bPTTActive = true;

	// New user turn: cancel old response, clear buffers
	Realtime->BeginUserTurn(true, true);

	// Push latest game-state
	if (!CachedGameState.IsEmpty())
	{
		Realtime->UpdateDynamicContext(CachedGameState);
	}

	// capture delay (삑/노이즈 유입 방지)
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
			&AVoicePTTRealtimeActor::StartCaptureInternal,
			StartCaptureDelaySeconds,
			false
		);
	}
}

void AVoicePTTRealtimeActor::StartCaptureInternal()
{
	EnsureComponentsBound();

	if (!PTT)
	{
		UE_LOG(LogTemp, Error, TEXT("[VoicePTT-RT] StartCaptureInternal: PTT is null"));
		bPTTActive = false;
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[VoicePTT-RT] Calling PTT->StartPTT"));
	PTT->StartPTT();
}

void AVoicePTTRealtimeActor::StopPTT()
{
	EnsureComponentsBound();

	const bool bWasActive = bPTTActive;
	bPTTActive = false;

	UE_LOG(LogTemp, Warning, TEXT("[VoicePTT-RT] StopPTT: this=%s WasActive=%d PTT=%s RealtimeConnected=%d"),
		*GetNameSafe(this),
		bWasActive ? 1 : 0,
		*GetNameSafe(PTT),
		(Realtime && Realtime->IsConnected()) ? 1 : 0);

	if (!bWasActive)
	{
		// 이미 stop 상태면(키업 중복 등) 추가 처리 X
		return;
	}

	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(StartCaptureTimer);
	}

	if (PTT)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoicePTT-RT] Calling PTT->StopPTT"));
		PTT->StopPTT(); // <- WAV 없음, 스트림 모드
	}

	StopStaticLoop();
	PlayPTTEndSfx();

	// 여기서부터 "AI 응답 송출" 시작
	// (RadioManager를 통해 재생해야 이펙트가 먹음)
	if (Realtime && Realtime->IsConnected())
	{
		BeginRadioRealtime();

		if (bCommitAndCreateImmediatelyOnStop)
		{
			Realtime->CommitInputAudio();
			Realtime->CreateResponse(); // <- 이 호출 이후에만 output_audio.delta가 살아남(게이트)
		}
	}
}

void AVoicePTTRealtimeActor::BeginRadioRealtime()
{
	ARadioManager* RM = GetRadioManagerCached();
	if (!RM)
		return;

	FRadioSubtitleInfomation Info;
	Info.SpeakerId = AISpeakerId;
	Info.SpeakerName = AISpeakerName;
	Info.SubtitleText = FText::GetEmpty(); // transcript 붙일 거면 여기 갱신

	// bInterruptIfBusy=true : 혹시 남아있는 큐 재생이 있으면 끊고 Realtime로 전환
	RM->BeginRealtimeTransmission(Info, true);
}

void AVoicePTTRealtimeActor::EndRadioRealtime()
{
	if (ARadioManager* RM = GetRadioManagerCached())
	{
		if (RM->IsRealtimeTransmitting())
		{
			RM->EndRealtimeTransmission(true);
		}
	}
}

void AVoicePTTRealtimeActor::HandlePcm16FrameReady(const TArray<uint8>& Pcm16BytesLE, int32 SampleRate, int32 NumChannels, float FrameDurationSec)
{
	// streaming append
	if (!Realtime || !Realtime->IsConnected())
		return;

	Realtime->AppendInputAudioPCM16(Pcm16BytesLE);
}

void AVoicePTTRealtimeActor::HandleCaptureFinalized(bool bSuccess, float TotalDurationSec, const FString& ErrorOrInfo)
{
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoicePTT-RT] CaptureFinalized failed: %s"), *ErrorOrInfo);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[VoicePTT-RT] CaptureFinalized ok: dur=%.2fs info=%s"), TotalDurationSec, *ErrorOrInfo);

	// 만약 Stop에서 CreateResponse를 안 하고 여기서 하고 싶으면:
	// if (Realtime && Realtime->IsConnected()) { BeginRadioRealtime(); Realtime->CommitInputAudio(); Realtime->CreateResponse(); }
}

void AVoicePTTRealtimeActor::HandleRadioBusyChanged(bool bBusy)
{
	if (!bBusy)
		return;

	PlayBusyWarning();
	StopPTT();
}

// ===== Realtime -> RadioManager =====

void AVoicePTTRealtimeActor::HandleRealtimeOutputAudioDelta(const TArray<uint8>& Pcm16LE, int32 SampleRate, int32 NumChannels)
{
	ARadioManager* RM = GetRadioManagerCached();
	if (!RM)
		return;

	// 이펙트/무전 톤은 RadioManager의 VoiceAudioComp 체인에서 처리됨
	RM->AppendRealtimePcm16(Pcm16LE, SampleRate, NumChannels);
}

void AVoicePTTRealtimeActor::HandleRealtimeOutputAudioDone()
{
	// 응답이 끝났을 때 무전 종료 톤 + Busy false + 큐 이어서
	EndRadioRealtime();
}

// ------------------- SFX -------------------

void AVoicePTTRealtimeActor::PlayPTTStartSfx()
{
	if (!SfxPTTStart) return;
	UGameplayStatics::PlaySound2D(this, SfxPTTStart);
}

void AVoicePTTRealtimeActor::PlayPTTEndSfx()
{
	if (!SfxPTTEnd) return;
	UGameplayStatics::PlaySound2D(this, SfxPTTEnd);
}

void AVoicePTTRealtimeActor::StartStaticLoop()
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

void AVoicePTTRealtimeActor::StopStaticLoop()
{
	if (!StaticLoopAC) return;
	StaticLoopAC->FadeOut(0.05f, 0.0f);
	StaticLoopAC = nullptr;
}

void AVoicePTTRealtimeActor::PlayBusyWarning()
{
	if (!SfxChannelBusyWarning) return;
	UGameplayStatics::PlaySound2D(this, SfxChannelBusyWarning);
}
