#pragma once

#include "BodyDescriptor.h"
#include "QuadrantBounds.h"

#include "Core/Threading/FAtomicMutex.h"
#include "Core/Threading/FAtomicScopeLock.h"

class FQuadTree;
class FQuadTreeNodeIter;

enum class ENodeType : uint8
{
	Empty,
	Cluster,
	Singleton
};

constexpr int32 QuadTreeBranchSize = 4;

/**
 * @brief Barnes Hut algorithm implementation specific QuadTree. This is an extension of FQuadTreeNode that includes
 *  more logic and per-node contiguous allocation behavior.
 */

class alignas(32) FQuadTreeNode
{
	friend class FQuadTree;

public:
	class FIterator
	{
		friend class FQuadTreeNode;

	private:
		explicit FIterator(const FQuadTreeNode* ParentNode, const int Index = 0):
			Index(Index),
			ParentNode(ParentNode)
		{
			check(ParentNode != nullptr);
			check(ParentNode->IsCluster());
		}

	public:
		FIterator() = delete;

		FORCEINLINE FQuadTreeNode& operator*() const { return ParentNode->GetLeaf(Index); }
		FORCEINLINE FQuadTreeNode* operator->() const { return &ParentNode->GetLeaf(Index); }

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
		const FQuadTreeNode* ParentNode;
	};

	FBodyDescriptor BodyDescriptor;
	FQuadrantBounds NodeBounds;
	ENodeType NodeType;

private:
	TArray<FQuadTreeNode*, TFixedAllocator<QuadTreeBranchSize>> Leaves;
	FAtomicMutex Mutex;

	explicit FQuadTreeNode(const FQuadrantBounds NodeBounds) :
		NodeBounds(NodeBounds),
		NodeType(ENodeType::Empty)
	{
		Leaves.Init(nullptr, QuadTreeBranchSize);
	}

public:
	FQuadTreeNode(FQuadTreeNode&& Other) noexcept
	{
		FAtomicScopeLock(Other.Mutex);
		BodyDescriptor = Other.BodyDescriptor;
		NodeBounds = Other.NodeBounds;
		NodeType = Other.NodeType;
		Leaves.Init(nullptr, QuadTreeBranchSize);
	}

private:
	FORCEINLINE FQuadTreeNode& GetLeaf(const EQuadrantLocation Location) const
	{
		return *Leaves[StaticCast<int>(Location)];
	}

	FORCEINLINE FQuadTreeNode& GetLeaf(const int Location) const
	{
		check(Location < Leaves.Num() && Location >= 0);
		check(Leaves[Location])
		return *Leaves[Location];
	}

	FORCEINLINE void InsertLeaf(int Location, FQuadTreeNode* Node)
	{
		check(Leaves.Num() == QuadTreeBranchSize);
		Leaves[Location] = Node;
	}

	FORCEINLINE void InsertLeaf(EQuadrantLocation Location, FQuadTreeNode* Node)
	{
		InsertLeaf(StaticCast<int>(Location), Node);
	}

public:
	FORCEINLINE bool IsCluster() const { return NodeType == ENodeType::Cluster; }
	FORCEINLINE bool IsSingleton() const { return NodeType == ENodeType::Singleton; }
	FORCEINLINE bool IsEmpty() const { return NodeType == ENodeType::Empty; }


	FORCEINLINE FIterator begin() const { return FIterator(this); }

	FORCEINLINE FIterator end() const { return FIterator(this, QuadTreeBranchSize); }
};
