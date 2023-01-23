#pragma once
#include "TreeNode.h"

typedef TTreeNode<ETreeBranchSize::QuadTree> TQuadTreeNode; 
typedef TTreeNode<ETreeBranchSize::Octree> TOctreeNode; 

/**
 * @brief Guaranteed to always have a root node.
 * These datastructures do need some clean up at this point as well..
 * @TODO: Could probably use another pass at the templating
 */
template<int BranchSize>
class TBarnesHutTree
{
private:
	TArray<TTreeNode<BranchSize>> InternalNodesArr;

	// If node bounds length is less than this, no new leaves will be created.
	// This is used to avoid infinite recursion considering we have no collision detection.
	// Nodes will still be updated, bodies will still have their masses taken into account for the sim.
	// Currently calculated to be 0.5% of the ortho cam width
	// @TODO: Move this to a more configurable place
	const float MinNodeSize;
	
public:
	/**
	 * @brief A QuadTree implementation for the Barnes Hut algorithm.
	 * @param WorldBounds World bounds to start the tree with
	 * @param NumElements The amount of elements this tree expects to hold
	 */
	TBarnesHutTree(const FQuadrantBounds WorldBounds, const int NumElements) : MinNodeSize(WorldBounds.HorizontalSize() * 0.00005)
	{
		Reset(WorldBounds, NumElements);
	}

	FORCEINLINE operator TTreeNode<BranchSize>&() { return GetRootNode(); }
	FORCEINLINE operator TTreeNode<BranchSize>*() { return &GetRootNode(); }

	FORCEINLINE typename TTreeNode<BranchSize>::FIterator begin() { return GetRootNode().begin(); }
	FORCEINLINE typename TTreeNode<BranchSize>::FIterator end() { return GetRootNode().end(); }

	FORCEINLINE void Reset(FQuadrantBounds WorldBounds)
	{
		InternalNodesArr.Reset();
		InternalNodesArr.Insert(TTreeNode(WorldBounds), 0);
	}

	FORCEINLINE void Reset(FQuadrantBounds WorldBounds, const int NumElements)
	{
		InternalNodesArr.Reset(BranchSize * NumElements + 1);
		InternalNodesArr.Insert(TTreeNode<BranchSize>(WorldBounds), 0);
	}


	FORCEINLINE TTreeNode<BranchSize>& GetRootNode() { return InternalNodesArr[0]; }

	FORCEINLINE bool Insert(const FBodyDescriptor& Body)
	{
		return InsertInternal(GetRootNode(), Body);
	}

private:
	void UpdateNodeMass(TTreeNode<BranchSize>& Node, const FBodyDescriptor& Body);
	bool InsertInternal(TTreeNode<BranchSize>& Node, const FBodyDescriptor& Body);
	
	/**
	 * @brief Attempts to pool a new node from the existing array, or add a new node (Expanding the array) if
	 * there aren't free nodes left.
	 * @param Bounds Bounds to initialize the node with
	 * @return 
	 */
	TTreeNode<BranchSize>& MakeNewNode(const FQuadrantBounds& Bounds);

	/**
	 * @brief Transforms a node from singleton to cluster
	 * 
	 * @param Node The node to upgrade from singleton to cluster
	 * @return The body that existed inside the node pre-transform
	 */
	FBodyDescriptor MakeClusterNode(TTreeNode<BranchSize>& Node);
};

template<int BranchSize>
void TBarnesHutTree<BranchSize>::UpdateNodeMass(TTreeNode<BranchSize>& Node, const FBodyDescriptor& Body)
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
template<int BranchSize>
bool TBarnesHutTree<BranchSize>::InsertInternal(TTreeNode<BranchSize>& Node, const FBodyDescriptor& Body)
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
	default:
		return false;
	}
}
template<int BranchSize>
TTreeNode<BranchSize>& TBarnesHutTree<BranchSize>::MakeNewNode(const FQuadrantBounds& Bounds)
{
	const int Index = InternalNodesArr.Add(TTreeNode<BranchSize>(Bounds));
	return InternalNodesArr[Index];
}

// @TODO: Cleanup
template<int BranchSize>
FBodyDescriptor TBarnesHutTree<BranchSize>::MakeClusterNode(TTreeNode<BranchSize>& Node)
{
	// Create and insert a new node in the internal array for each quadrant
	// Then add them to the node as leaves
	for (int QuadIndex = 0; QuadIndex < BranchSize; QuadIndex++)
	{
		const FQuadrantBounds Bounds = Node.NodeBounds.GetQuadrantBounds(QuadIndex);
		TTreeNode<BranchSize>& NewNode = MakeNewNode(Bounds);
		Node.InsertLeaf(QuadIndex, &NewNode);
	}

	// Cleanup and return the existing body to be handled by the caller
	Node.NodeType = ENodeType::Cluster;
	const FBodyDescriptor ExistingBody = Node.BodyDescriptor;
	Node.BodyDescriptor = FBodyDescriptor();

	return ExistingBody;
}
