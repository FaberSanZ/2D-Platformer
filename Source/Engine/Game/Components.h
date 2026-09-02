#pragma once

#include <DirectXMath.h>


struct TransformComponent
{
	DirectX::XMFLOAT3 position;
};
enum class ShapeType
{
	Rectangle,
	Circle,
	Capsule,
	CapsuleBetween,
	RoundedRectangle,
	Sprite,
	ConvexPolygon
};



struct MeshComponent
{
	ShapeType shapeType;
};