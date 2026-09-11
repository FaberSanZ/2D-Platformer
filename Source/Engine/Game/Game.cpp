#include <iostream>
#include <algorithm>
#include <cmath>
#include "GameBase.h"

enum class PlayerState
{
    Idle,
    Walk,
    Run,
    Roll,
    Attack,
    Death
};

enum class BossState
{
    Idle,
    Chase,
    Attack,
    Death
};

class Game final : public GameBase
{
protected:
    Model* m_knightModel = nullptr;
    Model* m_spiderModel = nullptr;

    entt::entity m_player = entt::null;
    entt::entity m_boss = entt::null;
    entt::entity m_camera = entt::null;

    void OnInitialize(entt::registry& registry) override
    {
        m_knightModel = Assets().LoadModel("../Assets/Models/KnightCharacter.glb");
        m_spiderModel = Assets().LoadModel("../Assets/Models/Spider.glb");

        m_player = CreateKnight(registry, { -4.0f, 0.0f, 0.0f });
        m_boss = CreateSpider(registry, { 4.0f, 0.0f, 0.0f });

        m_camera = Scene().CreateEntity(registry, "Camera");

        auto& cameraTransform = registry.get<TransformComponent>(m_camera);
        cameraTransform.position = { -10.0f, 3.0f, 0.0f };

        registry.emplace<CameraComponent>(m_camera);
        registry.emplace<PrimaryCameraComponent>(m_camera);

        m_cameraYaw = DirectX::XMConvertToRadians(90.0f);
        m_cameraPitch = -0.22f;

        SetPlayerState(registry, PlayerState::Idle);
        SetBossState(registry, BossState::Idle);

        UpdateThirdPersonCamera(registry);

        Serializer().Save(registry, "../Assets/Scenes/TestScene.yaml", "TestScene");

        entt::registry testRegistry;

        if (Serializer().Load(testRegistry, Scene(), Assets(), "../Assets/Scenes/TestScene.yaml"))
            Serializer().Save(testRegistry, "../Assets/Scenes/TestScene_RoundTrip.yaml", "TestScene");
    }

    void OnUpdate(entt::registry& registry, float deltaTime) override
    {
        UpdateCameraInput();
        UpdatePlayer(registry, deltaTime);
        UpdateBoss(registry, deltaTime);
        UpdateThirdPersonCamera(registry);

        m_previousAttack = GameInput::IsMouseButtonDown(GameInput::MouseButton::Left);
        m_previousRoll = GameInput::IsKeyDown(GameInput::KeyCode::Space);
        m_previousDeath = GameInput::IsKeyDown(GameInput::KeyCode::T);
    }

    void OnDestroy(entt::registry& registry) override
    {
        GameInput::SetMouseMode(GameInput::MouseMode::Absolute);
    }

private:
    entt::entity CreateKnight(entt::registry& registry, const DirectX::XMFLOAT3& position)
    {
        entt::entity entity = Scene().CreateEntity(registry, "Knight");

        auto& transform = registry.get<TransformComponent>(entity);
        transform.position = position;
        transform.scale = { 1.0f, 1.0f, 1.0f };

        DirectX::XMStoreFloat4(&transform.rotation, DirectX::XMQuaternionRotationRollPitchYaw(0.0f, DirectX::XMConvertToRadians(90.0f), 0.0f));

        auto& model = registry.emplace<ModelComponent>(entity);
        model.model = m_knightModel;
        model.assetPath = "../Assets/Models/KnightCharacter.glb";
        model.color = { 0.25f, 0.55f, 1.0f, 1.0f };

        return entity;
    }

    entt::entity CreateSpider(entt::registry& registry, const DirectX::XMFLOAT3& position)
    {
        entt::entity entity = Scene().CreateEntity(registry, "Spider");

        auto& transform = registry.get<TransformComponent>(entity);
        transform.position = position;
        transform.scale = { 1.0f, 1.0f, 1.0f };

        DirectX::XMStoreFloat4(&transform.rotation, DirectX::XMQuaternionRotationRollPitchYaw(0.0f, DirectX::XMConvertToRadians(-90.0f), 0.0f));

        auto& model = registry.emplace<ModelComponent>(entity);
        model.model = m_spiderModel;
        model.assetPath = "../Assets/Models/Spider.glb";
        model.color = { 1.0f, 0.2f, 0.15f, 1.0f };

        return entity;
    }

    void SetPlayerState(entt::registry& registry, PlayerState state)
    {
        if (m_playerState == state)
            return;

        m_playerState = state;

        switch (state)
        {
        case PlayerState::Idle: Animations().Play(registry, m_player, "Idle", true, 0.25f); break;
        case PlayerState::Walk: Animations().Play(registry, m_player, "Walking", true, 0.25f); break;
        case PlayerState::Run: Animations().Play(registry, m_player, "Run", true, 0.20f); break;
        case PlayerState::Roll: Animations().Play(registry, m_player, "Roll", false, 0.08f); break;
        case PlayerState::Attack: Animations().Play(registry, m_player, "swordAttackJump", false, 0.10f); break;
        case PlayerState::Death: Animations().Play(registry, m_player, "Death", false, 0.20f); break;
        }
    }

    void SetBossState(entt::registry& registry, BossState state)
    {
        if (m_bossState == state)
            return;

        m_bossState = state;

        switch (state)
        {
        case BossState::Idle: Animations().Play(registry, m_boss, "SpiderArmature|Spider_Idle", true, 0.18f); break;
        case BossState::Chase: Animations().Play(registry, m_boss, "SpiderArmature|Spider_Walk", true, 0.12f); break;
        case BossState::Attack: Animations().Play(registry, m_boss, "SpiderArmature|Spider_Attack", false, 0.08f); break;
        case BossState::Death: Animations().Play(registry, m_boss, "SpiderArmature|Spider_Death", false, 0.20f); break;
        }
    }

    void UpdatePlayer(entt::registry& registry, float deltaTime)
    {
        if (m_player == entt::null)
            return;

        TransformComponent* transform = registry.try_get<TransformComponent>(m_player);

        if (!transform)
            return;

        bool attackDown = GameInput::IsMouseButtonDown(GameInput::MouseButton::Left);
        bool rollDown = GameInput::IsKeyDown(GameInput::KeyCode::Space);
        bool deathDown = GameInput::IsKeyDown(GameInput::KeyCode::T);

        bool attackPressed = attackDown && !m_previousAttack;
        bool rollPressed = rollDown && !m_previousRoll;
        bool deathPressed = deathDown && !m_previousDeath;

        if (deathPressed)
        {
            SetPlayerState(registry, PlayerState::Death);
            return;
        }

        if (m_playerState == PlayerState::Death)
            return;

        if (m_playerState == PlayerState::Roll)
        {
            if (!Animations().Finished(registry, m_player))
            {
                transform->position.x += m_rollDirection.x * m_rollSpeed * deltaTime;
                transform->position.z += m_rollDirection.z * m_rollSpeed * deltaTime;
                return;
            }

            SetPlayerState(registry, PlayerState::Idle);
        }

        if (m_playerState == PlayerState::Attack)
        {
            if (!Animations().Finished(registry, m_player))
                return;

            SetPlayerState(registry, PlayerState::Idle);
        }

        DirectX::XMFLOAT3 movementDirection = GetMovementDirection();

        bool moving = movementDirection.x != 0.0f || movementDirection.z != 0.0f;

        if (attackPressed)
        {
            SetPlayerState(registry, PlayerState::Attack);
            return;
        }

        if (rollPressed)
        {
            if (moving)
            {
                m_rollDirection = movementDirection;
            }
            else
            {
                DirectX::XMVECTOR rotation = DirectX::XMLoadFloat4(&transform->rotation);
                DirectX::XMVECTOR forward = DirectX::XMVector3Rotate(DirectX::XMVectorSet(0, 0, 1, 0), rotation);

                DirectX::XMStoreFloat3(&m_rollDirection, forward);

                m_rollDirection.y = 0.0f;

                NormalizeDirection(m_rollDirection);
            }

            FaceDirection(*transform, m_rollDirection, deltaTime, 14.0f);

            SetPlayerState(registry, PlayerState::Roll);

            return;
        }

        if (!moving)
        {
            SetPlayerState(registry, PlayerState::Idle);
            return;
        }

        bool running = GameInput::IsKeyDown(GameInput::KeyCode::Shift);

        float speed = running ? m_runSpeed : m_walkSpeed;

        transform->position.x += movementDirection.x * speed * deltaTime;
        transform->position.z += movementDirection.z * speed * deltaTime;

        FaceDirection(*transform, movementDirection, deltaTime, m_playerRotationSpeed);

        SetPlayerState(registry, running ? PlayerState::Run : PlayerState::Walk);
    }

    void UpdateBoss(entt::registry& registry, float deltaTime)
    {
        if (m_boss == entt::null || m_player == entt::null)
            return;

        TransformComponent* bossTransform = registry.try_get<TransformComponent>(m_boss);
        TransformComponent* playerTransform = registry.try_get<TransformComponent>(m_player);

        if (!bossTransform || !playerTransform || m_bossState == BossState::Death)
            return;

        if (m_bossAttackCooldown > 0.0f)
            m_bossAttackCooldown -= deltaTime;

        if (m_bossState == BossState::Attack)
        {
            if (!Animations().Finished(registry, m_boss))
                return;

            m_bossAttackCooldown = 0.8f;

            SetBossState(registry, BossState::Idle);
        }

        DirectX::XMFLOAT3 direction
        {
            playerTransform->position.x - bossTransform->position.x,
            0.0f,
            playerTransform->position.z - bossTransform->position.z
        };

        float distance = std::sqrt(direction.x * direction.x + direction.z * direction.z);

        if (distance > m_bossDetectionRange)
        {
            SetBossState(registry, BossState::Idle);
            return;
        }

        if (distance > 0.0001f)
        {
            direction.x /= distance;
            direction.z /= distance;

            FaceDirection(*bossTransform, direction, deltaTime, m_bossRotationSpeed);
        }

        if (distance <= m_bossAttackRange)
        {
            if (m_bossAttackCooldown <= 0.0f)
                SetBossState(registry, BossState::Attack);
            else
                SetBossState(registry, BossState::Idle);

            return;
        }

        bossTransform->position.x += direction.x * m_bossMoveSpeed * deltaTime;
        bossTransform->position.z += direction.z * m_bossMoveSpeed * deltaTime;

        SetBossState(registry, BossState::Chase);
    }

    DirectX::XMFLOAT3 GetMovementDirection()
    {
        float inputX = 0.0f;
        float inputZ = 0.0f;

        if (GameInput::IsKeyDown(GameInput::KeyCode::W)) inputZ += 1.0f;
        if (GameInput::IsKeyDown(GameInput::KeyCode::S)) inputZ -= 1.0f;
        if (GameInput::IsKeyDown(GameInput::KeyCode::D)) inputX += 1.0f;
        if (GameInput::IsKeyDown(GameInput::KeyCode::A)) inputX -= 1.0f;

        if (inputX == 0.0f && inputZ == 0.0f)
            return { 0.0f, 0.0f, 0.0f };

        DirectX::XMVECTOR yawRotation = DirectX::XMQuaternionRotationRollPitchYaw(0.0f, m_cameraYaw, 0.0f);
        DirectX::XMVECTOR forward = DirectX::XMVector3Rotate(DirectX::XMVectorSet(0, 0, 1, 0), yawRotation);
        DirectX::XMVECTOR right = DirectX::XMVector3Rotate(DirectX::XMVectorSet(1, 0, 0, 0), yawRotation);

        DirectX::XMVECTOR direction = DirectX::XMVectorAdd(DirectX::XMVectorScale(forward, inputZ), DirectX::XMVectorScale(right, inputX));
        direction = DirectX::XMVector3Normalize(direction);

        DirectX::XMFLOAT3 result{};

        DirectX::XMStoreFloat3(&result, direction);

        result.y = 0.0f;

        return result;
    }

    void FaceDirection(TransformComponent& transform, const DirectX::XMFLOAT3& direction, float deltaTime, float rotationSpeed)
    {
        if (direction.x == 0.0f && direction.z == 0.0f)
            return;

        float yaw = std::atan2(direction.x, direction.z);

        DirectX::XMVECTOR currentRotation = DirectX::XMLoadFloat4(&transform.rotation);
        DirectX::XMVECTOR targetRotation = DirectX::XMQuaternionRotationRollPitchYaw(0.0f, yaw, 0.0f);

        float factor = 1.0f - std::exp(-rotationSpeed * deltaTime);

        DirectX::XMVECTOR rotation = DirectX::XMQuaternionSlerp(currentRotation, targetRotation, factor);
        rotation = DirectX::XMQuaternionNormalize(rotation);

        DirectX::XMStoreFloat4(&transform.rotation, rotation);
    }

    void NormalizeDirection(DirectX::XMFLOAT3& direction)
    {
        float lengthSquared = direction.x * direction.x + direction.z * direction.z;

        if (lengthSquared <= 0.0f)
            return;

        float length = std::sqrt(lengthSquared);

        direction.x /= length;
        direction.z /= length;
    }

    void UpdateCameraInput()
    {
        if (GameInput::IsMouseButtonDown(GameInput::MouseButton::Right))
        {
            GameInput::SetMouseMode(GameInput::MouseMode::Relative);

            m_cameraYaw += static_cast<float>(GameInput::GetMouseDeltaX()) * m_cameraSensitivity;
            m_cameraPitch += static_cast<float>(GameInput::GetMouseDeltaY()) * m_cameraSensitivity;

            m_cameraPitch = std::clamp(m_cameraPitch, -0.90f, 0.35f);
        }
        else
        {
            GameInput::SetMouseMode(GameInput::MouseMode::Absolute);
        }
    }

    void UpdateThirdPersonCamera(entt::registry& registry)
    {
        TransformComponent* playerTransform = registry.try_get<TransformComponent>(m_player);
        TransformComponent* cameraTransform = registry.try_get<TransformComponent>(m_camera);

        if (!playerTransform || !cameraTransform)
            return;

        DirectX::XMVECTOR target = DirectX::XMVectorSet(playerTransform->position.x, playerTransform->position.y + m_cameraTargetHeight, playerTransform->position.z, 1.0f);
        DirectX::XMVECTOR cameraRotation = DirectX::XMQuaternionRotationRollPitchYaw(m_cameraPitch, m_cameraYaw, 0.0f);
        DirectX::XMVECTOR forward = DirectX::XMVector3Rotate(DirectX::XMVectorSet(0, 0, 1, 0), cameraRotation);
        DirectX::XMVECTOR cameraPosition = DirectX::XMVectorSubtract(target, DirectX::XMVectorScale(forward, m_cameraDistance));

        DirectX::XMStoreFloat3(&cameraTransform->position, cameraPosition);
        DirectX::XMStoreFloat4(&cameraTransform->rotation, cameraRotation);
    }

private:
    PlayerState m_playerState = PlayerState::Death;
    BossState m_bossState = BossState::Death;

    float m_walkSpeed = 3.0f;
    float m_runSpeed = 15.0f;
    float m_rollSpeed = 8.0f;

    float m_playerRotationSpeed = 8.0f;

    float m_bossMoveSpeed = 2.2f;
    float m_bossRotationSpeed = 5.0f;
    float m_bossDetectionRange = 12.0f;
    float m_bossAttackRange = 2.3f;
    float m_bossAttackCooldown = 0.0f;

    DirectX::XMFLOAT3 m_rollDirection{ 0.0f, 0.0f, 1.0f };

    float m_cameraYaw = 0.0f;
    float m_cameraPitch = 0.0f;
    float m_cameraDistance = 10.0f;
    float m_cameraTargetHeight = 5.4f;
    float m_cameraSensitivity = 0.0025f;

    bool m_previousAttack = false;
    bool m_previousRoll = false;
    bool m_previousDeath = false;
};

int main()
{
    Game game;
    game.Run();

    return 0;
}