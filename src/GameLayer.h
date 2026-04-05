#pragma once

#include "LI.h"
#include <vector>

enum class GameState    { Menu, Playing, GameOver };
enum class ObstacleType { Death };
enum class PlayerColor  { Yellow, Blue };
enum class BlockColor   { Black, Yellow, Blue };

struct Player
{
	glm::vec2   Position   = { 0.0f, 0.0f };
	glm::vec2   Size       = { 0.4f, 0.4f };
	PlayerColor ColorIndex = PlayerColor::Yellow;
	float       HSpeed     = 4.0f;    // 当前水平速度（随时间增加）
	float       VSpeed     = 6.0f;    // 垂直控制速度
	float       Energy     = 100.0f;  // 当前能量

	glm::vec4 GetColor() const {
		return ColorIndex == PlayerColor::Yellow
			? glm::vec4{ 1.0f, 0.85f, 0.0f, 1.0f }
			: glm::vec4{ 0.1f, 0.45f, 1.0f, 1.0f };
	}
};

static constexpr float MaxEnergy   = 100.0f;
static constexpr float BoostMul    = 2.0f;   // 加速时速度倍率
static constexpr float EnergyDrain = 30.0f;  // 加速时能量消耗/秒
static constexpr float EnergyRegen = 8.0f;   // 未加速时能量恢复/秒

struct Obstacle
{
	glm::vec2    Position;
	glm::vec2    Size;
	ObstacleType Type;
	BlockColor   ColorType;   // Black / Yellow / Blue
	glm::vec4    Color;       // 渲染颜色
	bool         Scored = false; // 已经触发过得分，避免同一块重复计分
};

class GameLayer : public LI::Layer
{
public:
	static constexpr float HalfH = 5.0f;
	static constexpr float HalfW = HalfH * (16.0f / 9.0f);

	GameLayer();
	virtual ~GameLayer() = default;

	virtual void OnAttach()                override;
	virtual void OnDetach()                override;
	virtual void OnUpdate(LI::Timestep ts) override;
	virtual void OnImGuiRender()           override;
	virtual void OnEvent(LI::Event& e)     override;

private:
	void ResetGame();
	void UpdatePlaying(float dt);
	void SpawnObstacles(float dt);
	void CheckCollisions();
	void RenderScene();

	bool OnKeyPressed(LI::KeyPressedEvent& e);

	void InitPostProcess();
	void RenderPostProcess();

	static constexpr float    HSpeedMax = 20.0f;
	static constexpr float    HAccel    = 0.5f;
	static constexpr uint32_t ViewportW = 1280;
	static constexpr uint32_t ViewportH = 720;

private:
	GameState              m_State = GameState::Menu;
	LI::OrthographicCamera m_Camera;

	Player                m_Player;
	std::vector<Obstacle> m_Obstacles;

	float m_SpawnTimer    = 0.0f;
	float m_SpawnInterval = 1.5f;
	float m_Distance      = 0.0f;
	int   m_Score         = 0;

	// 后处理
	LI::Ref<LI::Framebuffer> m_Framebuffer;
	LI::Ref<LI::Shader>      m_PostShader;
	LI::Ref<LI::VertexArray> m_FullscreenQuad;
	float                    m_FlashTimer  = 0.0f;
	bool                     m_BoostLocked = false;
};
