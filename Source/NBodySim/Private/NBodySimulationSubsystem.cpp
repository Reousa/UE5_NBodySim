// Fill out your copyright notice in the Description page of Project Settings.


#include "NBodySimulationSubsystem.h"

#include "SimulatedBody.h"

#include "Camera/CameraComponent.h"

void UNBodySimulationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

}

void UNBodySimulationSubsystem::Deinitialize()
{
	Super::Deinitialize();
	FViewport::ViewportResizedEvent.Remove(ViewportResizedEventDelegate);
}

void UNBodySimulationSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void UNBodySimulationSubsystem::StartSimulation(int NumStartingBodies, UClass* BodyClass)
{
	// Get the first (and only) auto camera and initialize the starting screen bounds
	GameCamera = GetWorld()->GetAutoActivateCameraIterator()->Get();
	UpdateCameraWorldBounds();
	ViewportResizedEventDelegate = FViewport::ViewportResizedEvent.AddUObject(this, &UNBodySimulationSubsystem::OnViewportResized);
	
	for(int i = 0; i < NumStartingBodies; i++)
	{
		const FVector Position = FVector(
			FMath::RandRange(WorldBounds.Left, WorldBounds.Right),
			FMath::RandRange(WorldBounds.Top, WorldBounds.Bottom),
			0.0f);
		GetWorld()->SpawnActor(BodyClass, &Position);
	}
}

const FWorldBounds UNBodySimulationSubsystem::GetWorldBounds() const
{
	return WorldBounds;
}

void UNBodySimulationSubsystem::OnViewportResized(FViewport* Viewport, unsigned I)
{
	UE_LOG(LogTemp, Warning, TEXT("Viewport size changed"));
	UpdateCameraWorldBounds();
}

void UNBodySimulationSubsystem::UpdateCameraWorldBounds()
{
	const UGameViewportClient* GameViewport = GetWorld()->GetGameViewport();
	const FBoxSphereBounds CameraBounds = GameCamera->GetCameraComponent()->Bounds;

	FVector2D ViewportSize {0, 0};
	GameViewport->GetViewportSize(ViewportSize);
	float AspectRatio = 0;
	
	// In case the sim starts before the viewport is ready
	if(ViewportSize.IsZero())
	{
		AspectRatio = GameCamera->GetCameraComponent()->AspectRatio;
	}else
	{
		AspectRatio = ViewportSize.X / ViewportSize.Y;
	}
	
	WorldBounds.HorizontalSize = GameCamera->GetCameraComponent()->OrthoWidth;
	WorldBounds.VerticalSize = WorldBounds.HorizontalSize / AspectRatio;
	WorldBounds.Left = -WorldBounds.HorizontalSize / 2 + CameraBounds.Origin.X;
	WorldBounds.Right = -1 * WorldBounds.Left;
	WorldBounds.Top = -WorldBounds.VerticalSize / 2 + CameraBounds.Origin.Y;
	WorldBounds.Bottom = -1 * WorldBounds.Top;
	
	UE_LOG(LogTemp, Warning, TEXT("Horizontal world bounds updated to: left %f, right: %f"),  WorldBounds.Left, WorldBounds.Right);
	UE_LOG(LogTemp, Warning, TEXT("Vertical world bounds updated to: top %f, bottom %f"), WorldBounds.Top, WorldBounds.Bottom);
}


