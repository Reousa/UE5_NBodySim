#pragma once
#include "QuadTreeNode.h"

/**
 * @brief Guaranteed to always have a root node.
 * These datastructures do need some clean up at this point as well..
 */
class FQuadTree
{
private:
	TArray<FQuadTreeNode> InternalNodesArr;
	// FCriticalSection Lock;
	// FScopeLock ScopeLock(&Lock);

public:
	/**
	 * @brief A QuadTree implementation for the Barnes Hut algorithm.
	 * @param WorldBounds World bounds to start the tree with
	 * @param NumElements The amount of elements this tree expects to hold
	 */
	FQuadTree(const FQuadrantBounds WorldBounds, const int NumElements)
	{
		Reset(WorldBounds, NumElements);
	}

	FORCEINLINE operator FQuadTreeNode&() { return GetRootNode(); }
	FORCEINLINE operator FQuadTreeNode*() { return &GetRootNode(); }

	FORCEINLINE FQuadTreeNode::FIterator begin() { return GetRootNode().begin(); }
	FORCEINLINE FQuadTreeNode::FIterator end() { return GetRootNode().end(); }

	void Reset(FQuadrantBounds WorldBounds, const int NumElements)
	{
		InternalNodesArr.Reset((QuadTreeBranchSize) * NumElements + 1);
		InternalNodesArr.Insert(FQuadTreeNode(WorldBounds), 0);
	}
	

	FORCEINLINE FQuadTreeNode& GetRootNode() { return InternalNodesArr[0]; }

	FORCEINLINE bool Insert(const FBodyDescriptor& Body)
	{
		checkSlow(InternalIndex <= (QuadTreeBranchSize) * NumElements + 1)
		return InsertInternal(GetRootNode(), Body);
	}

private:

	bool InsertInternal(FQuadTreeNode& Node, const FBodyDescriptor& Body);

	/**
	 * @brief Attempts to pool a new node from the existing array, or add a new node (Expanding the array) if
	 * there aren't free nodes left.
	 * @param Bounds Bounds to initialize the node with
	 * @return 
	 */
	FQuadTreeNode& GetOrCreateNewNode(const FQuadrantBounds& Bounds);

	/**
	 * @brief Transforms a node from singleton to cluster
	 * 
	 * @param Node The node to upgrade from singleton to cluster
	 * @return The body that existed inside the node pre-transform
	 */
	FBodyDescriptor MakeClusterNode(FQuadTreeNode& Node);
};

inline bool FQuadTree::InsertInternal(FQuadTreeNode& Node, const FBodyDescriptor& Body)
{
	checkSlow(!Node.NodeBounds.IsWithinBounds(Body.Location))

	switch (Node.NodeType)
	{
	// If cluster type node, attempt to insert into the quadrant we belong to
	case ENodeType::Cluster:
		{
			checkSlow(QuadLocation < QuadTreeBranchSize)
			const EQuadrantLocation QuadLocation = Node.NodeBounds.GetQuadrantLocation(Body.Location);
			if (QuadLocation < EQuadrantLocation::Outside)
			{
				// @TODO: Clean this up
				// CoM = (M1 * P1 + M2 * P2) / TotalMass
				const auto M1P1 = Node.BodyDescriptor.Mass * Node.BodyDescriptor.Location;
				const auto M2P2 = Body.Mass * Body.Location;
				const auto TotalMass = Body.Mass + Node.BodyDescriptor.Mass;

				Node.BodyDescriptor.Location = (M1P1 + M2P2) / TotalMass;

				// Update total mass
				Node.BodyDescriptor.Mass = TotalMass;

				// We don't have collision detection yet, so just in case two bodies overlap, we ignore one of them.
				// We're adding their mass to this node's pseudo body descriptor so they'll still be calculated for other bodies.
				return InsertInternal(Node.GetLeaf(QuadLocation), Body);
			}
		}

	// If this node is still empty, place the body here directly.
	case ENodeType::Empty:
		{
			Node.NodeType = ENodeType::Singleton;
			Node.BodyDescriptor.Mass = Body.Mass;
			Node.BodyDescriptor.Location = Body.Location;
			return true;
		}
		
	// If the node contains a singleton body, we update this node to cluster type & insert
	// both bodies recursively down the tree branches.
	case ENodeType::Singleton:
		{
			const auto ExistingBody = MakeClusterNode(Node);

			const bool bHasInsertedNewBody = InsertInternal(Node, Body);
			const bool bHasInsertedExistingBody = InsertInternal(Node, ExistingBody);

			checkSlow(bHasInsertedExistingBody && bHasInsertedNewBody)
			return bHasInsertedExistingBody && bHasInsertedNewBody;
		}
	}
	return false;
}

inline FQuadTreeNode& FQuadTree::GetOrCreateNewNode(const FQuadrantBounds& Bounds)
{
	const int Index = InternalNodesArr.Add(FQuadTreeNode(Bounds));
	return InternalNodesArr[Index];
}


inline FBodyDescriptor FQuadTree::MakeClusterNode(FQuadTreeNode& Node)
{
	// Create and insert a new node in the internal array for each quadrant
	// Then add them to the node as leaves
	for (int QuadIndex = 0; QuadIndex < QuadTreeBranchSize; QuadIndex++)
	{
		const FQuadrantBounds Bounds = Node.NodeBounds.GetQuadrantBounds(QuadIndex);
		FQuadTreeNode& NewNode = GetOrCreateNewNode(Bounds);
		Node.InsertLeaf(QuadIndex, &NewNode);
	}

	// Cleanup and return the existing body to be handled by the caller
	Node.NodeType = ENodeType::Cluster;
	FBodyDescriptor ExistingBody = Node.BodyDescriptor;
	Node.BodyDescriptor = FBodyDescriptor();

	return ExistingBody;
}
