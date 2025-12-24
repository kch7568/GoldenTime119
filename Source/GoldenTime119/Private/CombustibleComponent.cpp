// ============================ CombustibleComponent.cpp ============================
#include "CombustibleComponent.h"

#include "RoomActor.h"
#include "FireActor.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogComb, Log, All);

UCombustibleComponent::UCombustibleComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UCombustibleComponent::BeginPlay()
{
    Super::BeginPlay();

    EnsureFuelInitialized();
    AActor* Owner = GetOwner();
    if (!Owner) return;

    if (!SmokePsc)
    {
        SmokePsc = NewObject<UParticleSystemComponent>(Owner, TEXT("SmokePSC"));
        SmokePsc->SetupAttachment(Owner->GetRootComponent());
        SmokePsc->RegisterComponent();
        SmokePsc->SetTemplate(SmokeTemplate);
        SmokePsc->bAutoActivate = true;
    }

    if (!SteamPsc)
    {
        SteamPsc = NewObject<UParticleSystemComponent>(Owner, TEXT("SteamPSC"));
        SteamPsc->SetupAttachment(Owner->GetRootComponent());
        SteamPsc->RegisterComponent();
        SteamPsc->SetTemplate(SteamTemplate);
        SteamPsc->bAutoActivate = false;   // 물 닿을 때만
        SteamPsc->DeactivateSystem();
    }
}

void UCombustibleComponent::SetOwningRoom(ARoomActor* InRoom)
{
    OwningRoom = InRoom;
}

void UCombustibleComponent::AddIgnitionPressure(const FGuid& SourceFireId, float Pressure)
{
    if (Pressure <= KINDA_SMALL_NUMBER) return;
    PendingPressure += Pressure;
    if (UWorld* W = GetWorld())
        LastInputTime = W->GetTimeSeconds();
    UE_LOG(LogComb, Warning, TEXT("[Comb] PressureIn Owner=%s P=%.4f Prog=%.4f"),
        *GetNameSafe(GetOwner()), Pressure, Ignition.IgnitionProgress01);
}

void UCombustibleComponent::AddHeat(float HeatDelta)
{
    if (HeatDelta <= KINDA_SMALL_NUMBER) return;
    PendingHeat += HeatDelta;
    if (UWorld* W = GetWorld())
        LastInputTime = W->GetTimeSeconds();
}

void UCombustibleComponent::ConsumeFuel(float ConsumeAmount)
{
    if (ConsumeAmount <= 0.f) return;
    Fuel.FuelCurrent = FMath::Max(0.f, Fuel.FuelCurrent - ConsumeAmount);
}

bool UCombustibleComponent::CanIgniteNow() const
{
    if (IsBurning()) return false;

    // 전기 타입 조건
    if (CombustibleType == ECombustibleType::Electric && !bElectricIgnitionTriggered)
        return false;

    // 연료 0이면 점화 불가
    if (Fuel.FuelCurrent <= 0.f)
        return false;

    // 룸 없으면 “환경과 무관”하게 점화시킬 수도 있으나, 여기선 룸 필수로 가정
    if (!OwningRoom.IsValid())
        return false;

    return true;
}

void UCombustibleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 1) 점화 진행도 업데이트(불 붙기 전 포함)
    UpdateIgnitionProgress(DeltaTime);

    // 2) 물에 의한 냉각/진압 적용
    if (PendingWater01 > KINDA_SMALL_NUMBER)
    {
        // 불 붙기 전: 점화 진행도 감소(냉각)
        Ignition.IgnitionProgress01 = FMath::Max<float>(
            0.f,
            Ignition.IgnitionProgress01 - PendingWater01 * WaterCoolPerSec * DeltaTime
        );

        // 불 붙은 상태: 서서히 소화(ExtinguishAlpha01 증가)
        if (IsBurning())
        {
            ExtinguishAlpha01 = FMath::Clamp(
                ExtinguishAlpha01 + PendingWater01 * WaterExtinguishPerSec * DeltaTime,
                0.f, 1.f
            );
        }

        // 물 입력 감쇠
        PendingWater01 = FMath::Max<float>(
            0.f,
            PendingWater01 - WaterDecayPerSec * DeltaTime
        );
    }
    else
    {
        // 물이 끊기면 소화 진행도는 천천히 복귀시킬지/유지할지 취향
        ExtinguishAlpha01 = FMath::Max<float>(0.f, ExtinguishAlpha01 - 0.05f * DeltaTime);
    }

    // 3) 연기 알파(불 붙기 전 훈소 포함) -> Smoke01
    const float t = FMath::GetMappedRangeValueClamped(
        FVector2D(SmokeStartProgress, SmokeFullProgress),
        FVector2D(0.f, 1.f),
        Ignition.IgnitionProgress01
    );
    SmokeAlpha01 = FMath::FInterpTo(SmokeAlpha01, t, DeltaTime, 2.0f);

    // === [NEW] Cascade 파라미터 주입 (지정된 이름만) ===
    // Smoke01: 훈소/Progress 기반
    if (IsValid(SmokePsc))
    {
        SmokePsc->SetFloatParameter(TEXT("Smoke01"), SmokeAlpha01);

        // 0이면 꺼두고 싶으면(선택)
        SmokePsc->SetFloatParameter(TEXT("Smoke01"), SmokeAlpha01);
    }

    // Steam01: 물 접촉 기반 (입력 있을 때만)
    const float Steam01 = FMath::Clamp(PendingWater01, 0.f, 1.f);
    if (IsValid(SteamPsc))
    {
        SteamPsc->SetFloatParameter(TEXT("Steam01"), Steam01);

        if (Steam01 <= 0.01f) SteamPsc->DeactivateSystem();
        else                  SteamPsc->ActivateSystem(true);
    }
    // === [/NEW] ===

    // 4) 점화 시도
    TryIgnite();

    // 5) 소화 판정: ExtinguishAlpha01이 충분하면 FireActor에 “소화 요청”
    if (IsBurning() && ExtinguishAlpha01 >= 1.f)
    {
        if (IsValid(ActiveFire))
        {
            ActiveFire->Extinguish(); // “서서히”의 끝에서 완전 소화
        }
        ExtinguishAlpha01 = 0.f;
    }
}


void UCombustibleComponent::UpdateIgnitionProgress(float DeltaTime)
{
    const float PressureImpulse = PendingPressure;
    const float HeatImpulse = PendingHeat;

    PendingPressure = 0.f;
    PendingHeat = 0.f;

    const float InputImpulse =
        (PressureImpulse + HeatImpulse * 0.25f) * Ignition.Flammability;

    if (InputImpulse > KINDA_SMALL_NUMBER)
    {
        // DeltaTime 절대 곱하지 말 것
        Ignition.IgnitionProgress01 = FMath::Clamp(
            Ignition.IgnitionProgress01
            + InputImpulse * Ignition.IgnitionSpeed,
            0.f, 1.25f
        );
    }
    else
    {
        // 감쇠만 시간 기반
        Ignition.IgnitionProgress01 = FMath::Max(
            0.f,
            Ignition.IgnitionProgress01
            - Ignition.IgnitionDecayPerSec * DeltaTime
        );
    }
}




void UCombustibleComponent::TryIgnite()
{
    if (!CanIgniteNow())
        return;

    if (Ignition.IgnitionProgress01 < Ignition.IgniteThreshold)
        return;

    // 점화!
    OnIgnited();
}

void UCombustibleComponent::OnIgnited()
{
    ARoomActor* Room = OwningRoom.Get();
    if (!IsValid(Room)) return;

    UE_LOG(LogComb, Warning, TEXT("[Comb] IGNITE Owner=%s Room=%s Type=%d"),
        *GetNameSafe(GetOwner()),
        *GetNameSafe(Room),
        (int32)CombustibleType);

    AFireActor* NewFire = Room->SpawnFireForCombustible(this, CombustibleType);
    if (!IsValid(NewFire))
    {
        // 실패하면 진행도 일부 유지 or 리셋(취향)
        Ignition.IgnitionProgress01 = 0.5f;
        return;
    }

    ActiveFire = NewFire;
    bIsBurning = true;

    // 진행도는 불 붙으면 잠깐 유지했다가(재점화 방지) 0으로 내려도 됨
    Ignition.IgnitionProgress01 = 0.f;
}

void UCombustibleComponent::OnExtinguished()
{
    bIsBurning = false;
    ActiveFire = nullptr;
}

AFireActor* UCombustibleComponent::ForceIgnite(bool bAllowElectric /*=true*/)
{
    if (!CanIgniteNow()) return nullptr;
    if (IsBurning()) return ActiveFire;

    if (CombustibleType == ECombustibleType::Electric && !bAllowElectric)
        return nullptr;

    if (CombustibleType == ECombustibleType::Electric && !bElectricIgnitionTriggered)
        return nullptr;

    ARoomActor* Room = OwningRoom.Get();
    if (!IsValid(Room))
    {
        // 룸이 안 잡혔다면 Owner에서 룸을 찾는 폴백(선택)
        return nullptr;
    }

    AActor* OwnerActor = GetOwner();
    if (!IsValid(OwnerActor))
        return nullptr;

    // Room이 최종 스폰(위치/Attach/등록 등 한 곳에서 관리)
    return Room->IgniteActor(OwnerActor);
}

void UCombustibleComponent::EnsureFuelInitialized()
{
    const float BeforeInit = Fuel.FuelInitial;
    const float BeforeCur = Fuel.FuelCurrent;

    if (Fuel.FuelInitial <= 0.f)
        Fuel.FuelInitial = 12.f;

    // 핵심: "0이면 채움"이 아니라, Current가 Init보다 작으면 올려버림
    // (BP/데이터가 Current만 0으로 들어오는 케이스를 강제로 복구)
    Fuel.FuelCurrent = FMath::Clamp(Fuel.FuelCurrent, 0.f, Fuel.FuelInitial);
    if (Fuel.FuelCurrent <= 0.f)
        Fuel.FuelCurrent = Fuel.FuelInitial;

    UE_LOG(LogComb, Warning, TEXT("[Comb] EnsureFuel Owner=%s Init %.2f->%.2f Cur %.2f->%.2f"),
        *GetNameSafe(GetOwner()), BeforeInit, Fuel.FuelInitial, BeforeCur, Fuel.FuelCurrent);
}

void UCombustibleComponent::AddWaterContact(float Amount01)
{
    PendingWater01 = FMath::Clamp(PendingWater01 + Amount01, 0.f, 1.5f);
}