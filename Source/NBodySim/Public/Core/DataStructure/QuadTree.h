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

	// If node bounds length is less than this, no new leaves will be created.
	// This is used to avoid infinite recursion considering we have no collision detection.
	// Nodes will still be updated, bodies will still have their masses taken into account for the sim.
	// Currently calculated as 0.5% of the total horizontal space of the world bounds.
	// @TODO: Move this to a more configurable place
	const float MinNodeSize;
	
public:
	/**
	 * @brief A QuadTree implementation for the Barnes Hut algorithm.
	 * @param WorldBounds World bounds to start the tree with
	 * @param NumElements The amount of elements this tree expects to hold
	 */
	FQuadTree(const FQuadrantBounds WorldBounds, const int NumElements) : MinNodeSize(WorldBounds.HorizontalSize() * 0.005)
	{
		Reset(WorldBounds, NumElements);
	}

	FORCEINLINE operator FQuadTreeNode&() { return GetRootNode(); }
	FORCEINLINE operator FQuadTreeNode*() { return &GetRootNode(); }

	FORCEINLINE FQuadTreeNode::FIterator begin() { return GetRootNode().begin(); }
	FORCEINLINE FQuadTreeNode::FIterator end() { return GetRootNode().end(); }

	FORCEINLINE void Reset(FQuadrantBounds WorldBounds)
	{
		InternalNodesArr.Reset();
		InternalNodesArr.Insert(FQuadTreeNode(WorldBounds), 0);
	}

	FORCEINLINE void Reset(FQuadrantBounds WorldBounds, const int NumElements)
	{
		InternalNodesArr.Reset((QuadTreeBranchSize) * NumElements + 1);
		InternalNodesArr.Insert(FQuadTreeNode(WorldBounds), 0);
	}


	FORCEINLINE FQuadTreeNode& GetRootNode() { return InternalNodesArr[0]; }

	FORCEINLINE bool Insert(const FBodyDescriptor& Body)
	{
		return InsertInternal(GetRootNode(), Body);
	}

private:
	void UpdateNodeMass(FQuadTreeNode& Node, const FBodyDescriptor& Body);
	bool InsertInternal(FQuadTreeNode& Node, const FBodyDescriptor& Body);
	
	/**
	 * @brief Attempts to pool a new node from the existing array, or add a new node (Expanding the array) if
	 * there aren't free nodes left.
	 * @param Bounds Bounds to initialize the node with
	 * @return 
	 */
	FQuadTreeNode& MakeNewNode(const FQuadrantBounds& Bounds);

	/**
	 * @brief Transforms a node from singleton to cluster
	 * 
	 * @param Node The node to upgrade from singleton to cluster
	 * @return The body that existed inside the node pre-transform
	 */
	FBodyDescriptor MakeClusterNode(FQuadTreeNode& Node);
};

inline void FQuadTree::UpdateNodeMass(FQuadTreeNode& Node, const FBodyDescriptor& Body)
{
	if(Node.IsCluster())
	{
		// CoM = (M1 * P1 + M2 * P2) / TotalMass
		const auto M1P1 = Node.BodyDescriptor.Mass * Node.BodyDescriptor.Location;
		const auto M2P2 = Body.Mass * Body.Location;
		const auto TotalMass = Body.Mass + Node.BodyDescriptor.Mass;

		Node.BodyDescriptor.Location = (M1P1 + M2P2) / TotalMass;

		// Update total mass
		Node.BodyDescriptor.Mass = TotalMass;
	}
	else
	{
		Node.BodyDescriptor.Location = Body.Location;
		Node.BodyDescriptor.Mass = Body.Mass;
	}
}

inline bool FQuadTree::InsertInternal(FQuadTreeNode& Node, const FBodyDescriptor& Body)
{
	check(Node.NodeBounds.IsWithinBounds(Body.Location));

	switch (Node.NodeType)
	{
	// If cluster type node, attempt to insert into the quadrant we belong to
	case ENodeType::Cluster:
		{
			const EQuadrantLocation QuadLocation = Node.NodeBounds.GetQuadrantLocation(Body.Location);
			check(QuadLocation < EQuadrantLocation::Outside);

			UpdateNodeMass(Node, Body);
			
			// Given we can have many bodies in the same spot, we'll opt not to create extra nodes below a certain size
			// We're adding their mass to this node's pseudo body descriptor so they'll still be calculated for other bodies.
			if (Node.NodeBounds.Length() <= MinNodeSize)
				return true;
			
			return InsertInternal(Node.GetLeaf(QuadLocation), Body);
		}

	// If this node is still empty, place the body here directly & update accordingly.
	case ENodeType::Empty:
		{
			Node.NodeType = ENodeType::Singleton;
			UpdateNodeMass(Node, Body);
			return true;
		}

	// If the node contains a singleton body, we update this node to cluster type & insert
	// both bodies recursively down the tree branches.
	case ENodeType::Singleton:
		{
			const auto ExistingBody = MakeClusterNode(Node);

			const bool bHasInsertedNewBody = InsertInternal(Node, Body);
			const bool bHasInsertedExistingBody = InsertInternal(Node, ExistingBody);

			return bHasInsertedExistingBody && bHasInsertedNewBody;
		}
	}
	return false;
}

inline FQuadTreeNode& FQuadTree::MakeNewNode(const FQuadrantBounds& Bounds)
{
	const int Index = InternalNodesArr.Add(FQuadTreeNode(Bounds));
	return InternalNodesArr[Index];
}

// @TODO: Cleanup
inline FBodyDescriptor FQuadTree::MakeClusterNode(FQuadTreeNode& Node)
{
	// Create and insert a new node in the internal array for each quadrant
	// Then add them to the node as leaves
	for (int QuadIndex = 0; QuadIndex < QuadTreeBranchSize; QuadIndex++)
	{
		const FQuadrantBounds Bounds = Node.NodeBounds.GetQuadrantBounds(QuadIndex);
		FQuadTreeNode& NewNode = MakeNewNode(Bounds);
		Node.InsertLeaf(QuadIndex, &NewNode);
	}

	// Cleanup and return the existing body to be handled by the caller
	Node.NodeType = ENodeType::Cluster;
	const FBodyDescriptor ExistingBody = Node.BodyDescriptor;
	Node.BodyDescriptor = FBodyDescriptor();

	return ExistingBody;
}
