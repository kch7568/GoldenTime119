#include "WhisperSTTComponent.h"

#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "TimerManager.h"

void UWhisperSTTComponent::BeginPlay()
{
	Super::BeginPlay();

	const FString ProjDir = FPaths::ProjectDir();
	ExePathAbs = ProjDir / TEXT("Plugins/WhisperRuntime/ThirdParty/Whisper/whisper-cli.exe");
	ModelPathAbs = ProjDir / TEXT("Plugins/WhisperRuntime/ThirdParty/Whisper/models/ggml-base-q5_1.bin");

	// Saved/Whisper/out/ptt_result(.txt)
	OutputBaseAbs = FPaths::ProjectSavedDir() / TEXT("Whisper/out/ptt_result");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputBaseAbs), true);
}

void UWhisperSTTComponent::RunWhisperOnWav(const FString& WavPath)
{
	if (!FPaths::FileExists(ExePathAbs))
	{
		OnFinished.Broadcast(false, FString::Printf(TEXT("Whisper exe not found: %s"), *ExePathAbs));
		return;
	}
	if (!FPaths::FileExists(ModelPathAbs))
	{
		OnFinished.Broadcast(false, FString::Printf(TEXT("Model not found: %s"), *ModelPathAbs));
		return;
	}
	if (!FPaths::FileExists(WavPath))
	{
		OnFinished.Broadcast(false, FString::Printf(TEXT("Wav not found: %s"), *WavPath));
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[Whisper] RunWhisperOnWav: %s"), *WavPath);
	UE_LOG(LogTemp, Warning, TEXT("[Whisper] Exe=%s"), *ExePathAbs);
	UE_LOG(LogTemp, Warning, TEXT("[Whisper] Model=%s"), *ModelPathAbs);
	UE_LOG(LogTemp, Warning, TEXT("[Whisper] OutBase=%s"), *OutputBaseAbs);


	const FString ResultPath = OutputBaseAbs + TEXT(".txt");
	IFileManager::Get().Delete(*ResultPath);

	const FString Args = FString::Printf(
		TEXT("-m \"%s\" -f \"%s\" -l %s --no-timestamps -otxt -of \"%s\""),
		*ModelPathAbs, *WavPath, *Language, *OutputBaseAbs
	);

	FProcHandle Proc = FPlatformProcess::CreateProc(
		*ExePathAbs, *Args,
		true,  /* detached */
		true,  /* hidden */
		true,  /* really hidden */
		nullptr, 0, nullptr, nullptr
	);

	if (!Proc.IsValid())
	{
		OnFinished.Broadcast(false, TEXT("Failed to start whisper process (CreateProc invalid)."));
		return;
	}

	StartTimeSec = FPlatformTime::Seconds();
	GetWorld()->GetTimerManager().SetTimer(PollTimer, this, &UWhisperSTTComponent::PollResultFile, PollIntervalSeconds, true);
}

void UWhisperSTTComponent::PollResultFile()
{
	const double Now = FPlatformTime::Seconds();
	if ((Now - StartTimeSec) > TimeoutSeconds)
	{
		GetWorld()->GetTimerManager().ClearTimer(PollTimer);
		OnFinished.Broadcast(false, TEXT("Whisper timeout (result file not created)."));
		return;
	}

	const FString ResultPath = OutputBaseAbs + TEXT(".txt");
	if (!FPaths::FileExists(ResultPath))
		return;

	GetWorld()->GetTimerManager().ClearTimer(PollTimer);

	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *ResultPath))
	{
		OnFinished.Broadcast(false, TEXT("Failed to read whisper result file."));
		return;
	}

	Text.TrimStartAndEndInline();
	OnFinished.Broadcast(true, Text);
}
