#pragma once

#include "LI.h"
#include <vector>

enum class GameState { Menu, Playing, GameOver };
enum class ObstacleType { Death };

struct Player
{
	glm::vec2 Position  = { 0.0f, 0.0f };
	glm::vec2 Size      = { 0.4f, 0.4f };
	glm::vec4 Color     = { 0.0f, 0.0f, 0.0f, 1.0f };
	float     HSpeed    = 4.0f;   // 当前水平速度（随时间增加）
	float     VSpeed    = 6.0f;   // 垂直控制速度
	float     Energy    = 100.0f; // 当前能量
};

static constexpr float MaxEnergy    = 100.0f;
static constexpr float BoostMul     = 2.0f;   // 加速时速度倍率
static constexpr float EnergyDrain  = 30.0f;  // 加速时能量消耗/秒
static constexpr float EnergyRegen  = 8.0f;   // 未加速时能量恢复/秒

struct Obstacle
{
	glm::vec2    Position;
	glm::vec2    Size;
	ObstacleType Type;
	glm::vec4    Color;
};

class GameLayer : public LI::Layer
{
public:
	static constexpr float HalfH = 5.0f;
	static constexpr float HalfW = HalfH * (16.0f / 9.0f);

	GameLayer();
	virtual ~GameLayer() = default;

	virtual void OnAttach()                    override;
	virtual void OnDetach()                    override;
	virtual void OnUpdate(LI::Timestep ts)     override;
	virtual void OnImGuiRender()               override;
	virtual void OnEvent(LI::Event& e)         override;

private:
	void ResetGame();
	void UpdatePlaying(float dt);
	void SpawnObstacles(float dt);
	void CheckCollisions();
	void RenderScene();

	bool OnKeyPressed(LI::KeyPressedEvent& e);

	void InitPostProcess();
	void RenderPostProcess();

	// 水平加速参数
	static constexpr float HSpeedMax = 20.0f;
	static constexpr float HAccel    = 0.5f;   // 单位/秒²

	static constexpr uint32_t ViewportW = 1280;
	static constexpr uint32_t ViewportH = 720;

private:
	GameState              m_State  = GameState::Menu;
	LI::OrthographicCamera m_Camera;

	Player                 m_Player;
	std::vector<Obstacle>  m_Obstacles;

	float m_SpawnTimer    = 0.0f;
	float m_SpawnInterval = 1.5f;
	float m_Distance      = 0.0f;

	// 后处理
	LI::Ref<LI::Framebuffer>  m_Framebuffer;
	LI::Ref<LI::Shader>       m_PostShader;
	LI::Ref<LI::VertexArray>  m_FullscreenQuad;
	float                     m_FlashTimer = 0.0f;  // 死亡红屏倒计时
};
