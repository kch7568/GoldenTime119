#include "RadioManager.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

ARadioManager::ARadioManager()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(RootComp);

	// B2: 음성
	VoiceAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("VoiceAudioComp"));
	VoiceAudioComp->SetupAttachment(RootComponent);
	VoiceAudioComp->bAutoActivate = false;

	// B1: 루프 노이즈
	LoopAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("LoopAudioComp"));
	LoopAudioComp->SetupAttachment(RootComponent);
	LoopAudioComp->bAutoActivate = false;
	LoopAudioComp->bIsUISound = true; // 필요 시

	// A/C: 시작/종료음
	SfxAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("SfxAudioComp"));
	SfxAudioComp->SetupAttachment(RootComponent);
	SfxAudioComp->bAutoActivate = false;

	// Voice 끝났을 때 콜백 (파라미터 없는 버전)
	VoiceAudioComp->OnAudioFinished.AddDynamic(this, &ARadioManager::OnVoiceFinished);
}

void ARadioManager::BeginPlay()
{
	Super::BeginPlay();
}

ARadioManager* ARadioManager::GetRadioManager(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	AActor* Found = UGameplayStatics::GetActorOfClass(
		WorldContextObject,
		ARadioManager::StaticClass()
	);

	return Cast<ARadioManager>(Found);
}

void ARadioManager::EnqueueRadioLine(URadioLineData* LineData)
{
	if (!LineData || !LineData->VoiceSound)
	{
		return;
	}

	Queue.Add(LineData);

	if (!bIsPlaying)
	{
		TryPlayNextFromQueue();
	}
}

void ARadioManager::TryPlayNextFromQueue()
{
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
			GetWorldTimerManager().SetTimer(
				StateTimerHandle,
				this,
				&ARadioManager::OnStartToneFinished,
				Duration,
				false
			);
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
		GetWorldTimerManager().SetTimer(
			StateTimerHandle,
			this,
			&ARadioManager::OnPreDelayFinished,
			CurrentLine->PreDelay,
			false
		);
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

	// B1: 루프 노이즈 시작
	if (LoopNoiseSound)
	{
		LoopAudioComp->SetSound(LoopNoiseSound);
		LoopAudioComp->Play();
	}

	// 자막 시작
	OnSubtitleBegin.Broadcast(CurrentLine->Subtitle);

	// B2: 음성 재생
	VoiceAudioComp->SetSound(CurrentLine->VoiceSound);
	VoiceAudioComp->Play();
}

void ARadioManager::OnVoiceFinished()
{
	// 자막 종료
	if (CurrentLine)
	{
		OnSubtitleEnd.Broadcast(CurrentLine->Subtitle);
	}

	// 루프 노이즈 정지
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
		GetWorldTimerManager().SetTimer(
			StateTimerHandle,
			this,
			&ARadioManager::OnPostDelayFinished,
			CurrentLine->PostDelay,
			false
		);
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
			GetWorldTimerManager().SetTimer(
				StateTimerHandle,
				this,
				&ARadioManager::OnEndToneFinished,
				Duration,
				false
			);
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
