#pragma once
#include "QuadrantBounds.generated.h"

enum class EQuadrantLocation : uint8
{
	TopLeft = 0,
	TopRight = 1,
	BottomLeft = 2,
	BottomRight = 3,
	Outside = 4
};

USTRUCT()
struct alignas(32) FQuadrantBounds
{
	GENERATED_BODY()
public:
	union
	{
		struct
		{
			float Left;
			float Right;
			float Top;
			float Bottom;
		};

		FVector4f Vec;
	};

	FQuadrantBounds(): FQuadrantBounds(0)
	{
	}

	explicit FQuadrantBounds(int Value): Left(Value), Right(Value), Top(Value), Bottom(Value)
	{
	}

	FQuadrantBounds(const float Left, const float Right, const float Top, const float Bottom):
		Left(Left),
		Right(Right),
		Top(Top),
		Bottom(Bottom)
	{
	}

	FORCEINLINE operator FVector4f&() { return Vec; }

	FORCEINLINE float HorizontalSize() const { return FMath::Abs(Right - Left); }

	FORCEINLINE float VerticalSize() const { return FMath::Abs(Bottom - Top); }

	FORCEINLINE FVector2f DiagonalVector() const { return FVector2f(Bottom, Right) - FVector2f(Top, Left); }

	FORCEINLINE float Length() const { return DiagonalVector().Length(); }

	FORCEINLINE bool IsWithinBounds(const FVector2f Location) const
	{
		return Location.X >= Left && Location.X <= Right && Location.Y >= Top && Location.Y <= Bottom;
	}

	FORCEINLINE FVector2f Midpoint() const { return FVector2f((Left + Right) * 0.5, (Top + Bottom) * 0.5); }

	FQuadrantBounds GetQuadrantBounds(const EQuadrantLocation Location) const
	{
		const FVector2f Center = Midpoint();

		switch (Location)
		{
		case EQuadrantLocation::TopLeft:
			return FQuadrantBounds(Left, Center.X, Top, Center.Y);
		case EQuadrantLocation::TopRight:
			return FQuadrantBounds(Center.X, Right, Top, Center.Y);
		case EQuadrantLocation::BottomLeft:
			return FQuadrantBounds(Left, Center.X, Center.Y, Bottom);
		case EQuadrantLocation::BottomRight:
			return FQuadrantBounds(Center.X, Right, Center.Y, Bottom);
		default:
			return FQuadrantBounds();
		}
	}

	FORCEINLINE FQuadrantBounds GetQuadrantBounds(const int Location) const
	{
		return GetQuadrantBounds(StaticCast<EQuadrantLocation>(Location));
	}

	EQuadrantLocation GetQuadrantLocation(const FVector2f Location) const
	{
		const FVector2f Center = Midpoint();

		// Left
		if (Location.X <= Center.X)
			if (Location.Y <= Center.Y)
				return EQuadrantLocation::TopLeft;
			else
				return EQuadrantLocation::BottomLeft;
		// Right
		if (Location.X > Center.X)
			if (Location.Y < Center.Y)
				return EQuadrantLocation::TopRight;
			else
				return EQuadrantLocation::BottomRight;

		// Should be unreachable.
		check(false)
		return EQuadrantLocation::Outside;
	}
};
