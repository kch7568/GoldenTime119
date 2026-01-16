#include "RadioManager.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

#include "Sound/SoundBase.h"
#include "Sound/SoundWaveProcedural.h"

#include "RadioLineData.h" // URadioLineData

ARadioManager::ARadioManager()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(RootComp);

	VoiceAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("VoiceAudioComp"));
	VoiceAudioComp->SetupAttachment(RootComponent);
	VoiceAudioComp->bAutoActivate = false;
	VoiceAudioComp->bIsUISound = true;

	LoopAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("LoopAudioComp"));
	LoopAudioComp->SetupAttachment(RootComponent);
	LoopAudioComp->bAutoActivate = false;
	LoopAudioComp->bIsUISound = true;

	SfxAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("SfxAudioComp"));
	SfxAudioComp->SetupAttachment(RootComponent);
	SfxAudioComp->bAutoActivate = false;
	SfxAudioComp->bIsUISound = true;

	VoiceAudioComp->OnAudioFinished.AddDynamic(this, &ARadioManager::OnVoiceFinished);
}

void ARadioManager::BeginPlay()
{
	Super::BeginPlay();
}

ARadioManager* ARadioManager::GetRadioManager(UObject* WorldContextObject)
{
	if (!WorldContextObject) return nullptr;

	AActor* Found = UGameplayStatics::GetActorOfClass(WorldContextObject, ARadioManager::StaticClass());
	return Cast<ARadioManager>(Found);
}

// =======================
// Clip queue
// =======================

void ARadioManager::EnqueueRadioLine(URadioLineData* LineData)
{
	if (!LineData || !LineData->VoiceSound)
		return;

	Queue.Add(LineData);

	if (!bIsPlaying && !bRealtimeActive)
	{
		TryPlayNextFromQueue();
	}
}

void ARadioManager::TryPlayNextFromQueue()
{
	if (bRealtimeActive)
		return;

	if (Queue.Num() == 0)
	{
		bIsPlaying = false;
		OnBusyChanged.Broadcast(false);
		CurrentLine = nullptr;
		PlayState = ERadioPlayState::Idle;
		return;
	}

	bIsPlaying = true;
	OnBusyChanged.Broadcast(true);

	CurrentLine = Queue[0];
	Queue.RemoveAt(0);

	PlayState = ERadioPlayState::StartTone;
	PlayStartTone();
}

void ARadioManager::PlayStartTone()
{
	ClearStateTimer();

	if (StartToneSound)
	{
		SfxAudioComp->SetSound(StartToneSound);
		SfxAudioComp->Play();

		const float Duration = StartToneSound->GetDuration();
		if (Duration > 0.f)
		{
			GetWorldTimerManager().SetTimer(StateTimerHandle, this, &ARadioManager::OnStartToneFinished, Duration, false);
			return;
		}
	}

	OnStartToneFinished();
}

void ARadioManager::OnStartToneFinished()
{
	PlayState = ERadioPlayState::PreDelay;
	PlayPreDelay();
}

void ARadioManager::PlayPreDelay()
{
	ClearStateTimer();

	if (CurrentLine && CurrentLine->PreDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(StateTimerHandle, this, &ARadioManager::OnPreDelayFinished, CurrentLine->PreDelay, false);
	}
	else
	{
		OnPreDelayFinished();
	}
}

void ARadioManager::OnPreDelayFinished()
{
	PlayState = ERadioPlayState::Voice;
	PlayVoice();
}

void ARadioManager::PlayVoice()
{
	ClearStateTimer();

	if (!CurrentLine || !CurrentLine->VoiceSound)
	{
		FinishCurrentAndContinue();
		return;
	}

	if (LoopNoiseSound)
	{
		LoopAudioComp->SetSound(LoopNoiseSound);
		LoopAudioComp->Play();
	}

	OnSubtitleBegin.Broadcast(CurrentLine->Subtitle);

	VoiceAudioComp->SetSound(CurrentLine->VoiceSound);
	VoiceAudioComp->Play();
}

void ARadioManager::OnVoiceFinished()
{
	// Realtime 중이면 신뢰하지 않음
	if (bRealtimeActive)
		return;

	if (CurrentLine)
	{
		OnSubtitleEnd.Broadcast(CurrentLine->Subtitle);
	}

	if (LoopAudioComp->IsPlaying())
	{
		LoopAudioComp->Stop();
	}

	PlayState = ERadioPlayState::PostDelay;
	PlayPostDelay();
}

void ARadioManager::PlayPostDelay()
{
	ClearStateTimer();

	if (CurrentLine && CurrentLine->PostDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(StateTimerHandle, this, &ARadioManager::OnPostDelayFinished, CurrentLine->PostDelay, false);
	}
	else
	{
		OnPostDelayFinished();
	}
}

void ARadioManager::OnPostDelayFinished()
{
	PlayState = ERadioPlayState::EndTone;
	PlayEndTone();
}

void ARadioManager::PlayEndTone()
{
	ClearStateTimer();

	if (EndToneSound)
	{
		SfxAudioComp->SetSound(EndToneSound);
		SfxAudioComp->Play();

		const float Duration = EndToneSound->GetDuration();
		if (Duration > 0.f)
		{
			GetWorldTimerManager().SetTimer(StateTimerHandle, this, &ARadioManager::OnEndToneFinished, Duration, false);
			return;
		}
	}

	OnEndToneFinished();
}

void ARadioManager::OnEndToneFinished()
{
	FinishCurrentAndContinue();
}

void ARadioManager::FinishCurrentAndContinue()
{
	ClearStateTimer();
	CurrentLine = nullptr;
	PlayState = ERadioPlayState::Idle;
	TryPlayNextFromQueue();
}

void ARadioManager::ClearStateTimer()
{
	if (StateTimerHandle.IsValid())
	{
		GetWorldTimerManager().ClearTimer(StateTimerHandle);
	}
}

// =======================
// Realtime streaming (fixed)
// =======================

void ARadioManager::InterruptAllPlayback_Internal(bool bClearQueue)
{
	ClearStateTimer();
	StopFlushTimer();

	if (VoiceAudioComp && VoiceAudioComp->IsPlaying())
	{
		VoiceAudioComp->Stop();
	}

	if (LoopAudioComp && LoopAudioComp->IsPlaying())
	{
		LoopAudioComp->Stop();
	}

	if (SfxAudioComp && SfxAudioComp->IsPlaying())
	{
		SfxAudioComp->Stop();
	}

	if (CurrentLine)
	{
		OnSubtitleEnd.Broadcast(CurrentLine->Subtitle);
		CurrentLine = nullptr;
	}

	if (bClearQueue)
	{
		Queue.Reset();
	}

	PlayState = ERadioPlayState::Idle;
	bIsPlaying = false;
	OnBusyChanged.Broadcast(false);
}

int32 ARadioManager::GetUseChannels(int32 InChannels) const
{
	int32 Use = FMath::Clamp(InChannels, 1, 2);
	if (bRealtimeForceMono)
	{
		Use = 1;
	}
	return Use;
}

int32 ARadioManager::CalcPrebufferBytes(int32 SampleRate, int32 NumChannels) const
{
	const float Sec = FMath::Max(0.0f, RealtimePrebufferMs) / 1000.0f;
	const int32 SR = FMath::Max(8000, SampleRate);
	const int32 CH = FMath::Clamp(NumChannels, 1, 2);
	const int32 Samples = (int32)FMath::RoundToInt(Sec * (float)SR);
	return Samples * CH * 2; // PCM16
}

USoundWaveProcedural* ARadioManager::EnsureRealtimeWave(int32 SampleRate, int32 NumChannels)
{
	const int32 TargetSR = FMath::Max(8000, SampleRate);
	const int32 TargetCH = FMath::Clamp(NumChannels, 1, 2);

	auto CreateWave = [&](const TCHAR* Name) -> USoundWaveProcedural*
		{
			USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(this, Name);
			W->SoundGroup = SOUNDGROUP_Voice;
			W->bCanProcessAsync = true;

			// "비면 끝" 판정 완화
			W->bLooping = true;
			W->Duration = INDEFINITELY_LOOPING_DURATION;

			W->NumChannels = (uint32)TargetCH;
			W->SetSampleRate(TargetSR);
			return W;
		};

	if (!RealtimeWave)
	{
		RealtimeWave = CreateWave(TEXT("RadioRealtimeWave"));
		return RealtimeWave;
	}

	if ((int32)RealtimeWave->GetSampleRateForCurrentPlatform() != TargetSR || (int32)RealtimeWave->NumChannels != TargetCH)
	{
		RealtimeWave = CreateWave(TEXT("RadioRealtimeWave_Recreate"));
	}
	return RealtimeWave;
}

bool ARadioManager::BeginRealtimeTransmission(const FRadioSubtitleInfomation& SubtitleInfo, bool bInterruptIfBusy)
{
	if (bRealtimeActive)
	{
		RealtimeSubtitle = SubtitleInfo;
		return true;
	}

	if (bIsPlaying || CurrentLine || (Queue.Num() > 0))
	{
		if (!bInterruptIfBusy)
		{
			return false;
		}
		InterruptAllPlayback_Internal(false); // 큐 유지
	}

	bRealtimeActive = true;
	bIsPlaying = true;
	OnBusyChanged.Broadcast(true);

	RealtimeSubtitle = SubtitleInfo;

	RealtimeSampleRate = RealtimeDefaultSampleRate;
	RealtimeNumChannels = RealtimeDefaultNumChannels;

	PendingPcm.Reset();
	bRealtimeVoiceStarted = false;
	RealtimeWave = nullptr;

	PlayState = ERadioPlayState::RealtimeStartTone;
	PlayRealtimeStartTone();
	return true;
}

void ARadioManager::PlayRealtimeStartTone()
{
	ClearStateTimer();

	if (StartToneSound)
	{
		SfxAudioComp->SetSound(StartToneSound);
		SfxAudioComp->Play();

		const float Duration = StartToneSound->GetDuration();
		if (Duration > 0.f)
		{
			GetWorldTimerManager().SetTimer(StateTimerHandle, this, &ARadioManager::OnRealtimeStartToneFinished, Duration, false);
			return;
		}
	}
	OnRealtimeStartToneFinished();
}

void ARadioManager::OnRealtimeStartToneFinished()
{
	PlayState = ERadioPlayState::RealtimeVoice;

	if (LoopNoiseSound)
	{
		LoopAudioComp->SetSound(LoopNoiseSound);
		LoopAudioComp->Play();
	}

	OnSubtitleBegin.Broadcast(RealtimeSubtitle);

	// ✅ 여기서 VoiceAudioComp->Play() 금지
	// 프리버퍼 찼을 때 StartRealtimeVoiceIfNeeded에서 시작
}

void ARadioManager::StartFlushTimerIfNeeded()
{
	if (!GetWorld()) return;
	if (RealtimeFlushIntervalSec <= 0.0f) return;

	if (!RealtimeFlushTimerHandle.IsValid())
	{
		FTimerDelegate D;
		D.BindWeakLambda(this, [this]()
			{
				FlushPendingPcm(false);
			});

		GetWorldTimerManager().SetTimer(
			RealtimeFlushTimerHandle,
			D,
			RealtimeFlushIntervalSec,
			true
		);
	}
}


void ARadioManager::StopFlushTimer()
{
	if (RealtimeFlushTimerHandle.IsValid() && GetWorld())
	{
		GetWorldTimerManager().ClearTimer(RealtimeFlushTimerHandle);
	}
	RealtimeFlushTimerHandle.Invalidate();
}

void ARadioManager::FlushPendingPcm(bool bForceAll)
{
	if (!bRealtimeActive)
		return;

	if (!RealtimeWave)
		return;

	if (PendingPcm.Num() <= 0)
		return;

	int32 BytesToFlush = PendingPcm.Num();

	if (!bForceAll)
	{
		// 40ms chunk
		const int32 SR = FMath::Max(8000, RealtimeSampleRate);
		const int32 CH = FMath::Clamp(RealtimeNumChannels, 1, 2);
		const int32 TargetBytes = (int32)FMath::RoundToInt(0.04f * (float)SR) * CH * 2;
		BytesToFlush = FMath::Min(BytesToFlush, TargetBytes);
	}

	RealtimeWave->QueueAudio(PendingPcm.GetData(), BytesToFlush);

	if (BytesToFlush == PendingPcm.Num())
	{
		PendingPcm.Reset();
	}
	else
	{
		PendingPcm.RemoveAt(0, BytesToFlush, false);
	}
}

void ARadioManager::StartRealtimeVoiceIfNeeded()
{
	if (bRealtimeVoiceStarted)
		return;

	const int32 NeedBytes = CalcPrebufferBytes(RealtimeSampleRate, RealtimeNumChannels);
	if (PendingPcm.Num() < NeedBytes)
		return;

	RealtimeWave = EnsureRealtimeWave(RealtimeSampleRate, RealtimeNumChannels);
	if (!RealtimeWave || !VoiceAudioComp)
		return;

	// 시작 전 한 번 크게 밀어넣고 Play
	FlushPendingPcm(true);

	VoiceAudioComp->SetSound(RealtimeWave);
	if (!VoiceAudioComp->IsPlaying())
	{
		VoiceAudioComp->Play();
	}

	bRealtimeVoiceStarted = true;
	StartFlushTimerIfNeeded();
}

void ARadioManager::AppendRealtimePcm16(const TArray<uint8>& Pcm16LE, int32 SampleRate, int32 NumChannels)
{
	if (!bRealtimeActive)
		return;

	if (Pcm16LE.Num() <= 0)
		return;

	const int32 UseSR = (SampleRate > 0) ? SampleRate : RealtimeDefaultSampleRate;
	const int32 UseCH = GetUseChannels((NumChannels > 0) ? NumChannels : RealtimeDefaultNumChannels);

	RealtimeSampleRate = UseSR;
	RealtimeNumChannels = UseCH;

	// Pending 누적
	const int32 OldNum = PendingPcm.Num();
	PendingPcm.SetNumUninitialized(OldNum + Pcm16LE.Num());
	FMemory::Memcpy(PendingPcm.GetData() + OldNum, Pcm16LE.GetData(), Pcm16LE.Num());

	// 재생 시작 전이면 프리버퍼 확인 후 시작
	if (!bRealtimeVoiceStarted)
	{
		StartRealtimeVoiceIfNeeded();
		return;
	}

	// 재생 중이면 타이머 flush가 처리(너무 쌓이면 조금 빨리 밀어넣기)
	const int32 NeedBytes = CalcPrebufferBytes(RealtimeSampleRate, RealtimeNumChannels);
	if (PendingPcm.Num() >= NeedBytes * 2)
	{
		FlushPendingPcm(false);
	}
}

void ARadioManager::StopRealtimeVoice_Internal()
{
	StopFlushTimer();

	if (VoiceAudioComp && VoiceAudioComp->IsPlaying())
	{
		VoiceAudioComp->Stop();
	}
	bRealtimeVoiceStarted = false;
}

void ARadioManager::EndRealtimeTransmission(bool bFlushAndStop)
{
	if (!bRealtimeActive)
		return;

	OnSubtitleEnd.Broadcast(RealtimeSubtitle);

	if (LoopAudioComp && LoopAudioComp->IsPlaying())
	{
		LoopAudioComp->Stop();
	}

	if (bFlushAndStop)
	{
		if (RealtimeWave && PendingPcm.Num() > 0)
		{
			FlushPendingPcm(true);
		}
		StopRealtimeVoice_Internal();
	}
	else
	{
		StopRealtimeVoice_Internal();
	}

	PlayState = ERadioPlayState::RealtimeEndTone;
	PlayRealtimeEndTone();
}

void ARadioManager::PlayRealtimeEndTone()
{
	ClearStateTimer();

	if (EndToneSound)
	{
		SfxAudioComp->SetSound(EndToneSound);
		SfxAudioComp->Play();

		const float Duration = EndToneSound->GetDuration();
		if (Duration > 0.f)
		{
			GetWorldTimerManager().SetTimer(StateTimerHandle, this, &ARadioManager::OnRealtimeEndToneFinished, Duration, false);
			return;
		}
	}

	OnRealtimeEndToneFinished();
}

void ARadioManager::OnRealtimeEndToneFinished()
{
	bRealtimeActive = false;

	StopFlushTimer();

	PendingPcm.Reset();
	RealtimeWave = nullptr;
	bRealtimeVoiceStarted = false;

	PlayState = ERadioPlayState::Idle;

	bIsPlaying = false;
	OnBusyChanged.Broadcast(false);

	TryPlayNextFromQueue();
}
