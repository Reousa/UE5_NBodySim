// Copyright Epic Games, Inc. All Rights Reserved.


#include "NBodySim/Public/Core/NBodySimGameModeBase.h"

#include "NBodySimulationSubsystem.h"


void ANBodySimGameModeBase::BeginPlay()
{
	Super::BeginPlay();
	const TObjectPtr<UNBodySimulationSubsystem> NBodySubsystem = GetWorld()->GetSubsystem<UNBodySimulationSubsystem>();
	NBodySubsystem->StartSimulation(NumStaringBodies, DefaultBodyClass);
}

APawn* ANBodySimGameModeBase::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	// We don't want a default pawn to spawn for this project
	return nullptr;
}
