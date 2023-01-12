// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NBodySimulationSubsystem.h"
#include "PaperSpriteComponent.h"

#include "GameFramework/Actor.h"
#include "SimulatedBody.generated.h"

UCLASS(Blueprintable)
class NBODYSIM_API ASimulatedBody : public AActor
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "NBody|Defaults")
	TObjectPtr<UPaperSprite> DefaultSprite; 
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPaperSpriteComponent> SpriteComponent;

	TObjectPtr<UNBodySimulationSubsystem> NBodySubsystem;

	virtual void BeginPlay() override;

public:	
	ASimulatedBody();

	virtual void Tick(float DeltaTime) override;

	virtual void WarpToScreenBounds();
};
