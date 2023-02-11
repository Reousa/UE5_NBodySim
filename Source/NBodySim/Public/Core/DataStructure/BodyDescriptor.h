#pragma once

#include "BodyDescriptor.generated.h"

USTRUCT(BlueprintType, DisplayName="NBody Desciptor")
struct alignas(32) FBodyDescriptor
{
	GENERATED_BODY()
public:
	FVector2f Location;
	FVector2f Velocity;
	float Mass;
	// Calculation cost, value used for threading
	float SimCost;

	FBodyDescriptor(const FVector2f Location, const float Mass): Location(Location), Velocity(0), Mass(Mass), SimCost(1)
	{
	}
	
	FBodyDescriptor(): FBodyDescriptor(FVector2f(0),0)
	{
	}

	FORCEINLINE void WarpWithinBounds(const float Left, const float Right, const float Top, const float Bottom)
	{
		if(Location.X < Left)
			Location.X += FMath::Abs(Right - Left);
		else if(Location.X > Right)
			Location.X -= FMath::Abs(Right - Left);
		if(Location.Y < Top)
			Location.Y += FMath::Abs(Bottom - Top);
		if(Location.Y > Bottom)
			Location.Y -= FMath::Abs(Bottom - Top);

	}
	FORCEINLINE void WarpWithinBounds(const FVector4f Vec)
	{
		WarpWithinBounds(Vec.X, Vec.Y, Vec.Z, Vec.W);
	} 

	FORCEINLINE bool operator==(const FBodyDescriptor& Other) const { return Other.Location == Location && Other.Mass == Mass; }
	FORCEINLINE bool operator!=(const FBodyDescriptor& Other) const { return !(*this==Other); }
};
