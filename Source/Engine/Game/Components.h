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

struct RigidBodyComponent
{
	DirectX::XMFLOAT2 position;
	DirectX::XMFLOAT2 linearVelocity;
	DirectX::XMFLOAT2 linearAcceleration;
};