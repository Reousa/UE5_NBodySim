#pragma once

#include "BodyDescriptor.h"
#include "QuadrantBounds.h"

#include "Core/Threading/FAtomicReadWriteLock.h"
#include "Core/Threading/FAtomicScopeLock.h"

DECLARE_STATS_GROUP(TEXT("NBodySim - TreeNode"), STATGROUP_NBSTreeNode, STATCAT_Advanced);

DECLARE_CYCLE_STAT(TEXT("BeginInsert"), STAT_BeginInsert, STATGROUP_NBSTreeNode)
DECLARE_CYCLE_STAT(TEXT("UpdateCluster"), STAT_UpdateCluster, STATGROUP_NBSTreeNode)
DECLARE_CYCLE_STAT(TEXT("UpdateEmpty"), STAT_UpdateEmpty, STATGROUP_NBSTreeNode)
DECLARE_CYCLE_STAT(TEXT("UpdateSingleton"), STAT_UpdateSingleton, STATGROUP_NBSTreeNode)
DECLARE_CYCLE_STAT(TEXT("UpdateMass"), STAT_UpdateMass, STATGROUP_NBSTreeNode)
DECLARE_CYCLE_STAT(TEXT("WaitLock"), STAT_WaitLock, STATGROUP_NBSTreeNode)

class FQuadTreeNodeIter;

enum class ENodeType : uint8
{
	Empty,
	Cluster,
	Singleton
};

enum ETreeBranchSize
{
	QuadTree = 4,
	Octree = 6
};

/**
 * @brief Barnes Hut algorithm implementation specific QuadTree. This is an extension of FQuadTreeNode that includes
 *  more logic and per-node contiguous allocation behavior.
 */
template <int BranchSize>
class alignas(32) TTreeNode
{
	template <int>
	friend class TBarnesHutTree;

public:
	class FIterator
	{
		friend class TTreeNode;

	private:
		explicit FIterator(const TTreeNode* ParentNode, const int Index = 0):
			Index(Index),
			ParentNode(ParentNode)
		{
			checkf(ParentNode != nullptr, TEXT("FIterator::FIterator"));
			checkf(ParentNode->IsCluster(), TEXT("FIterator::FIterator"));
		}

	public:
		FIterator() = delete;

		FORCEINLINE TTreeNode& operator*() const { return ParentNode->GetLeaf(Index); }
		FORCEINLINE TTreeNode* operator->() const { return &ParentNode->GetLeaf(Index); }

		FORCEINLINE FIterator& operator++()
		{
			++Index;
			return *this;
		}

		FORCEINLINE FIterator operator++(int)
		{
			const FIterator Tmp = *this;
			++(*this);
			return Tmp;
		}

		FORCEINLINE FIterator& operator--()
		{
			--Index;
			return *this;
		}

		FORCEINLINE FIterator operator--(int)
		{
			const FIterator Tmp = *this;
			--(*this);
			return Tmp;
		}

		FORCEINLINE bool operator==(const FIterator& Other) const
		{
			return ParentNode == Other.ParentNode && Index == Other.Index;
		}

		FORCEINLINE bool operator!=(const FIterator& Other) const { return !(*this == Other); };

	private:
		int Index;
		const TTreeNode* ParentNode;
	};

	FBodyDescriptor BodyDescriptor;
	FQuadrantBounds NodeBounds;
	ENodeType NodeType;

private:
	// If node bounds length is less than this, no new leaves will be created.
	// This is used to avoid infinite recursion considering we have no collision detection.
	// Nodes will still be updated, bodies will still have their masses taken into account for the sim.
	// Currently calculated to be 0.5% of the ortho cam width
	// @TODO: Move this to a more configurable place
	constexpr static float MinNodeSize = 5;

	// @TODO: Have this use a custom thread safe contiguous allocator of some sort that handles it's own destruction.
	TArray<TTreeNode*, TFixedAllocator<BranchSize>> Leaves;

	TTreeNode<BranchSize>* ParentNode;

	FAtomicReadWriteLock Mutex;

public:
	explicit TTreeNode(const FQuadrantBounds NodeBounds, TTreeNode<BranchSize>* ParentNode = nullptr) :
		NodeBounds(NodeBounds),
		NodeType(ENodeType::Empty), ParentNode(ParentNode)
	{
		Leaves.Init(nullptr, BranchSize);
	}

	TTreeNode(TTreeNode&& Other) noexcept
	{
		FAtomicScopeLock(Other.Mutex);
		BodyDescriptor = Other.BodyDescriptor;
		NodeBounds = Other.NodeBounds;
		NodeType = Other.NodeType;
		Leaves = Other.Leaves;
		ParentNode = Other.ParentNode;
	}

	~TTreeNode()
	{
		// @TODO: Have this use a custom thread safe contiguous allocator of some sort that handles it's own destruction.
		for (const TTreeNode* Node : Leaves)
			if (Node)
				delete Node;
	}

	FORCEINLINE void Reset(const FQuadrantBounds& Bounds, TTreeNode<BranchSize>* InParentNode = nullptr)
	{
		BodyDescriptor = FBodyDescriptor();
		NodeBounds = Bounds;
		NodeType = ENodeType::Empty;
		ParentNode = InParentNode;
	}

private:
	TTreeNode& GetLeaf(const EQuadrantLocation Location) const
	{
		return *Leaves[StaticCast<int>(Location)];
	}

	TTreeNode& GetLeaf(const int Location) const
	{
		checkf(Location < Leaves.Num() && Location >= 0, TEXT("TTreeNode::GetLeaf: Index out of range."));
		checkf(Leaves[Location], TEXT("TTreeNode::GetLeaf: Target node invalid."));
		return *Leaves[Location];
	}

public:
	FORCEINLINE bool IsCluster() const { return NodeType == ENodeType::Cluster; }
	FORCEINLINE bool IsSingleton() const { return NodeType == ENodeType::Singleton; }
	FORCEINLINE bool IsEmpty() const { return NodeType == ENodeType::Empty; }

	FORCEINLINE void Insert(const FBodyDescriptor& Body);
	FORCEINLINE void MakeCluster();

	FORCEINLINE FIterator begin() const { return FIterator(this); }

	FORCEINLINE FIterator end() const { return FIterator(this, BranchSize); }

	/**
	 * @brief Traverses to the bottom the tree and updates node mass moving upwards for all the parents
	 */
	FORCEINLINE void UpdateMassBottomUp();

private:
	// bool InsertInternal(const FBodyDescriptor& Body);
	FORCEINLINE void UpdateClusterMass(const FBodyDescriptor& Body);
	FORCEINLINE void MakeSingleton(const FBodyDescriptor& Body);
};

template <int BranchSize>
void TTreeNode<BranchSize>::Insert(const FBodyDescriptor& Body)
{
	// Alternative to recursion, can probably be cleaned up better.
	// This loop basically keeps running until we find an empty leaf to place the body into.
	SCOPE_CYCLE_COUNTER(STAT_BeginInsert)
	TTreeNode* InsertionNode = this;
	while (true)
	{
		// FAtomicScopeLock ScopeLock = FAtomicScopeLock(InsertionNode->Mutex);

		switch (InsertionNode->NodeType)
		{
		// If cluster type node, attempt to insert into the quadrant we belong to
		case ENodeType::Cluster:
			{
				SCOPE_CYCLE_COUNTER(STAT_UpdateCluster)
				// Given we can have many bodies in the same spot, we'll opt not to create extra nodes below a certain size
				// We're adding their mass to this node's pseudo body descriptor so they'll still be calculated for other bodies.
				SCOPE_CYCLE_COUNTER(STAT_WaitLock)
				FAtomicScopeLock ScopeLock = FAtomicScopeLock(InsertionNode->Mutex);

				InsertionNode->UpdateClusterMass(Body);
				
				const bool bIsMinSize = InsertionNode->NodeBounds.Length() <= MinNodeSize;

				if (bIsMinSize)
				{
					return;
				}

				InsertionNode = &InsertionNode->GetLeaf(InsertionNode->NodeBounds.GetQuadrantLocation(Body.Location));

				break;
			}

		// If this node is still empty, place the body here directly & update accordingly.
		case ENodeType::Empty:
			{
				SCOPE_CYCLE_COUNTER(STAT_UpdateEmpty)
				SCOPE_CYCLE_COUNTER(STAT_WaitLock)
				FAtomicScopeLock ScopeLock = FAtomicScopeLock(InsertionNode->Mutex);
				InsertionNode->MakeSingleton(Body);

				// Tell the insert func that we're done, body has been inserted into an empty leaf.
				return;
			}

		// If the node contains a singleton body, we update this node to cluster type & insert
		// both bodies down the tree branches.
		case ENodeType::Singleton:
			{
				SCOPE_CYCLE_COUNTER(STAT_UpdateSingleton)
				SCOPE_CYCLE_COUNTER(STAT_WaitLock)
				FAtomicScopeLock ScopeLock = FAtomicScopeLock(InsertionNode->Mutex);
				InsertionNode->MakeCluster();

				// Break back to the while loop and try re-inserting
				break;
			}
		default:
			check(false);
			break;
		}
	}
}

template <int BranchSize>
void TTreeNode<BranchSize>::MakeCluster()
{
	NodeType = ENodeType::Cluster;

	// Create and insert a new node in the internal array for each quadrant
	// Then add them to the node as leaves
	for (int QuadIndex = 0; QuadIndex < BranchSize; QuadIndex++)
	{
		const FQuadrantBounds Bounds = NodeBounds.GetQuadrantBounds(QuadIndex);

		if (Leaves.IsValidIndex(QuadIndex) && Leaves[QuadIndex] != nullptr)
		{
			Leaves[QuadIndex]->Reset(Bounds, this);
		}
		else
		{
			Leaves[QuadIndex] = new TTreeNode<BranchSize>(Bounds, this);
		}
	}

	// Insert the current body in appropriate leaf

	const EQuadrantLocation CurrentBodyLocation = NodeBounds.GetQuadrantLocation(BodyDescriptor.Location);
	GetLeaf(CurrentBodyLocation).Insert(BodyDescriptor);
}


template <int BranchSize>
void TTreeNode<BranchSize>::UpdateMassBottomUp()
{
	for (TTreeNode* Node : Leaves)
	{
		SCOPE_CYCLE_COUNTER(STAT_UpdateMass)
		if (Node->IsCluster())
			Node->UpdateMassBottomUp();

		if (!Node->IsEmpty())
			if (Node->BodyDescriptor.Mass > 0)
				UpdateClusterMass(Node->BodyDescriptor);
	}
}

template <int BranchSize>
void TTreeNode<BranchSize>::UpdateClusterMass(const FBodyDescriptor& Body)
{
	// CoM = (M1 * P1 + M2 * P2) / TotalMass
	const auto M1P1 = BodyDescriptor.Mass * BodyDescriptor.Location;
	const auto M2P2 = Body.Mass * Body.Location;
	const auto TotalMass = Body.Mass + BodyDescriptor.Mass;

	BodyDescriptor.Location = (M1P1 + M2P2) / TotalMass;

	// Update total mass
	BodyDescriptor.Mass = TotalMass;
}

template <int BranchSize>
void TTreeNode<BranchSize>::MakeSingleton(const FBodyDescriptor& Body)
{
	NodeType = ENodeType::Singleton;
	BodyDescriptor.Location = Body.Location;
	BodyDescriptor.Mass = Body.Mass;
}

typedef TTreeNode<ETreeBranchSize::QuadTree> TQuadTreeNode;
typedef TTreeNode<ETreeBranchSize::Octree> TOctreeNode;
