// ============================ FireAxeActor.cpp ============================
#include "FireAxeActor.h"
#include "BreakableComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogFireAxe, Log, All);

AFireAxeActor::AFireAxeActor()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    AxeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AxeMesh"));
    AxeMesh->SetupAttachment(Root);

    // 도끼날 충돌 박스
    BladeCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BladeCollision"));
    BladeCollision->SetupAttachment(AxeMesh);
    BladeCollision->SetBoxExtent(FVector(15.f, 8.f, 20.f));
    BladeCollision->SetRelativeLocation(FVector(0.f, 0.f, 50.f)); // 도끼날 위치
    BladeCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BladeCollision->SetCollisionResponseToAllChannels(ECR_Overlap);
    BladeCollision->SetGenerateOverlapEvents(true);

    // 그랩 포인트
    GripPoint = CreateDefaultSubobject<USceneComponent>(TEXT("GripPoint"));
    GripPoint->SetupAttachment(Root);
    GripPoint->SetRelativeLocation(FVector(0.f, 0.f, -30.f)); // 손잡이 끝
}

void AFireAxeActor::BeginPlay()
{
    Super::BeginPlay();

    LastPosition = GetActorLocation();

    // 충돌 이벤트 바인딩
    BladeCollision->OnComponentBeginOverlap.AddDynamic(this, &AFireAxeActor::OnBladeOverlapBegin);

    UE_LOG(LogFireAxe, Warning, TEXT("[FireAxe] Initialized - BaseDamage:%.1f MinSpeed:%.1f MaxSpeed:%.1f"),
        BaseDamage, MinSwingSpeed, MaxSwingSpeed);
}

void AFireAxeActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateVelocityTracking();
}

void AFireAxeActor::UpdateVelocityTracking()
{
    if (!bTrackControllerVelocity)
        return;

    // 위치 기반 속도 계산
    const FVector CurrentPosition = GetActorLocation();
    const FVector FrameVelocity = (CurrentPosition - LastPosition) / GetWorld()->GetDeltaSeconds();
    LastPosition = CurrentPosition;

    // 샘플 저장
    VelocitySamples.Add(FrameVelocity);
    if (VelocitySamples.Num() > VelocitySampleCount)
    {
        VelocitySamples.RemoveAt(0);
    }

    // 평균 속도 계산
    FVector AverageVelocity = FVector::ZeroVector;
    for (const FVector& Sample : VelocitySamples)
    {
        AverageVelocity += Sample;
    }
    if (VelocitySamples.Num() > 0)
    {
        AverageVelocity /= VelocitySamples.Num();
    }

    CurrentVelocity = AverageVelocity;

    // 스윙 감지
    const float Speed = CurrentVelocity.Size();
    if (Speed >= MinSwingSpeed)
    {
        OnAxeSwing.Broadcast(Speed);
    }
}

void AFireAxeActor::SetControllerVelocity(FVector Velocity)
{
    CurrentVelocity = Velocity;
    VelocitySamples.Add(Velocity);
    if (VelocitySamples.Num() > VelocitySampleCount)
    {
        VelocitySamples.RemoveAt(0);
    }
}

float AFireAxeActor::GetCurrentSwingSpeed() const
{
    return CurrentVelocity.Size();
}

float AFireAxeActor::GetSwingIntensity() const
{
    const float Speed = GetCurrentSwingSpeed();
    if (Speed < MinSwingSpeed)
        return 0.f;

    return FMath::Clamp((Speed - MinSwingSpeed) / (MaxSwingSpeed - MinSwingSpeed), 0.f, 1.f);
}

void AFireAxeActor::OnBladeOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!IsValid(OtherActor) || OtherActor == this)
        return;

    // 쿨다운 체크
    const float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastHitTime < HitCooldown)
        return;

    // 같은 액터 연속 히트 방지
    if (LastHitActor.IsValid() && LastHitActor.Get() == OtherActor)
    {
        if (CurrentTime - LastHitActorTime < HitCooldown * 2.f)
            return;
    }

    // 스윙 속도 체크
    const float SwingSpeed = GetCurrentSwingSpeed();
    if (SwingSpeed < MinSwingSpeed)
    {
        UE_LOG(LogFireAxe, Verbose, TEXT("[FireAxe] Swing too slow: %.1f < %.1f"), SwingSpeed, MinSwingSpeed);
        return;
    }

    // HitResult 생성
    FHitResult HitResult = SweepResult;
    if (!HitResult.bBlockingHit)
    {
        HitResult.Location = OtherActor->GetActorLocation();
        HitResult.ImpactPoint = BladeCollision->GetComponentLocation();
        HitResult.ImpactNormal = -CurrentVelocity.GetSafeNormal();
    }

    // 데미지 적용
    const float DamageDealt = CalculateAndApplyDamage(OtherActor, HitResult);

    if (DamageDealt > 0.f)
    {
        LastHitTime = CurrentTime;
        LastHitActor = OtherActor;
        LastHitActorTime = CurrentTime;

        // 이벤트
        OnAxeHit.Broadcast(OtherActor, HitResult.ImpactPoint, DamageDealt);

        // 햅틱
        const float HapticScale = FMath::Clamp(DamageDealt / (BaseDamage * MaxSpeedDamageMultiplier), 0.3f, 1.f);
        PlayHitHaptic(HitHapticIntensity * HapticScale, HitHapticDuration);

        UE_LOG(LogFireAxe, Warning, TEXT("[FireAxe] HIT %s! Speed:%.1f Damage:%.1f"),
            *OtherActor->GetName(), SwingSpeed, DamageDealt);
    }
}

float AFireAxeActor::CalculateAndApplyDamage(AActor* HitActor, const FHitResult& HitResult)
{
    // 스윙 강도 계산
    const float SwingIntensity = GetSwingIntensity();
    const float SpeedMultiplier = FMath::Lerp(1.f, MaxSpeedDamageMultiplier, SwingIntensity);
    const float CalculatedDamage = BaseDamage * SpeedMultiplier;

    // BreakableComponent 확인
    UBreakableComponent* Breakable = HitActor->FindComponentByClass<UBreakableComponent>();

    if (IsValid(Breakable))
    {
        // Breakable에 데미지 적용
        const float ActualDamage = Breakable->ApplyDamage(
            CalculatedDamage,
            ToolType,
            HitResult.ImpactPoint,
            HitResult.ImpactNormal
        );

        PlayHitEffects(HitResult, Breakable->Material);

        return ActualDamage;
    }
    else
    {
        // 일반 데미지 (UE 기본 데미지 시스템)
        UGameplayStatics::ApplyDamage(HitActor, CalculatedDamage, nullptr, this, nullptr);

        PlayHitEffects(HitResult, EBreakableMaterial::Wood);

        return CalculatedDamage;
    }
}

void AFireAxeActor::PlayHitEffects(const FHitResult& HitResult, EBreakableMaterial Material)
{
    USoundBase* SoundToPlay = HitSound;

    // 재질별 사운드
    switch (Material)
    {
    case EBreakableMaterial::Metal:
        if (HitMetalSound) SoundToPlay = HitMetalSound;
        break;
    case EBreakableMaterial::Wood:
        if (HitWoodSound) SoundToPlay = HitWoodSound;
        break;
    default:
        break;
    }

    if (SoundToPlay)
    {
        UGameplayStatics::PlaySoundAtLocation(this, SoundToPlay, HitResult.ImpactPoint);
    }
}

void AFireAxeActor::SimulateSwing(float Speed)
{
    // 테스트용 스윙 시뮬레이션
    CurrentVelocity = GetActorForwardVector() * Speed;
    VelocitySamples.Empty();
    for (int32 i = 0; i < VelocitySampleCount; i++)
    {
        VelocitySamples.Add(CurrentVelocity);
    }

    UE_LOG(LogFireAxe, Warning, TEXT("[FireAxe] Simulated swing at speed %.1f"), Speed);

    // 전방 라인트레이스로 히트 체크
    FHitResult HitResult;
    FVector Start = BladeCollision->GetComponentLocation();
    FVector End = Start + GetActorForwardVector() * 100.f;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_WorldDynamic, Params))
    {
        if (IsValid(HitResult.GetActor()))
        {
            const float DamageDealt = CalculateAndApplyDamage(HitResult.GetActor(), HitResult);
            OnAxeHit.Broadcast(HitResult.GetActor(), HitResult.ImpactPoint, DamageDealt);
        }
    }
}