// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Camera/CameraActor.h"
#include "NBodySimulationSubsystem.generated.h"


// struct FWorldBounds
// {
// 	FVector2D Horizontal;
// 	FVector2D Vertical;
// };

struct FWorldBounds
{
	float HorizontalSize;
	float VerticalSize;
	float Left;
	float Right;
	float Top;
	float Bottom;
};

/**
 * 
 */
UCLASS()
class NBODYSIM_API UNBodySimulationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

protected:
	TObjectPtr<ACameraActor> GameCamera;

	FWorldBounds WorldBounds;

	FDelegateHandle ViewportResizedEventDelegate;

public:
	virtual TStatId GetStatId() const override { return Super::GetStatID(); }
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;

	virtual void StartSimulation(int NumStartingBodies, UClass* BodyClass);
	const FWorldBounds GetWorldBounds() const;

protected:
	virtual void OnViewportResized(FViewport* Viewport, unsigned I);
	virtual void UpdateCameraWorldBounds();

};
