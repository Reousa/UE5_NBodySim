// Copyright Epic Games, Inc. All Rights Reserved.


#include "Core/Framework/NBodySimGameModeBase.h"
#include "Game/NBodySimulationSubsystem.h"


void ANBodySimGameModeBase::BeginPlay()
{
	Super::BeginPlay();
	const TObjectPtr<UNBodySimulationSubsystem> NBodySubsystem = GetWorld()->GetSubsystem<UNBodySimulationSubsystem>();
	NBodySubsystem->InitializeDefaults(DefaultRenderer, NumStaringBodies, AccuracyCoefficient, MinimumBodyMass, MaximumBodyMass, bShouldAutoLoad);
	NBodySubsystem->StartSimulation();
}

APawn* ANBodySimGameModeBase::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	// We don't want a default pawn to spawn for this project
	return nullptr;
}
