#pragma once

// Shapes2D.h | C++20 | DirectXMath
// 2D meshes in the XY plane: +X right, +Y up, Z = render depth.
// Clockwise triangles viewed from +Z. UV origin: top-left.
// Matches default D3D11 back-face culling with a +Y-up orthographic view.
// Full dimensions; shapes are centered unless a pivot or explicit points are provided.
// Each generator returns MeshData containing vertices and 32-bit triangle indices.
// Invalid parameters throw std::invalid_argument.
// Self-contained header. Vertex matches the renderer's position/color/UV layout.
// position = { x, y, layer, 1.0f }. Stride: 40 bytes.

#include <DirectXMath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace Shapes2D
{
    using DirectX::XMFLOAT2;
    using DirectX::XMFLOAT4;

    // Packed vertex data: position.xyzw, color.rgba, uv.xy.
    struct Vertex
    {
        float position[4];
        float color[4];
        float uv[2];
    };

    // POSITION: float4, offset 0. COLOR: float4, offset 16. TEXCOORD: float2, offset 32.
    static_assert(std::is_standard_layout_v<Vertex> && sizeof(Vertex) == 40);
    static_assert(offsetof(Vertex, position) == 0 && offsetof(Vertex, color) == 16 && offsetof(Vertex, uv) == 32);

    struct MeshData
    {
        std::vector<Vertex> Vertices;
        std::vector<std::uint32_t> Indices; // DXGI_FORMAT_R32_UINT; TRIANGLELIST.
    };

    struct UvRect
    {
        XMFLOAT2 Min{ 0.0f, 0.0f }; // Top-left corner.
        XMFLOAT2 Max{ 1.0f, 1.0f }; // Bottom-right corner.
    };

    enum class CapsuleAxis { Horizontal, Vertical };

    namespace Detail
    {
        inline void Require(bool valid, const char* message)
        {
            if (!valid) throw std::invalid_argument(message);
        }

        inline bool IsFinite(XMFLOAT2 point) noexcept
        {
            return std::isfinite(point.x) && std::isfinite(point.y);
        }

        inline void RequirePositive(float value)
        {
            Require(std::isfinite(value) && value > 0.0f, "Shapes2D: dimensions must be finite and positive");
        }

        inline void ValidateAppearance(float layer, XMFLOAT4 color)
        {
            Require(std::isfinite(layer), "Shapes2D: layer must be finite");
            Require(std::isfinite(color.x) && std::isfinite(color.y) && std::isfinite(color.z) && std::isfinite(color.w), "Shapes2D: color must be finite");
        }

        inline void ValidatePointCount(std::uint64_t count)
        {
            Require(count >= 3 && count <= (std::numeric_limits<std::uint32_t>::max)() / 3u, "Shapes2D: invalid point count for 32-bit indexed drawing");
        }

        inline double Cross(XMFLOAT2 a, XMFLOAT2 b, XMFLOAT2 c) noexcept
        {
            const double abX = static_cast<double>(b.x) - a.x;
            const double abY = static_cast<double>(b.y) - a.y;
            const double acX = static_cast<double>(c.x) - a.x;
            const double acY = static_cast<double>(c.y) - a.y;
            return abX * acY - abY * acX;
        }

        // The perimeter must be convex and counterclockwise. UVs use its XY bounds.
        // Triangle indices are emitted clockwise to match the renderer's default culling.
        inline MeshData BuildMesh(std::span<const XMFLOAT2> perimeter, float layer, XMFLOAT4 color)
        {
            ValidatePointCount(perimeter.size());
            ValidateAppearance(layer, color);

            XMFLOAT2 minimum = perimeter.front();
            XMFLOAT2 maximum = minimum;
            double sumX = 0.0;
            double sumY = 0.0;

            for (const XMFLOAT2 point : perimeter)
            {
                Require(IsFinite(point), "Shapes2D: generated coordinates must be finite");
                minimum.x = (std::min)(minimum.x, point.x);
                minimum.y = (std::min)(minimum.y, point.y);
                maximum.x = (std::max)(maximum.x, point.x);
                maximum.y = (std::max)(maximum.y, point.y);
                sumX += point.x;
                sumY += point.y;
            }

            const double width = static_cast<double>(maximum.x) - minimum.x;
            const double height = static_cast<double>(maximum.y) - minimum.y;
            Require(width > 0.0 && height > 0.0, "Shapes2D: mesh must have nonzero width and height");
            const double count = static_cast<double>(perimeter.size());
            const XMFLOAT2 center{ static_cast<float>(sumX / count), static_cast<float>(sumY / count) };

            MeshData mesh;
            mesh.Vertices.reserve(perimeter.size() + 1);
            mesh.Indices.reserve(perimeter.size() * 3);

            const auto addVertex = [&](XMFLOAT2 point)
                {
                    const float u = static_cast<float>((static_cast<double>(point.x) - minimum.x) / width);
                    const float v = static_cast<float>((maximum.y - static_cast<double>(point.y)) / height);
                    mesh.Vertices.push_back({ { point.x, point.y, layer, 1.0f }, { color.x, color.y, color.z, color.w }, { u, v } });
                };

            addVertex(center);
            for (const XMFLOAT2 point : perimeter) addVertex(point);

            const auto vertexCount = static_cast<std::uint32_t>(perimeter.size());
            for (std::uint32_t i = 0; i < vertexCount; ++i)
            {
                const std::uint32_t next = (i + 1) % vertexCount;
                Require(Cross(center, perimeter[i], perimeter[next]) > 0.0, "Shapes2D: degenerate triangle; check dimensions or segment count");
                mesh.Indices.insert(mesh.Indices.end(), { 0u, next + 1, i + 1 });
            }

            return mesh;
        }
    }

    // Pivot: (0, 0) bottom-left; (0.5, 0.5) center; (1, 1) top-right.
    // Custom UV regions support atlases; swapping Min/Max flips the texture.
    [[nodiscard]] inline MeshData CreateSpriteQuad(float width = 1.0f, float height = 1.0f, XMFLOAT2 pivot = { 0.5f, 0.5f }, UvRect uv = {}, float layer = 0.0f, XMFLOAT4 color = { 1, 1, 1, 1 })
    {
        Detail::RequirePositive(width);
        Detail::RequirePositive(height);
        Detail::Require(Detail::IsFinite(pivot) && Detail::IsFinite(uv.Min) && Detail::IsFinite(uv.Max), "CreateSpriteQuad: pivot and UV must be finite");
        Detail::ValidateAppearance(layer, color);

        const float left = -pivot.x * width;
        const float right = (1.0f - pivot.x) * width;
        const float bottom = -pivot.y * height;
        const float top = (1.0f - pivot.y) * height;
        Detail::Require(Detail::IsFinite({ left, bottom }) && Detail::IsFinite({ right, top }) && left < right && bottom < top, "CreateSpriteQuad: dimensions or pivot exceed float precision");

        MeshData mesh;
        mesh.Vertices = {
            { { left, bottom, layer, 1.0f }, { color.x, color.y, color.z, color.w }, { uv.Min.x, uv.Max.y } },
            { { right, bottom, layer, 1.0f }, { color.x, color.y, color.z, color.w }, { uv.Max.x, uv.Max.y } },
            { { right, top, layer, 1.0f }, { color.x, color.y, color.z, color.w }, { uv.Max.x, uv.Min.y } },
            { { left, top, layer, 1.0f }, { color.x, color.y, color.z, color.w }, { uv.Min.x, uv.Min.y } }
        };
        mesh.Indices = { 0, 2, 1, 0, 3, 2 };
        return mesh;
    }

    [[nodiscard]] inline MeshData CreateRectangle(float width, float height, float layer = 0.0f, XMFLOAT4 color = { 1, 1, 1, 1 })
    {
        return CreateSpriteQuad(width, height, { 0.5f, 0.5f }, {}, layer, color);
    }

    // segments >= 3. Radius is measured from the origin to each perimeter point.
    [[nodiscard]] inline MeshData CreateCircle(float radius, std::uint32_t segments = 32, float layer = 0.0f, XMFLOAT4 color = { 1, 1, 1, 1 })
    {
        Detail::RequirePositive(radius);
        Detail::ValidatePointCount(segments);

        std::vector<XMFLOAT2> perimeter;
        perimeter.reserve(segments);
        for (std::uint32_t i = 0; i < segments; ++i)
        {
            const float angle = DirectX::XM_2PI * (static_cast<float>(i) / segments);
            perimeter.push_back({ radius * std::cos(angle), radius * std::sin(angle) });
        }

        return Detail::BuildMesh(perimeter, layer, color);
    }

    // The two points specify the centers of the semicircular caps, not their tips.
    // arcSegments is the number of subdivisions per semicircle (>= 2).
    [[nodiscard]] inline MeshData CreateCapsuleBetween(XMFLOAT2 center1, XMFLOAT2 center2, float radius, std::uint32_t arcSegments = 8, float layer = 0.0f, XMFLOAT4 color = { 1, 1, 1, 1 })
    {
        Detail::RequirePositive(radius);
        Detail::Require(Detail::IsFinite(center1) && Detail::IsFinite(center2), "CreateCapsuleBetween: centers must be finite");
        Detail::Require(arcSegments >= 2, "CreateCapsuleBetween: arcSegments must be >= 2");
        const std::uint64_t pointCount = (static_cast<std::uint64_t>(arcSegments) + 1) * 2;
        Detail::ValidatePointCount(pointCount);

        const double dx = static_cast<double>(center2.x) - center1.x;
        const double dy = static_cast<double>(center2.y) - center1.y;
        const double length = std::hypot(dx, dy);
        Detail::Require(length > 0.0, "CreateCapsuleBetween: centers must differ; use CreateCircle for zero length");
        const XMFLOAT2 direction{ static_cast<float>(dx / length), static_cast<float>(dy / length) };
        const XMFLOAT2 left{ -direction.y, direction.x };

        std::vector<XMFLOAT2> perimeter;
        perimeter.reserve(static_cast<std::size_t>(pointCount));
        for (int cap = 0; cap < 2; ++cap)
        {
            const XMFLOAT2 center = cap == 0 ? center2 : center1;
            const float sign = cap == 0 ? 1.0f : -1.0f;
            for (std::uint32_t i = 0; i <= arcSegments; ++i)
            {
                const float angle = DirectX::XM_PI * (static_cast<float>(i) / arcSegments);
                const float cosine = i == arcSegments ? -1.0f : std::cos(angle);
                const float sine = (i == 0 || i == arcSegments) ? 0.0f : std::sin(angle);
                const float x = center.x + sign * radius * (-left.x * cosine + direction.x * sine);
                const float y = center.y + sign * radius * (-left.y * cosine + direction.y * sine);
                perimeter.push_back({ x, y });
            }
        }

        return Detail::BuildMesh(perimeter, layer, color);
    }

    // Total length = straightLength + 2 * radius. straightLength must be > 0.
    [[nodiscard]] inline MeshData CreateCapsule(float radius, float straightLength, CapsuleAxis axis = CapsuleAxis::Vertical, std::uint32_t arcSegments = 8, float layer = 0.0f, XMFLOAT4 color = { 1, 1, 1, 1 })
    {
        Detail::RequirePositive(straightLength);
        Detail::Require(axis == CapsuleAxis::Vertical || axis == CapsuleAxis::Horizontal, "CreateCapsule: invalid axis");
        const float half = straightLength * 0.5f;
        const XMFLOAT2 offset = axis == CapsuleAxis::Vertical ? XMFLOAT2{ 0, half } : XMFLOAT2{ half, 0 };
        return CreateCapsuleBetween({ -offset.x, -offset.y }, offset, radius, arcSegments, layer, color);
    }

    // width/height are the outer dimensions. 0 < cornerRadius < min(width, height) / 2.
    [[nodiscard]] inline MeshData CreateRoundedRectangle(float width, float height, float cornerRadius, std::uint32_t segmentsPerCorner = 4, float layer = 0.0f, XMFLOAT4 color = { 1, 1, 1, 1 })
    {
        Detail::RequirePositive(width);
        Detail::RequirePositive(height);
        Detail::RequirePositive(cornerRadius);
        Detail::Require(segmentsPerCorner >= 1, "CreateRoundedRectangle: segmentsPerCorner must be >= 1");
        Detail::Require(cornerRadius < (std::min)(width, height) * 0.5f, "CreateRoundedRectangle: cornerRadius must be smaller than both half dimensions");
        const std::uint64_t pointCount = (static_cast<std::uint64_t>(segmentsPerCorner) + 1) * 4;
        Detail::ValidatePointCount(pointCount);

        const float x = width * 0.5f - cornerRadius;
        const float y = height * 0.5f - cornerRadius;
        const std::array<XMFLOAT2, 4> centers{ XMFLOAT2{ x, -y }, XMFLOAT2{ x, y }, XMFLOAT2{ -x, y }, XMFLOAT2{ -x, -y } };
        const std::array<XMFLOAT2, 4> starts{ XMFLOAT2{ 0, -1 }, XMFLOAT2{ 1, 0 }, XMFLOAT2{ 0, 1 }, XMFLOAT2{ -1, 0 } };

        std::vector<XMFLOAT2> perimeter;
        perimeter.reserve(static_cast<std::size_t>(pointCount));
        for (std::size_t corner = 0; corner < centers.size(); ++corner)
        {
            const XMFLOAT2 start = starts[corner];
            for (std::uint32_t i = 0; i <= segmentsPerCorner; ++i)
            {
                const float angle = DirectX::XM_PIDIV2 * (static_cast<float>(i) / segmentsPerCorner);
                const float cosine = i == segmentsPerCorner ? 0.0f : std::cos(angle);
                const float sine = i == segmentsPerCorner ? 1.0f : std::sin(angle);
                const float px = centers[corner].x + cornerRadius * (start.x * cosine - start.y * sine);
                const float py = centers[corner].y + cornerRadius * (start.x * sine + start.y * cosine);
                perimeter.push_back({ px, py });
            }
        }

        return Detail::BuildMesh(perimeter, layer, color);
    }

    // Points must follow the perimeter in clockwise or counterclockwise order.
    // Preserves coordinates. Rejects concavity, intersections, duplicates and collinearity.
    // Does not compute a convex hull. Convexity validation takes O(n^2).
    [[nodiscard]] inline MeshData CreateConvexPolygon(std::span<const XMFLOAT2> points, float layer = 0.0f, XMFLOAT4 color = { 1, 1, 1, 1 })
    {
        Detail::ValidatePointCount(points.size());
        for (const XMFLOAT2 point : points) Detail::Require(Detail::IsFinite(point), "CreateConvexPolygon: points must be finite");

        std::vector<XMFLOAT2> perimeter(points.begin(), points.end());
        double area = 0.0;
        for (std::size_t i = 1; i + 1 < perimeter.size(); ++i) area += Detail::Cross(perimeter[0], perimeter[i], perimeter[i + 1]);
        if (area < 0.0) std::reverse(perimeter.begin(), perimeter.end());

        for (std::size_t i = 0; i < perimeter.size(); ++i)
        {
            const std::size_t next = (i + 1) % perimeter.size();
            for (std::size_t j = 0; j < perimeter.size(); ++j)
            {
                if (j == i || j == next) continue;
                Detail::Require(Detail::Cross(perimeter[i], perimeter[next], perimeter[j]) > 0.0, "CreateConvexPolygon: points must form a strictly convex simple polygon");
            }
        }

        return Detail::BuildMesh(perimeter, layer, color);
    }
}
