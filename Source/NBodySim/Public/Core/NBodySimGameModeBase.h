// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SimulatedBody.h"

#include "GameFramework/GameModeBase.h"
#include "NBodySimGameModeBase.generated.h"

/**
 * 
 */
UCLASS()
class NBODYSIM_API ANBodySimGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, Category = "NBody|Defaults")
	int NumStaringBodies = 10;
	
	UPROPERTY(EditAnywhere, Category = "NBody|Defaults")
	TSubclassOf<ASimulatedBody> DefaultBodyClass;
	
public:
	virtual void BeginPlay() override;
	
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;
};
