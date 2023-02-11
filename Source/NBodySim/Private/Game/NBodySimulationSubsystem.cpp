// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/NBodySimulationSubsystem.h"
#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"

#pragma region Debug CVars
/**
 * @brief Draw bounding boxes for occupied tree nodes when true
 */
static TAutoConsoleVariable<bool> CVarDrawTreeBounds(
	TEXT("NBodySim.Debug.bDrawTreeBounds"),
	false,
	TEXT("If true, draw bounding boxes for occupied tree nodes")
);

static TAutoConsoleVariable<bool> CVarDrawForceConnections(
	TEXT("NBodySim.Debug.bDrawForceConnections"),
	false,
	TEXT("If true, draw force lines between bodies")
);

/**
 * @brief Run the NBodySim simulation for one game tick.
 */
static FAutoConsoleCommandWithWorld CCmdSimulateOneTick(
	TEXT("NBodySim.Debug.SimulateOneTick"),
	TEXT("Run the NBody simulation for one game tick."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](const UWorld* World)
		{
			World->GetSubsystem<UNBodySimulationSubsystem>()->SimulateOneTick(1.f / 60.f);
		})
);

/**
 * @brief Set bShouldSimulate inside the NBodySim Subsystem.
 */
static FAutoConsoleCommandWithWorldAndArgs CCmdSetSimulateEnabled(
	TEXT("NBodySim.Debug.SetShouldSimulate"),
	TEXT("When set to false, the simulation will freeze in it's current state."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, const UWorld* World)
		{
			UNBodySimulationSubsystem* NBodySubsystem = World->GetSubsystem<UNBodySimulationSubsystem>();

			//Default to true if no arg specified.
			if (Args.Num() == 0)
				NBodySubsystem->SetShouldSimulate(true);
			else
				NBodySubsystem->SetShouldSimulate(Args[0].ToBool());
		})
);

/**
 * @brief Print the amount of bodies being simulated
 */
static FAutoConsoleCommandWithWorld CCmdGetBodiesCount(
	TEXT("NBodySim.GetBodiesCount"),
	TEXT("Prints the amount of bodies currently simulated."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](const UWorld* World)
		{
			const int Count = World->GetSubsystem<UNBodySimulationSubsystem>()->NumBodies();
			UE_LOG(LogTemp, Display, TEXT("Simulated NBodies Count: %d"), Count);
		})
);
#pragma endregion

void UNBodySimulationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UNBodySimulationSubsystem::Deinitialize()
{
	Super::Deinitialize();
	FViewport::ViewportResizedEvent.Remove(ViewportResizedEventDelegate);
	GetWorld()->GetTimerManager().ClearTimer(ProgramLoadTimerHandle);
}

void UNBodySimulationSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bShouldSimulate)
	{
		SimulateOneTick(DeltaTime);
	}
	UpdateRenderer();
}

void UNBodySimulationSubsystem::InitializeDefaults(const TSubclassOf<ANiagaraActor>& Renderer,
                                                   int NumStartingBodies, float Coefficient, float MinimumBodyMass,
                                                   float MaximumBodyMass, bool bShouldAutoLoad)
{
	this->RendererClass = Renderer;
	this->NumStartBodies = NumStartingBodies;
	this->AccuracyCoefficient = Coefficient;
	this->MinBodyMass = MinimumBodyMass;
	this->MaxBodyMass = MaximumBodyMass;
	this->bAutoLoad = bShouldAutoLoad;
}

void UNBodySimulationSubsystem::SetShouldSimulate(const bool bEnable)
{
	bShouldSimulate = bEnable;
}

void UNBodySimulationSubsystem::StartSimulation()
{
	Bodies.Reserve(NumStartBodies);
	RenderDataArr.Init(FVector(0), NumStartBodies);

	// Cache the first (and only) camera and initialize the starting screen bounds
	GameCamera = GetWorld()->GetAutoActivateCameraIterator()->Get();
	UpdateCameraWorldBounds();
	ViewportResizedEventDelegate = FViewport::ViewportResizedEvent.AddUObject(
		this, &UNBodySimulationSubsystem::OnViewportResizedCallback);

	// Initialize renderer actor
	RendererActor = GetWorld()->SpawnActor<ANiagaraActor>(RendererClass);
	NiagaraSystem = StaticCast<UNiagaraComponent*>(RendererActor->GetRootComponent());
	NiagaraSystem->SetVariableFloat(FName("MaxMass"), MaxBodyMass);

	QuadTree = MakeUnique<TTreeNode<ETreeBranchSize::QuadTree>>(WorldBounds);
	AddBodies(NumStartBodies);

	SetShouldSimulate(true);
	GetWorld()->GetTimerManager().SetTimer(ProgramLoadTimerHandle, this, &UNBodySimulationSubsystem::AdjustFrameLoad,
	                                       0.1f, true, 0.1f);
}

void UNBodySimulationSubsystem::AdjustFrameLoad()
{
	UE_LOG(LogTemp, Display, TEXT("Num simulated bodies: %d"), NumBodies());
	UE_LOG(LogTemp, Display, TEXT("Average frame time during last period: %f"), PeriodAverageFrameTime);

	const float LowerTargetWithDeviation = TargetFrameTime - TargetFrameTime * AcceptableDeviationPercentage;

	if (PeriodAverageFrameTime > LowerTargetWithDeviation)
		return;

	const float PercentageMul = TargetFrameTime / PeriodAverageFrameTime;
	const float LerpFactor = 1 - (PeriodAverageFrameTime / TargetFrameTime);

	// Update number to spawn next tick and reset last period average frame time
	// This can be improved and cleaned up a little
	NumToSpawnNextTick = FMath::Min(FMath::CeilToInt(NumBodies() * PercentageMul * LerpFactor), 2000);
	PeriodAverageFrameTime = 0;
}

void UNBodySimulationSubsystem::UpdateStats(const float DeltaTime)
{
	PeriodAverageFrameTime = (PeriodAverageFrameTime + DeltaTime * 1000) * 0.5;
}

void UNBodySimulationSubsystem::AddBodies(const int NumBodies)
{
	for (int i = 0; i < NumBodies; i++)
	{
		Bodies.Add(FBodyDescriptor(
			FVector2f(FMath::RandRange(WorldBounds.Left, WorldBounds.Right),
			          FMath::RandRange(WorldBounds.Top, WorldBounds.Bottom)),
			FMath::RandRange(MinBodyMass, MaxBodyMass)
		));
	}
}

void UNBodySimulationSubsystem::UpdateRenderer()
{
	if (!RendererActor)
		return;

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(NiagaraSystem, FName("ParticleData"),
	                                                                 RenderDataArr);
}

// @TODO: This needs cleanup
void UNBodySimulationSubsystem::SimulateOneTick(const float DeltaTime)
{
	UpdateStats(DeltaTime);

	// Update bodies count according to auto load result
	if (NumToSpawnNextTick > 0 && bAutoLoad)
	{
		UE_LOG(LogTemp, Display, TEXT("Spawning num bodies: %d"), NumToSpawnNextTick);
		AddBodies(NumToSpawnNextTick);
		NiagaraSystem->ResetSystem();

		NumToSpawnNextTick = 0;
	}

	// Ensure the bodies are actually warped before building the tree,
	// as this can lead to a crash if they're outside bounds at the time of tree building.
	for (auto& Body : Bodies)
	{
		Body.WarpWithinBounds(WorldBounds);
	}

	// Rerun the tree,
	BatchAndWaitBuildTree(DeltaTime);

	TickDebug(DeltaTime);

	BatchAndWaitBodyCalcTasks(DeltaTime);

	for (int i = 0; i < Bodies.Num(); i++)
	{
		const FBodyDescriptor& Body = Bodies[i];
		FVector RenderData;
		RenderData.X = Body.Location.X;
		RenderData.Y = Body.Location.Y;
		RenderData.Z = Body.Mass;

		if (i < RenderDataArr.Num())
			RenderDataArr[i] = RenderData;
		else
			RenderDataArr.Add(RenderData);
	}
}

// @TODO: Clean this up
void UNBodySimulationSubsystem::BatchAndWaitBodyCalcTasks(float DeltaTime)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("BeginBatchBodyCalc"), STAT_BeginBatchBodyCalc, STATGROUP_NBodySim)
	TFunction<void (int Start, int End)> Func = TFunction<void (int, int)>(
		[DeltaTime,this](int StartIndex, int EndIndex)
		{
		});

	const int NumThreads = FTaskGraphInterface::Get().GetNumBackgroundThreads();

	const int SimulationCostPerThread = (TotalSimulationCost > 0
		                                    ? TotalSimulationCost / NumThreads
		                                    : NumBodies() / NumThreads);
	// Reset total cost to be updated in the next run
	TotalSimulationCost = 0;

	// Initialize the tasks and run them
	int CurrentCostStep = 0;
	int StartIndex = 0;
	int EndIndex = 0;
	int TaskIndex = 0;

	TArray<TFuture<void>> TaskFutures;
	for (const FBodyDescriptor& Body : Bodies)
	{
		CurrentCostStep += Body.SimCost;
		++EndIndex;

		if (CurrentCostStep > SimulationCostPerThread)
		{
			TaskFutures.Add(
				AsyncPool(*GThreadPool,
				          [StartIndex, EndIndex, DeltaTime, this]()
				          {
					          for (int i = StartIndex; i < EndIndex; i++)
					          {
						          DECLARE_SCOPE_CYCLE_COUNTER(TEXT("BeginBodyCalc"), STAT_BeginBodyCalc,
						                                      STATGROUP_NBodySim)
						          FBodyDescriptor& BodyDescriptor = Bodies[i];

						          // Reset calc cost for next frame
						          BodyDescriptor.SimCost = 0;
						          CalculateBodyVelocity(DeltaTime, BodyDescriptor, *QuadTree);
						          BodyDescriptor.Location += BodyDescriptor.Velocity * DeltaTime;
						          TotalSimulationCost += BodyDescriptor.SimCost;
					          }
				          },
				          nullptr, EQueuedWorkPriority::Highest)
			);

			StartIndex = EndIndex;
			CurrentCostStep = 0;
			++TaskIndex;
		}
	}
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("RemainderBodyCalc"), STAT_RemainderBodyCalc, STATGROUP_NBodySim)
	// Run remainder on game thread.
	Func(StartIndex, Bodies.Num());

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("WaitCalcTasks"), STAT_WaitCalcTasks, STATGROUP_NBodySim)

	for (auto& Future : TaskFutures)
		Future.Wait();
}

// @TODO: Can be much further improved by assigning each thread it's
// own exclusive quad to populate and combining them at the end
void UNBodySimulationSubsystem::BatchAndWaitBuildTree(float DeltaTime)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("BeginBuildTree"), STAT_BeginBuildTree, STATGROUP_NBodySim)
	QuadTree->Reset(WorldBounds);
	Bodies.Sort([](const FBodyDescriptor& A, const FBodyDescriptor B)
	{
		return A.Location.ComponentwiseAllLessThan(B.Location);
	});

	const int NumThreads = FTaskGraphInterface::Get().GetNumBackgroundThreads();

	const int NumBodiesPerThread = NumBodies() / NumThreads;

	TArray<TFuture<void>> TaskFutures;
	for (int i = 0; i < NumThreads; i++)
	{
		TaskFutures.Add(
			AsyncPool(*GThreadPool,
			          [i, NumThreads, NumBodiesPerThread, this]()
			          {
				          DECLARE_SCOPE_CYCLE_COUNTER(TEXT("BeginBuildTreeTask"), STAT_BeginBuildTreeTask,
				                                      STATGROUP_NBodySim)

				          const int StartIndex = i * NumBodiesPerThread;
				          int EndIndex = (i + 1) * NumBodiesPerThread;
				          if (i == NumThreads - 1)
					          EndIndex = NumBodies();

				          for (int j = StartIndex; j < EndIndex; j++)
				          {
					          QuadTree->Insert(Bodies[j]);
				          }
			          },
			          nullptr, EQueuedWorkPriority::Highest)
		);
	}

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("WaitTreeTasks"), STAT_WaitTreeTasks, STATGROUP_NBodySim)
	for (auto& Future : TaskFutures)
		Future.Wait();

	// for (const FBodyDescriptor& BodyDescriptor : Bodies)
	// {
	// 	QuadTree->Insert(BodyDescriptor);
	// }
}

// @TODO: This needs cleanup
void UNBodySimulationSubsystem::CalculateBodyVelocity(const float DeltaTime, FBodyDescriptor& Body,
                                                      const TQuadTreeNode& RootNode)
{
	const bool bIsSameBody = RootNode.BodyDescriptor == Body;
	if (!bIsSameBody)
		if (RootNode.IsSingleton())
		{
			const FVector2f Dist = RootNode.BodyDescriptor.Location - Body.Location;
			const auto Force = Dist * (RootNode.BodyDescriptor.Mass / Dist.SquaredLength());

			Body.Velocity += Force;
			++Body.SimCost;
		}
		else if (RootNode.IsCluster())
		{
			// Accuracy coefficient factor
			// Node width / distance
			const float DistanceToNode = (RootNode.BodyDescriptor.Location - Body.Location).Length();
			const float AccuracyFactor = RootNode.NodeBounds.Length() / DistanceToNode;

			if (AccuracyFactor < AccuracyCoefficient)
			{
				const FVector2f Dist = RootNode.BodyDescriptor.Location - Body.Location;
				const auto Force = Dist * (RootNode.BodyDescriptor.Mass / Dist.SquaredLength());

				Body.Velocity += Force;
				++Body.SimCost;
			}
			else
			{
				// loop inner nodes
				for (const TTreeNode<ETreeBranchSize::QuadTree>& Node : RootNode)
					CalculateBodyVelocity(DeltaTime, Body, Node);
			}
		}
}

FQuadrantBounds UNBodySimulationSubsystem::GetWorldBounds() const
{
	return WorldBounds;
}


void UNBodySimulationSubsystem::OnViewportResizedCallback(FViewport* Viewport, unsigned I)
{
	UpdateCameraWorldBounds();
}

void UNBodySimulationSubsystem::UpdateCameraWorldBounds()
{
	const UGameViewportClient* GameViewport = GetWorld()->GetGameViewport();
	const FVector CameraLocation = GameCamera->GetActorLocation();

	FVector2D ViewportSize{0, 0};
	GameViewport->GetViewportSize(ViewportSize);

	// In case the sim starts before the viewport is ready initialize to default camera values
	// The bodies will warp to within screen on the next viewport update
	float AspectRatio = 0;
	if (ViewportSize.IsZero())
		AspectRatio = GameCamera->GetCameraComponent()->AspectRatio;
	else
		AspectRatio = ViewportSize.X / ViewportSize.Y;


	// Calculate world bounds relative to the camera location and screen size.
	const float HorizontalSize = GameCamera->GetCameraComponent()->OrthoWidth;
	const float VerticalSize = HorizontalSize / AspectRatio;

	WorldBounds.Left = -HorizontalSize * 0.5 + CameraLocation.X;
	WorldBounds.Right = HorizontalSize * 0.5 + CameraLocation.X;
	WorldBounds.Top = -VerticalSize * 0.5 + CameraLocation.Y;
	WorldBounds.Bottom = VerticalSize * 0.5 + CameraLocation.Y;

	UE_LOG(LogTemp, Log, TEXT("World bounds updated to: left %f, top %f, right %f, bottom %f"),
	       WorldBounds.Left, WorldBounds.Top, WorldBounds.Right, WorldBounds.Bottom);
}

#pragma region DEBUG

void UNBodySimulationSubsystem::TickDebug(float DeltaTime)
{
#if !UE_BUILD_SHIPPING
	DebugDrawTreeBounds(DeltaTime, *QuadTree);
#endif
}


void UNBodySimulationSubsystem::DebugDrawTreeBounds(float DeltaTime, TQuadTreeNode& Node)
{
#if !UE_BUILD_SHIPPING
	if (!CVarDrawTreeBounds->GetBool())
		return;

	if (Node.IsEmpty())
		return;
	const FVector2f Center = Node.NodeBounds.Midpoint();

	DrawDebugBox(GetWorld(), FVector(Center.X, Center.Y, 0),
	             FVector(Node.NodeBounds.HorizontalSize() * 0.5, Node.NodeBounds.VerticalSize() * 0.5, 0),
	             FColor::Green, false, DeltaTime, 0, 30);

	DrawDebugPoint(GetWorld(), FVector(Center.X, Center.Y, 0), 10, FColor::Red, false,
	               DeltaTime, 0);

	if (Node.IsCluster())
	{
		for (TQuadTreeNode& SubNode : Node)
			DebugDrawTreeBounds(DeltaTime, SubNode);
	}
#endif
}

void UNBodySimulationSubsystem::DebugDrawForceConnection(float DeltaTime, const FBodyDescriptor& Body1,
                                                         const FBodyDescriptor& Body2,
                                                         bool bIsPseudoBody)
{
#if !UE_BUILD_SHIPPING
	if (!CVarDrawForceConnections->GetBool())
		return;

	const FVector Loc1 = FVector(Body1.Location.X, Body1.Location.Y, 0);
	const FVector Loc2 = FVector(Body2.Location.X, Body2.Location.Y, 0);

	FColor Color = FColor::Orange;
	if (bIsPseudoBody)
		Color = FColor::Turquoise;

	DrawDebugLine(GetWorld(), Loc1, Loc2, Color, false, DeltaTime, 10, 30);
#endif
}

#pragma endregion
