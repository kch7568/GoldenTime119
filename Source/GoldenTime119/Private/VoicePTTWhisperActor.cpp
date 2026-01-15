#include "VoicePTTWhisperActor.h"

#include "PTTAudioRecorderComponent.h"
#include "WhisperSTTComponent.h"
#include "RadioManager.h"

#include "Components/SceneComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#include "Misc/Char.h"
#include "TimerManager.h"
#include "Engine/World.h"

AVoicePTTWhisperActor::AVoicePTTWhisperActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	// CDO 단계에서 기본 컴포넌트 생성
	PTT = CreateDefaultSubobject<UPTTAudioRecorderComponent>(TEXT("PTT"));
	Whisper = CreateDefaultSubobject<UWhisperSTTComponent>(TEXT("Whisper"));
}

void AVoicePTTWhisperActor::BeginPlay()
{
	Super::BeginPlay();

	// ✅ BP 인스턴스에서 포인터가 끊겨도 여기서 복구
	EnsureComponentsBound();

	if (!PTT || !Whisper)
	{
		UE_LOG(LogTemp, Error, TEXT("[VoicePTT] Components missing even after EnsureComponentsBound. PTT=%s Whisper=%s"),
			*GetNameSafe(PTT), *GetNameSafe(Whisper));
		return;
	}

	// 이벤트 바인딩 (중복 바인딩 방지: AddUniqueDynamic 사용)
	PTT->OnWavReady.AddUniqueDynamic(this, &AVoicePTTWhisperActor::HandleWavReady);
	Whisper->OnFinished.AddUniqueDynamic(this, &AVoicePTTWhisperActor::HandleWhisperFinished);

	RadioManager = nullptr;
	if (ARadioManager* RM = ARadioManager::GetRadioManager(this))
	{
		RadioManager = RM;
		// 네 RadioManager에 OnBusyChanged가 있다고 가정
		RadioManager->OnBusyChanged.AddUniqueDynamic(this, &AVoicePTTWhisperActor::HandleRadioBusyChanged);
	}
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
	// 1) 이미 정상
	if (PTT && Whisper)
		return;

	// 2) 인스턴스에 실제로 붙어있는 컴포넌트를 찾아서 “C++ 포인터 슬롯”을 복구
	//    (BP가 꼬였을 때도 이 방법이면 무조건 살릴 수 있음)
	if (!PTT)
	{
		if (UPTTAudioRecorderComponent* FoundPTT = FindComponentByClass<UPTTAudioRecorderComponent>())
		{
			PTT = FoundPTT;
		}
	}

	if (!Whisper)
	{
		if (UWhisperSTTComponent* FoundWhisper = FindComponentByClass<UWhisperSTTComponent>())
		{
			Whisper = FoundWhisper;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[VoicePTT] EnsureComponentsBound: this=%s PTT=%s Whisper=%s"),
		*GetNameSafe(this), *GetNameSafe(PTT), *GetNameSafe(Whisper));
}

void AVoicePTTWhisperActor::StartPTT()
{
	// ✅ 호출될 때도 마지막 안전장치
	EnsureComponentsBound();

	// 이미 켜져있으면 무시
	if (bPTTActive)
		return;

	// 라디오 채널이 이미 사용중이면 시작 차단
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
	// ✅ 호출될 때도 마지막 안전장치
	EnsureComponentsBound();

	const bool bWasActive = bPTTActive;
	bPTTActive = false;

	UE_LOG(LogTemp, Warning, TEXT("[VoicePTT] StopPTT: this=%s WasActive=%d PTT=%s class=%s"),
		*GetNameSafe(this),
		bWasActive ? 1 : 0,
		*GetNameSafe(PTT),
		PTT ? *PTT->GetClass()->GetName() : TEXT("null"));

	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(StartCaptureTimer);
	}

	// 여기서 무조건 Stop 시도
	if (PTT)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoicePTT] Calling PTT->StopPTTAndSave"));
		PTT->StopPTTAndSave(TEXT(""));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[VoicePTT] StopPTT: PTT is null, cannot StopPTTAndSave"));
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
		if (bBroadcastRawTextAlways)
		{
			OnCommandDetected.Broadcast(EGTVoiceCommand::None, WavPathOrError, 0.0f);
		}
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[VoicePTT] HandleWavReady success. WavPath=%s"), *WavPathOrError);

	EnsureComponentsBound();

	if (!Whisper)
	{
		if (bBroadcastRawTextAlways)
		{
			OnCommandDetected.Broadcast(EGTVoiceCommand::None, TEXT("Whisper component missing"), 0.0f);
		}
		return;
	}

	Whisper->RunWhisperOnWav(WavPathOrError);
}

void AVoicePTTWhisperActor::HandleWhisperFinished(bool bSuccess, const FString& TextOrError)
{
	if (!bSuccess)
	{
		if (bBroadcastRawTextAlways)
		{
			OnCommandDetected.Broadcast(EGTVoiceCommand::None, TextOrError, 0.0f);
		}
		return;
	}

	FString Raw = TextOrError;
	Raw.TrimStartAndEndInline();

	const FString N = NormalizeKo(Raw);

	EGTVoiceCommand Cmd = EGTVoiceCommand::None;
	float Confidence = 0.0f;
	ScoreCommand_TwoOnly(N, Cmd, Confidence);

	if (Cmd == EGTVoiceCommand::None || Confidence < MinConfidenceToAccept)
	{
		OnCommandDetected.Broadcast(EGTVoiceCommand::None, Raw, Confidence);
		return;
	}

	OnCommandDetected.Broadcast(Cmd, Raw, Confidence);
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

// ------------------- Normalize / Extract / Score -------------------

FString AVoicePTTWhisperActor::NormalizeKo(const FString& In)
{
	FString S = In;
	S.TrimStartAndEndInline();
	S = S.ToLower();

	for (int32 i = 0; i < S.Len(); ++i)
	{
		const TCHAR Ch = S[i];
		const uint32 Code = (uint32)Ch;
		const bool bHangul = (Code >= 0xAC00u && Code <= 0xD7A3u);
		if (FChar::IsAlnum(Ch) || Ch == TEXT(' ') || bHangul)
			continue;
		S[i] = TEXT(' ');
	}

	while (S.Contains(TEXT("  ")))
		S = S.Replace(TEXT("  "), TEXT(" "));

	// === 한글 리터럴 제거(ICE 우회) ===
	S = S.Replace(TEXT("\uD37C\uC13C\uD2B8"), TEXT("%"));              // 퍼센트
	S = S.Replace(TEXT("\uD504\uB85C"), TEXT("%"));                    // 프로
	S = S.Replace(TEXT("\uD37C"), TEXT("%"));                          // 퍼

	S = S.Replace(TEXT("\uC9C4\uC555 \uC728"), TEXT("\uC9C4\uC555\uB960")); // 진압 율 -> 진압률
	S = S.Replace(TEXT("\uC9C4\uC555\uC728"), TEXT("\uC9C4\uC555\uB960"));  // 진압율  -> 진압률

	S = S.Replace(TEXT("\uD654\uC7AC\uC0C1\uD669"), TEXT("\uD654\uC7AC \uC0C1\uD669")); // 화재상황 -> 화재 상황
	S = S.Replace(TEXT("\uC694 \uAD6C\uC870\uC790"), TEXT("\uC694\uAD6C\uC870\uC790")); // 요 구조자 -> 요구조자

	return S;
}

bool AVoicePTTWhisperActor::TryExtractPercent(const FString& S, float& OutPercent)
{
	const int32 PercentPos = S.Find(TEXT("%"));
	if (PercentPos != INDEX_NONE)
	{
		int32 Start = PercentPos - 1;
		while (Start >= 0 && FChar::IsDigit(S[Start])) --Start;
		++Start;

		if (Start < PercentPos)
		{
			const FString NumStr = S.Mid(Start, PercentPos - Start);
			OutPercent = FCString::Atof(*NumStr);
			return (OutPercent >= 0.f && OutPercent <= 100.f);
		}
	}

	const int32 KeyPos = S.Find(TEXT("\uC9C4\uC555\uB960")); // 진압률
	if (KeyPos != INDEX_NONE)
	{
		for (int32 i = KeyPos; i < S.Len(); ++i)
		{
			if (FChar::IsDigit(S[i]))
			{
				int32 j = i;
				while (j < S.Len() && FChar::IsDigit(S[j])) ++j;
				const FString NumStr = S.Mid(i, j - i);
				OutPercent = FCString::Atof(*NumStr);
				return (OutPercent >= 0.f && OutPercent <= 100.f);
			}
		}
	}

	return false;
}

bool AVoicePTTWhisperActor::TryExtractCount(const FString& S, int32& OutCount)
{
	const int32 NamePos = S.Find(TEXT("\uBA85")); // 명
	if (NamePos != INDEX_NONE)
	{
		int32 Start = NamePos - 1;
		while (Start >= 0 && FChar::IsDigit(S[Start])) --Start;
		++Start;

		if (Start < NamePos)
		{
			const FString NumStr = S.Mid(Start, NamePos - Start);
			OutCount = FCString::Atoi(*NumStr);
			return (OutCount >= 0 && OutCount <= 99);
		}
	}

	static const TCHAR* Keys[] = {
		TEXT("\uC694\uAD6C\uC870\uC790"), // 요구조자
		TEXT("\uC0AC\uB78C"),             // 사람
		TEXT("\uC778\uC6D0")              // 인원
	};

	for (const TCHAR* K : Keys)
	{
		const int32 KeyPos = S.Find(K);
		if (KeyPos == INDEX_NONE) continue;

		for (int32 i = KeyPos; i < S.Len(); ++i)
		{
			if (FChar::IsDigit(S[i]))
			{
				int32 j = i;
				while (j < S.Len() && FChar::IsDigit(S[j])) ++j;
				const FString NumStr = S.Mid(i, j - i);
				OutCount = FCString::Atoi(*NumStr);
				return (OutCount >= 0 && OutCount <= 99);
			}
		}
	}

	return false;
}

void AVoicePTTWhisperActor::ScoreCommand_TwoOnly(
	const FString& Normalized,
	EGTVoiceCommand& OutCmd,
	float& OutConfidence
)
{
	const FString& S = Normalized;

	float ScoreFire = 0.f;
	float ScoreVictim = 0.f;

	// === 화재 상황 + 진압률 ===
	const bool bFire = S.Contains(TEXT("\uD654\uC7AC")); // 화재
	const bool bSituation =
		S.Contains(TEXT("\uC0C1\uD669")) ||  // 상황
		S.Contains(TEXT("\uD604\uD669")) ||  // 현황
		S.Contains(TEXT("\uC0C1\uD0DC"));    // 상태

	const bool bSupp =
		S.Contains(TEXT("\uC9C4\uC555\uB960")) || // 진압률
		S.Contains(TEXT("%")) ||
		S.Contains(TEXT("\uC9C4\uC555"));        // 진압

	if (bFire) ScoreFire += 0.40f;
	if (bSituation) ScoreFire += 0.20f;
	if (bSupp) ScoreFire += 0.30f;

	float Pct = 0.f;
	if (TryExtractPercent(S, Pct)) ScoreFire += 0.20f;

	// === 요구조자 + 사람 ===
	const bool bVictim =
		S.Contains(TEXT("\uC694\uAD6C\uC870\uC790")) || // 요구조자
		S.Contains(TEXT("\uAD6C\uC870")) ||             // 구조
		S.Contains(TEXT("\uC778\uBA85"));               // 인명

	const bool bPeople =
		S.Contains(TEXT("\uC0AC\uB78C")) || // 사람
		S.Contains(TEXT("\uC778\uC6D0")) || // 인원
		S.Contains(TEXT("\uBA85"));         // 명

	if (bVictim) ScoreVictim += 0.45f;
	if (bPeople) ScoreVictim += 0.35f;

	int32 Cnt = 0;
	if (TryExtractCount(S, Cnt)) ScoreVictim += 0.25f;

	ScoreFire = FMath::Clamp(ScoreFire, 0.f, 1.f);
	ScoreVictim = FMath::Clamp(ScoreVictim, 0.f, 1.f);

	if (ScoreFire <= 0.f && ScoreVictim <= 0.f)
	{
		OutCmd = EGTVoiceCommand::None;
		OutConfidence = 0.f;
		return;
	}

	if (ScoreFire >= ScoreVictim)
	{
		OutCmd = (ScoreFire >= 0.50f) ? EGTVoiceCommand::FireStatusAndSuppression : EGTVoiceCommand::None;
		OutConfidence = ScoreFire;
	}
	else
	{
		OutCmd = (ScoreVictim >= 0.50f) ? EGTVoiceCommand::VictimAndPeople : EGTVoiceCommand::None;
		OutConfidence = ScoreVictim;
	}
}
