// Fill out your copyright notice in the Description page of Project Settings.


#include "SimulatedBody.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

void ASimulatedBody::BeginPlay()
{
	Super::BeginPlay();
	NBodySubsystem = GetWorld()->GetSubsystem<UNBodySimulationSubsystem>();
	SpriteComponent->SetSprite(DefaultSprite);
}

ASimulatedBody::ASimulatedBody()
{
	SpriteComponent = CreateDefaultSubobject<UPaperSpriteComponent>("Sprite");
	SetRootComponent(SpriteComponent);
	SpriteComponent->AddLocalRotation(FRotator(0.0f,0.0f,-90.0f));
	
	PrimaryActorTick.bCanEverTick = true;
}

void ASimulatedBody::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	SetActorLocation(GetActorLocation() + FVector(5, 5, 0));
	WarpToScreenBounds();
}

void ASimulatedBody::WarpToScreenBounds()
{
	const FWorldBounds WorldBounds = NBodySubsystem->GetWorldBounds();
	const FVector Location = GetActorLocation();
	if(Location.X < WorldBounds.Left)
	{
		SetActorLocation(Location + FVector(WorldBounds.HorizontalSize, 0, 0));
	}
	if(Location.X > WorldBounds.Right)
	{
		SetActorLocation(Location - FVector(WorldBounds.HorizontalSize, 0, 0));
	}
	if(Location.Y < WorldBounds.Top)
	{
		SetActorLocation(Location + FVector(0, WorldBounds.VerticalSize, 0));
	}
	if(Location.Y > WorldBounds.Bottom)
	{
		SetActorLocation(Location - FVector(0, WorldBounds.VerticalSize, 0));
	}
}

