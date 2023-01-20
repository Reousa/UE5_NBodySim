// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraActor.h"
#include "NiagaraSystem.h"

#include "Subsystems/WorldSubsystem.h"
#include "Camera/CameraActor.h"
#include "Core/DataStructure/QuadrantBounds.h"
#include "Core/DataStructure/QuadTree.h"
#include "Core/Framework/NBodySimGameModeBase.h"

#include "NBodySimulationSubsystem.generated.h"

DECLARE_STATS_GROUP(TEXT("Threading"), STATGROUP_NBodySim, STATCAT_Advanced);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Spawned Bodies"), NBodySim_NumSpawnedBodies, STATGROUP_NBodySim)
/**
 * 
 */
UCLASS()
class NBODYSIM_API UNBodySimulationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

protected:
	TSubclassOf<ANiagaraActor> RendererClass = nullptr;
	TObjectPtr<ANiagaraActor> RendererActor = nullptr;
	
	TObjectPtr<ACameraActor> GameCamera;
	FDelegateHandle ViewportResizedEventDelegate;

	FQuadrantBounds WorldBounds;

	TArray<FBodyDescriptor> Bodies;

	// Much better way to do this would be to expose FBodyDescriptor to a NiagaraDataInterface but I'm running out of time
	// Array containing (X, Y): Position & (Z): Mass
	TArray<FVector> RenderDataArr;	
	
	TUniquePtr<FQuadTree> QuadTree;

	/**
	 * @brief Check & adjust load when this timer is fired, gather FPS data in frames between timer ticks.
	 */
	FTimerHandle ProgramLoadTimerHandle;
	
	bool bShouldSimulate = false;
	int NumStartBodies = 0;
	float AccuracyCoefficient;
	float MinBodyMass = 0;
	float MaxBodyMass = 0;

	/**
	 * @brief The average FPS during the last load timer period
	 */
	float PeriodAverageFrameTime = 0;

	/**
	 * @brief Target frame time to run at
	 */
	float TargetFrameTime = 16;

	/**
	 * @brief Acceptable deviation from the target frame time
	 */
	float AcceptableDeviationPercentage = 0.1;

	/**
	 * @brief Number of bodies to spawn in the next tick
	 */
	int NumToSpawnNextTick = 0;
	
	/**
	 * @brief Whether the sim will attempt to reach it's target load.
	 * When false, the sim will remain with the starting amount of bodies.
	 */
	bool bAutoLoad = true;

	/**
	 * @brief The total cost of simulating bodies during the last tick (Sum of body costs).
	 * Used for load balancing threads.
	 */
	int TotalSimulationCost = 0;
	
	TObjectPtr<UNiagaraComponent> NiagaraSystem = nullptr;
	
public:
	virtual TStatId GetStatId() const override { return Super::GetStatID(); }
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;

	/**
	 * @brief Initializes default values for the subsystem
	 * @param Renderer The subclass of ANiagaraActor to use
	 * @param NumStartingBodies Amount of bodies to start with
	 * @param Coefficient Barnes Hut accuracy coefficient
	 * @param MinimumBodyMass Minimum mass of bodies
	 * @param MaximumBodyMass Maximum mass of bodies
	 */
	virtual void InitializeDefaults(const TSubclassOf<ANiagaraActor>& Renderer, int NumStartingBodies, float Coefficient,
		float MinimumBodyMass, float MaximumBodyMass, bool bShouldAutoLoad);

	/**
	 * @brief Sets the variable responsible for enabling/disabling the simulation on Tick.
	 * @param bEnable Whether or not to enable the simulation
	 */
	virtual void SetShouldSimulate(const bool bEnable);
	
	/**
	 * @brief Returns the game world bounds of the simulation.
	 * @return FQuadrantBounds describing the Bounding Box in 2D.
	 */
	FQuadrantBounds GetWorldBounds() const;
	
	virtual void StartSimulation();

	/**
	 * @brief Adjusts the program load with a target of simulating as many bodies as possible within a 60 fps average
	 */
	virtual void AdjustFrameLoad();

	/**
	 * @brief Update realtime performance stats (currently just FPS time, really)
	 */
	virtual void UpdateStats(float DeltaTime);

	virtual void AddBodies(const int NumBodies);

	FORCEINLINE virtual int NumBodies() { return Bodies.Num(); }

	virtual void UpdateRenderer();

	virtual void SimulateOneTick(float DeltaTime);

	/**
	 * @brief Distributes bodies into AsyncTasks running on the game thread pool.
	 * Ideally this would be a custom thread pool, I'm not entirely sure what the overhead difference is.
	 * No locks, no atomics, the tree is pre-calculated and every subset of bodies is owned by the context of the thread
	 * that works on it.
	 */
	virtual void BatchAndWaitBodyCalcTasks(float DeltaTime);

	virtual void BatchAndWaitBuildTree(float DeltaTime);
	
	virtual void CalculateBodyVelocity(float DeltaTime, FBodyDescriptor& Body, const FQuadTreeNode& RootNode);

protected:
	virtual void OnViewportResizedCallback(FViewport* Viewport, unsigned I);
	virtual void UpdateCameraWorldBounds();

#pragma region DEBUG
	virtual void TickDebug(float DeltaTime);

	virtual void DebugDrawTreeBounds(float DeltaTime, FQuadTreeNode& Node);

	virtual void DebugDrawForceConnection(float DeltaTime, const FBodyDescriptor& Body1, const FBodyDescriptor& Body2, bool bIsPseudoBody = false);
#pragma endregion 
};
