#pragma once

#include "BodyDescriptor.h"
#include "QuadrantBounds.h"

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
 * @brief Barnes Hut algorithm implementation specific QuadTree. Every node in the tree is also a tree.
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

	explicit FQuadTreeNode(const FQuadrantBounds NodeBounds) :
		NodeBounds(NodeBounds),
		NodeType(ENodeType::Empty)
	{
		Leaves.Init(nullptr, QuadTreeBranchSize);
	}

	FORCEINLINE FQuadTreeNode& GetLeaf(const EQuadrantLocation Location) const
	{
		return *Leaves[StaticCast<int>(Location)];
	}

	FORCEINLINE FQuadTreeNode& GetLeaf(const int Location) const
	{
		checkSlow(Location < Leaves.Num() && Location >= 0);
		return *Leaves[Location];
	}

	FORCEINLINE void InsertLeaf(int Location, FQuadTreeNode* Node)
	{
		if (Leaves.IsValidIndex(Location) && Location < QuadTreeBranchSize)
			Leaves[Location] = Node;
	}

	FORCEINLINE void InsertLeaf(EQuadrantLocation Location, FQuadTreeNode* Node)
	{
		InsertLeaf(StaticCast<int>(Location), Node);
	}

	void Reset(FQuadrantBounds Bounds)
	{
		// Not resetting Leaves as to not destroy objects - the addresses will be reset when new leaves replace them 
		NodeType = ENodeType::Empty;
		NodeBounds = Bounds;
		BodyDescriptor = FBodyDescriptor();
	}

public:
	FORCEINLINE bool IsCluster() const { return NodeType == ENodeType::Cluster; }
	FORCEINLINE bool IsSingleton() const { return NodeType == ENodeType::Singleton; }
	FORCEINLINE bool IsEmpty() const { return NodeType == ENodeType::Empty; }


	FORCEINLINE FIterator begin() const { return FIterator(this); }

	FORCEINLINE FIterator end() const { return FIterator(this, QuadTreeBranchSize); }
};
