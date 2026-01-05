#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FireDirectorWorldSubsystem.generated.h"

class ARoomActor;
class AFireActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRoomAllFiresExtinguished, FName, RoomId, ARoomActor*, Room);

UCLASS()
class GOLDENTIME119_API UFireDirectorWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// RoomId(=RoomActor 이름)로 룸을 찾아 점화 N회 (랜덤 가연물)
	UFUNCTION(BlueprintCallable, Category = "Fire|Director")
	int32 IgniteRandomFiresInRoomById(FName RoomId, int32 Count, bool bAllowElectric);

	// 룸의 "모든 불 진화" 이벤트 구독 (룸 내 불이 0이 되는 순간)
	UFUNCTION(BlueprintCallable, Category = "Fire|Director")
	bool WatchRoomAllFiresExtinguished(FName RoomId);

	// 룸 감시 해제
	UFUNCTION(BlueprintCallable, Category = "Fire|Director")
	void UnwatchRoom(FName RoomId);

	// 외부(StageRunner 등)에서 구독할 수 있는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Fire|Director")
	FOnRoomAllFiresExtinguished OnRoomAllFiresExtinguished;

	// 현재 룸 불 개수 조회 (디버그/폴링용)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Fire|Director")
	int32 GetRoomActiveFireCount(FName RoomId) const;

private:
	// RoomId -> RoomActor 캐시
	UPROPERTY()
	TMap<FName, TObjectPtr<ARoomActor>> RoomCache;

	// RoomId -> 바인딩 여부
	UPROPERTY()
	TSet<FName> WatchingRooms;

	ARoomActor* ResolveRoomById(FName RoomId) const;

	UFUNCTION()
	void HandleRoomFireExtinguished(AFireActor* Fire);

	// 진화 체크(이벤트 기반 + 안전망)
	void CheckAndBroadcastIfAllExtinguished(ARoomActor* Room);
};
