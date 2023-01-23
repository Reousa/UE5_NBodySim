#pragma once

#include "BodyDescriptor.h"
#include "QuadrantBounds.h"

#include "Core/Threading/FAtomicMutex.h"
#include "Core/Threading/FAtomicScopeLock.h"

class FBarnesHutTree;
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
template<int BranchSize>
class alignas(32) TTreeNode
{
	template<int>
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
			check(ParentNode != nullptr);
			check(ParentNode->IsCluster());
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
		};
		FORCEINLINE bool operator!=(const FIterator& Other) const { return !(*this == Other); };

	private:
		int Index;
		const TTreeNode* ParentNode;
	};

	FBodyDescriptor BodyDescriptor;
	FQuadrantBounds NodeBounds;
	ENodeType NodeType;

private:
	TArray<TTreeNode*, TFixedAllocator<BranchSize>> Leaves;
	FAtomicMutex Mutex;

	explicit TTreeNode(const FQuadrantBounds NodeBounds) :
		NodeBounds(NodeBounds),
		NodeType(ENodeType::Empty)
	{
		Leaves.Init(nullptr, BranchSize);
	}

public:
	TTreeNode(TTreeNode&& Other) noexcept
	{
		FAtomicScopeLock(Other.Mutex);
		BodyDescriptor = Other.BodyDescriptor;
		NodeBounds = Other.NodeBounds;
		NodeType = Other.NodeType;
		Leaves.Init(nullptr, BranchSize);
	}

private:
	FORCEINLINE TTreeNode& GetLeaf(const EQuadrantLocation Location) const
	{
		return *Leaves[StaticCast<int>(Location)];
	}

	FORCEINLINE TTreeNode& GetLeaf(const int Location) const
	{
		check(Location < Leaves.Num() && Location >= 0);
		check(Leaves[Location]);
		return *Leaves[Location];
	}

	void InsertLeaf(int Location, TTreeNode* Node)
	{
		check(Leaves.Num() == BranchSize);
		Leaves[Location] = Node;
	}

	void InsertLeaf(EQuadrantLocation Location, TTreeNode* Node)
	{
		InsertLeaf(StaticCast<int>(Location), Node);
	}

public:
	FORCEINLINE bool IsCluster() const { return NodeType == ENodeType::Cluster; }
	FORCEINLINE bool IsSingleton() const { return NodeType == ENodeType::Singleton; }
	FORCEINLINE bool IsEmpty() const { return NodeType == ENodeType::Empty; }


	FORCEINLINE FIterator begin() const { return FIterator(this); }

	FORCEINLINE FIterator end() const { return FIterator(this, BranchSize); }
};
