#include "FireDirectorWorldSubsystem.h"

#include "EngineUtils.h"
#include "RoomActor.h"
#include "FireActor.h"

ARoomActor* UFireDirectorWorldSubsystem::ResolveRoomById(FName RoomId) const
{
	if (RoomId == NAME_None) return nullptr;

	// 1) 캐시에서 먼저
	if (const TObjectPtr<ARoomActor>* Found = RoomCache.Find(RoomId))
	{
		return Found->Get();
	}

	// 2) 월드에서 이름 매칭
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	for (TActorIterator<ARoomActor> It(World); It; ++It)
	{
		ARoomActor* Room = *It;
		if (!IsValid(Room)) continue;

		// 정책: RoomId == RoomActor의 GetFName()
		if (Room->GetFName() == RoomId)
		{
			// 캐시는 const 함수라서 여기서 못 넣으므로, 호출부에서 넣습니다.
			return Room;
		}
	}

	return nullptr;
}

int32 UFireDirectorWorldSubsystem::IgniteRandomFiresInRoomById(FName RoomId, int32 Count, bool bAllowElectric)
{
	if (Count <= 0) return 0;

	UWorld* World = GetWorld();
	if (!World) return 0;

	ARoomActor* Room = nullptr;

	// 캐시 확인 (mutable이 아니라 여기서 갱신)
	if (const TObjectPtr<ARoomActor>* Found = RoomCache.Find(RoomId))
	{
		Room = Found->Get();
	}
	if (!IsValid(Room))
	{
		Room = ResolveRoomById(RoomId);
		if (IsValid(Room))
		{
			RoomCache.Add(RoomId, Room);
		}
	}

	if (!IsValid(Room))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FireDirector] Room not found. RoomId=%s"), *RoomId.ToString());
		return 0;
	}

	int32 Ignited = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		AFireActor* NewFire = Room->IgniteRandomCombustibleInRoom(bAllowElectric);
		if (IsValid(NewFire))
		{
			Ignited++;
		}
	}

	return Ignited;
}

bool UFireDirectorWorldSubsystem::WatchRoomAllFiresExtinguished(FName RoomId)
{
	if (RoomId == NAME_None) return false;

	UWorld* World = GetWorld();
	if (!World) return false;

	ARoomActor* Room = nullptr;

	if (const TObjectPtr<ARoomActor>* Found = RoomCache.Find(RoomId))
	{
		Room = Found->Get();
	}
	if (!IsValid(Room))
	{
		Room = ResolveRoomById(RoomId);
		if (IsValid(Room))
		{
			RoomCache.Add(RoomId, Room);
		}
	}

	if (!IsValid(Room)) return false;

	// 이미 감시 중이면 OK
	if (WatchingRooms.Contains(RoomId))
		return true;

	WatchingRooms.Add(RoomId);

	// 이벤트 바인딩: 룸의 OnFireExtinguished는 이미 Broadcast(Fire) 하고 있음
	Room->OnFireExtinguished.AddDynamic(this, &UFireDirectorWorldSubsystem::HandleRoomFireExtinguished);

	// 혹시 이미 불이 0이라면 즉시 방송
	CheckAndBroadcastIfAllExtinguished(Room);

	return true;
}

void UFireDirectorWorldSubsystem::UnwatchRoom(FName RoomId)
{
	if (!WatchingRooms.Contains(RoomId))
		return;

	ARoomActor* Room = nullptr;
	if (const TObjectPtr<ARoomActor>* Found = RoomCache.Find(RoomId))
	{
		Room = Found->Get();
	}

	if (IsValid(Room))
	{
		Room->OnFireExtinguished.RemoveDynamic(this, &UFireDirectorWorldSubsystem::HandleRoomFireExtinguished);
	}

	WatchingRooms.Remove(RoomId);
}

int32 UFireDirectorWorldSubsystem::GetRoomActiveFireCount(FName RoomId) const
{
	ARoomActor* Room = nullptr;

	if (const TObjectPtr<ARoomActor>* Found = RoomCache.Find(RoomId))
		Room = Found->Get();

	if (!IsValid(Room))
		Room = ResolveRoomById(RoomId);

	return IsValid(Room) ? Room->GetActiveFireCount() : -1;
}


void UFireDirectorWorldSubsystem::HandleRoomFireExtinguished(AFireActor* Fire)
{
	if (!IsValid(Fire)) return;

	ARoomActor* Room = Fire->LinkedRoom; // FireActor에 LinkedRoom 있음
	if (!IsValid(Room)) return;

	CheckAndBroadcastIfAllExtinguished(Room);
}

void UFireDirectorWorldSubsystem::CheckAndBroadcastIfAllExtinguished(ARoomActor* Room)
{
	if (!IsValid(Room)) return;

	const int32 Count = Room->GetActiveFireCount();
	if (Count > 0) return;

	const FName RoomId = Room->GetFName();
	if (!WatchingRooms.Contains(RoomId)) return;

	OnRoomAllFiresExtinguished.Broadcast(RoomId, Room);
}
