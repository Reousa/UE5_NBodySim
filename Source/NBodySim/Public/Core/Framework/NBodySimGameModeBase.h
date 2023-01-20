// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraActor.h"

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

	UPROPERTY(EditDefaultsOnly, Category = "NBody|Defaults")
	float AccuracyCoefficient = 1.2f;

	UPROPERTY(EditDefaultsOnly, Category = "NBody|Defaults")
	float MinimumBodyMass = 30;

	UPROPERTY(EditDefaultsOnly, Category = "NBody|Defaults")
	float MaximumBodyMass = 130;
	
	// Actor containing the niagara system used for rendering
	UPROPERTY(EditDefaultsOnly, Category = "NBody|Defaults")
	TSubclassOf<ANiagaraActor> DefaultRenderer;
	
	UPROPERTY(EditDefaultsOnly, Category = "NBody|Defaults")
	bool bShouldAutoLoad;

public:
	virtual void BeginPlay() override;
	
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;
};
