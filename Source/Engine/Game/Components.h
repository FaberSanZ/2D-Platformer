#pragma once

#include <DirectXMath.h>


struct TransformComponent
{
	DirectX::XMFLOAT3 position;
	float rotation;
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