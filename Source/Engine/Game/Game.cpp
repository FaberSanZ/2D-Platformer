#include <algorithm>
#include <cmath>
#include "GameBase.h"

enum class PlayerState
{
    None,
    Idle,
    Walk,
    Run,
    Roll,
    Attack,
    Death
};

enum class BossState
{
    None,
    Idle,
    Chase,
    Attack,
    Death
};

struct PlayerComponent
{
    PlayerState state = PlayerState::None;
    DirectX::XMFLOAT3 rollDirection{ 0.0f, 0.0f, 1.0f };
    bool previousAttack = false;
    bool previousRoll = false;
    bool previousDeath = false;
};

struct BossComponent
{
    BossState state = BossState::None;
    float attackCooldown = 0.0f;
};

class Game final : public GameBase
{
protected:
    Model* m_knightModel = nullptr;
    Model* m_spiderModel = nullptr;

    void OnInitialize(entt::registry& registry) override
    {
        Serializer().RegisterTag<PlayerComponent>("Player");
        Serializer().RegisterTag<BossComponent>("Boss");

        m_knightModel = Assets().LoadModel("../Assets/Models/KnightCharacter.glb");
        m_spiderModel = Assets().LoadModel("../Assets/Models/Spider.glb");

        entt::entity player = CreateKnight(registry, { -4.0f, 0.0f, 0.0f });
        entt::entity boss = CreateSpider(registry, { 4.0f, 0.0f, 0.0f });
        CreateCamera(registry);

        auto& playerComponent = registry.get<PlayerComponent>(player);
        auto& bossComponent = registry.get<BossComponent>(boss);

        SetPlayerState(registry, player, playerComponent, PlayerState::Idle);
        SetBossState(registry, boss, bossComponent, BossState::Idle);

        m_cameraYaw = DirectX::XMConvertToRadians(90.0f);
        m_cameraPitch = -0.22f;

        UpdateThirdPersonCamera(registry);
    }

    void OnUpdate(entt::registry& registry, float deltaTime) override
    {
        UpdateCameraInput();
        UpdatePlayer(registry, deltaTime);
        UpdateBoss(registry, deltaTime);
        UpdateThirdPersonCamera(registry);
    }

    void OnDestroy(entt::registry& registry) override
    {
        GameInput::SetMouseMode(GameInput::MouseMode::Absolute);
    }

private:
    entt::entity CreateKnight(entt::registry& registry, const DirectX::XMFLOAT3& position)
    {
        entt::entity entity = CreateEntity(registry, "Knight");

        auto& transform = registry.get<TransformComponent>(entity);
        transform.position = position;
        transform.scale = { 1.0f, 1.0f, 1.0f };

        DirectX::XMStoreFloat4(&transform.rotation, DirectX::XMQuaternionRotationRollPitchYaw(0.0f, DirectX::XMConvertToRadians(90.0f), 0.0f));

        auto& model = registry.emplace<ModelComponent>(entity);
        model.model = m_knightModel;
        model.assetPath = "../Assets/Models/KnightCharacter.glb";
        model.color = { 1.0f, 1.0f, 1.0f, 1.0f };

        registry.emplace<PlayerComponent>(entity);

        return entity;
    }

    entt::entity CreateSpider(entt::registry& registry, const DirectX::XMFLOAT3& position)
    {
        entt::entity entity = CreateEntity(registry, "Spider");

        auto& transform = registry.get<TransformComponent>(entity);
        transform.position = position;
        transform.scale = { 1.0f, 1.0f, 1.0f };

        DirectX::XMStoreFloat4(&transform.rotation, DirectX::XMQuaternionRotationRollPitchYaw(0.0f, DirectX::XMConvertToRadians(-90.0f), 0.0f));

        auto& model = registry.emplace<ModelComponent>(entity);
        model.model = m_spiderModel;
        model.assetPath = "../Assets/Models/Spider.glb";
        model.color = { 1.0f, 0.2f, 0.15f, 1.0f };

        registry.emplace<BossComponent>(entity);

        return entity;
    }

    entt::entity CreateCamera(entt::registry& registry)
    {
        entt::entity entity = CreateEntity(registry, "Camera");

        auto& transform = registry.get<TransformComponent>(entity);
        transform.position = { -10.0f, 3.0f, 0.0f };

        registry.emplace<CameraComponent>(entity);
        registry.emplace<PrimaryCameraComponent>(entity);

        return entity;
    }

    void SetPlayerState(entt::registry& registry, entt::entity entity, PlayerComponent& player, PlayerState state)
    {
        if (player.state == state)
            return;

        player.state = state;

        switch (state)
        {
        case PlayerState::Idle: Animations().Play(registry, entity, "Idle", true, 0.25f); break;
        case PlayerState::Walk: Animations().Play(registry, entity, "Walking", true, 0.25f); break;
        case PlayerState::Run: Animations().Play(registry, entity, "Run", true, 0.20f); break;
        case PlayerState::Roll: Animations().Play(registry, entity, "Roll", false, 0.08f); break;
        case PlayerState::Attack: Animations().Play(registry, entity, "swordAttackJump", false, 0.10f); break;
        case PlayerState::Death: Animations().Play(registry, entity, "Death", false, 0.20f); break;
        case PlayerState::None: break;
        }
    }

    void SetBossState(entt::registry& registry, entt::entity entity, BossComponent& boss, BossState state)
    {
        if (boss.state == state)
            return;

        boss.state = state;

        switch (state)
        {
        case BossState::Idle: Animations().Play(registry, entity, "SpiderArmature|Spider_Idle", true, 0.18f); break;
        case BossState::Chase: Animations().Play(registry, entity, "SpiderArmature|Spider_Walk", true, 0.12f); break;
        case BossState::Attack: Animations().Play(registry, entity, "SpiderArmature|Spider_Attack", false, 0.08f); break;
        case BossState::Death: Animations().Play(registry, entity, "SpiderArmature|Spider_Death", false, 0.20f); break;
        case BossState::None: break;
        }
    }

    void UpdatePlayer(entt::registry& registry, float deltaTime)
    {
        auto view = registry.view<PlayerComponent, TransformComponent>();

        for (auto [entity, player, transform] : view.each())
        {
            bool attackDown = GameInput::IsMouseButtonDown(GameInput::MouseButton::Left);
            bool rollDown = GameInput::IsKeyDown(GameInput::KeyCode::Space);
            bool deathDown = GameInput::IsKeyDown(GameInput::KeyCode::T);

            if (player.state == PlayerState::None)
            {
                player.previousAttack = attackDown;
                player.previousRoll = rollDown;
                player.previousDeath = deathDown;
                SetPlayerState(registry, entity, player, PlayerState::Idle);
                continue;
            }

            bool attackPressed = attackDown && !player.previousAttack;
            bool rollPressed = rollDown && !player.previousRoll;
            bool deathPressed = deathDown && !player.previousDeath;

            player.previousAttack = attackDown;
            player.previousRoll = rollDown;
            player.previousDeath = deathDown;

            if (deathPressed)
            {
                SetPlayerState(registry, entity, player, PlayerState::Death);
                continue;
            }

            if (player.state == PlayerState::Death)
                continue;

            if (player.state == PlayerState::Roll)
            {
                if (!Animations().Finished(registry, entity))
                {
                    transform.position.x += player.rollDirection.x * m_rollSpeed * deltaTime;
                    transform.position.z += player.rollDirection.z * m_rollSpeed * deltaTime;
                    continue;
                }

                SetPlayerState(registry, entity, player, PlayerState::Idle);
            }

            if (player.state == PlayerState::Attack)
            {
                if (!Animations().Finished(registry, entity))
                    continue;

                SetPlayerState(registry, entity, player, PlayerState::Idle);
            }

            DirectX::XMFLOAT3 movementDirection = GetMovementDirection();
            bool moving = movementDirection.x != 0.0f || movementDirection.z != 0.0f;

            if (attackPressed)
            {
                SetPlayerState(registry, entity, player, PlayerState::Attack);
                continue;
            }

            if (rollPressed)
            {
                if (moving)
                {
                    player.rollDirection = movementDirection;
                }
                else
                {
                    DirectX::XMVECTOR rotation = DirectX::XMLoadFloat4(&transform.rotation);
                    DirectX::XMVECTOR forward = DirectX::XMVector3Rotate(DirectX::XMVectorSet(0, 0, 1, 0), rotation);

                    DirectX::XMStoreFloat3(&player.rollDirection, forward);

                    player.rollDirection.y = 0.0f;

                    NormalizeDirection(player.rollDirection);
                }

                FaceDirection(transform, player.rollDirection, deltaTime, 14.0f);
                SetPlayerState(registry, entity, player, PlayerState::Roll);
                continue;
            }

            if (!moving)
            {
                SetPlayerState(registry, entity, player, PlayerState::Idle);
                continue;
            }

            bool running = GameInput::IsKeyDown(GameInput::KeyCode::Shift);
            float speed = running ? m_runSpeed : m_walkSpeed;

            transform.position.x += movementDirection.x * speed * deltaTime;
            transform.position.z += movementDirection.z * speed * deltaTime;

            FaceDirection(transform, movementDirection, deltaTime, m_playerRotationSpeed);
            SetPlayerState(registry, entity, player, running ? PlayerState::Run : PlayerState::Walk);
        }
    }

    void UpdateBoss(entt::registry& registry, float deltaTime)
    {
        auto playerView = registry.view<PlayerComponent, TransformComponent>();
        entt::entity playerEntity = playerView.front();

        if (playerEntity == entt::null)
            return;

        const TransformComponent& playerTransform = registry.get<TransformComponent>(playerEntity);

        auto bossView = registry.view<BossComponent, TransformComponent>();

        for (auto [entity, boss, transform] : bossView.each())
        {
            if (boss.state == BossState::None)
            {
                SetBossState(registry, entity, boss, BossState::Idle);
                continue;
            }

            if (boss.state == BossState::Death)
                continue;

            if (boss.attackCooldown > 0.0f)
                boss.attackCooldown -= deltaTime;

            if (boss.state == BossState::Attack)
            {
                if (!Animations().Finished(registry, entity))
                    continue;

                boss.attackCooldown = 0.8f;
                SetBossState(registry, entity, boss, BossState::Idle);
            }

            DirectX::XMFLOAT3 direction
            {
                playerTransform.position.x - transform.position.x,
                0.0f,
                playerTransform.position.z - transform.position.z
            };

            float distance = std::sqrt(direction.x * direction.x + direction.z * direction.z);

            if (distance > m_bossDetectionRange)
            {
                SetBossState(registry, entity, boss, BossState::Idle);
                continue;
            }

            if (distance > 0.0001f)
            {
                direction.x /= distance;
                direction.z /= distance;

                FaceDirection(transform, direction, deltaTime, m_bossRotationSpeed);
            }

            if (distance <= m_bossAttackRange)
            {
                if (boss.attackCooldown <= 0.0f)
                    SetBossState(registry, entity, boss, BossState::Attack);
                else
                    SetBossState(registry, entity, boss, BossState::Idle);

                continue;
            }

            transform.position.x += direction.x * m_bossMoveSpeed * deltaTime;
            transform.position.z += direction.z * m_bossMoveSpeed * deltaTime;

            SetBossState(registry, entity, boss, BossState::Chase);
        }
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
        auto playerView = registry.view<PlayerComponent, TransformComponent>();
        entt::entity playerEntity = playerView.front();

        if (playerEntity == entt::null)
            return;

        auto cameraView = registry.view<PrimaryCameraComponent, TransformComponent>();
        entt::entity cameraEntity = cameraView.front();

        if (cameraEntity == entt::null)
            return;

        const TransformComponent& playerTransform = registry.get<TransformComponent>(playerEntity);
        TransformComponent& cameraTransform = registry.get<TransformComponent>(cameraEntity);

        DirectX::XMVECTOR target = DirectX::XMVectorSet(playerTransform.position.x, playerTransform.position.y + m_cameraTargetHeight, playerTransform.position.z, 1.0f);
        DirectX::XMVECTOR cameraRotation = DirectX::XMQuaternionRotationRollPitchYaw(m_cameraPitch, m_cameraYaw, 0.0f);
        DirectX::XMVECTOR forward = DirectX::XMVector3Rotate(DirectX::XMVectorSet(0, 0, 1, 0), cameraRotation);
        DirectX::XMVECTOR cameraPosition = DirectX::XMVectorSubtract(target, DirectX::XMVectorScale(forward, m_cameraDistance));

        DirectX::XMStoreFloat3(&cameraTransform.position, cameraPosition);
        DirectX::XMStoreFloat4(&cameraTransform.rotation, cameraRotation);
    }

private:
    float m_walkSpeed = 3.0f;
    float m_runSpeed = 15.0f;
    float m_rollSpeed = 8.0f;
    float m_playerRotationSpeed = 8.0f;

    float m_bossMoveSpeed = 2.2f;
    float m_bossRotationSpeed = 5.0f;
    float m_bossDetectionRange = 12.0f;
    float m_bossAttackRange = 2.3f;

    float m_cameraYaw = 0.0f;
    float m_cameraPitch = 0.0f;
    float m_cameraDistance = 10.0f;
    float m_cameraTargetHeight = 5.4f;
    float m_cameraSensitivity = 0.0025f;
};

int main()
{
    Game game;
    game.Run();

    return 0;
}
