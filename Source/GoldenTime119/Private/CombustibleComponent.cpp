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

    // BP에서 FuelCurrent가 0으로 저장돼 있거나, 초기화 누락 방지
    if (Fuel.FuelInitial <= 0.f)
        Fuel.FuelInitial = 12.f;

    if (Fuel.FuelCurrent <= 0.f)
        Fuel.FuelCurrent = Fuel.FuelInitial;

    EnsureFuelInitialized();
    // 룸 자동 연결을 원하면: Owner가 RoomBounds 안에 있을 때 Room이 Rescan/Overlap로 연결해줌
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

    UpdateIgnitionProgress(DeltaTime);
    TryIgnite();

    // 연료가 0이 되면(또는 룸 산소 부족) 불이 꺼지는 건 FireActor가 먼저 Extinguish 호출하겠지만,
    // 안전장치로 여기도 관찰 가능 (원하면 여기서도 소화 트리거 가능)
}

void UCombustibleComponent::UpdateIgnitionProgress(float DeltaTime)
{
    const float PressureImpulse = PendingPressure;
    const float HeatImpulse = PendingHeat;

    PendingPressure = 0.f;
    PendingHeat = 0.f;

    // 입력(임펄스) -> 진행도
    const float InputImpulse = (PressureImpulse + HeatImpulse * 0.25f) * Ignition.Flammability;

    if (InputImpulse > KINDA_SMALL_NUMBER)
    {
        // ★중요: 임펄스는 이미 “한 번의 전달량”이라 DeltaTime을 곱하지 않는다
        Ignition.IgnitionProgress01 = FMath::Clamp(
            Ignition.IgnitionProgress01 + InputImpulse * Ignition.IgnitionSpeed,
            0.f, 1.25f
        );
    }
    else
    {
        // 감쇠만 시간 기반
        if (Ignition.IgnitionDecayPerSec > 0.f)
        {
            Ignition.IgnitionProgress01 = FMath::Max(
                0.f,
                Ignition.IgnitionProgress01 - Ignition.IgnitionDecayPerSec * DeltaTime
            );
        }
    }

    UE_LOG(LogComb, Warning, TEXT("[Comb] ProgUpdate Owner=%s Input=%.4f Prog=%.4f"),
        *GetNameSafe(GetOwner()), InputImpulse, Ignition.IgnitionProgress01);
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