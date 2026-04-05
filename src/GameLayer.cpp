#include "GameLayer.h"
#include "imgui/imgui.h"

#include <glm/gtc/matrix_transform.hpp>
#include <random>
#include <algorithm>

static std::mt19937 s_Rng{ std::random_device{}() };

// -------------------------------------------------------------------------

GameLayer::GameLayer()
	: Layer("GameLayer"),
	  m_Camera(-HalfW, HalfW, -HalfH, HalfH)
{
}

void GameLayer::OnAttach()
{
	LI::Renderer2D::Init();
	m_Font = LI::Font::Create("assets/fonts/OpenSans-VariableFont_wdth,wght.ttf", 48);
	InitPostProcess();
	ResetGame();
	m_State = GameState::Playing;
}

void GameLayer::OnDetach()
{
	LI::Renderer2D::Shutdown();
}

void GameLayer::ResetGame()
{
	m_Player        = Player{};
	m_Obstacles.clear();
	m_SpawnTimer    = 0.0f;
	m_SpawnInterval = 1.5f;
	m_Distance      = 0.0f;
	m_FlashTimer    = 0.0f;
	m_BoostLocked   = false;
	m_Score         = 0;
}

// -------------------------------------------------------------------------
// Post-process setup
// -------------------------------------------------------------------------

void GameLayer::InitPostProcess()
{
	// FBO
	m_Framebuffer = LI::Framebuffer::Create({ ViewportW, ViewportH });

	// 后处理 shader
	m_PostShader = LI::Shader::Create(
		"assets/shaders/PostVs.glsl",
		"assets/shaders/PostFs.glsl");
	m_PostShader->Bind();
	m_PostShader->SetInt("u_Screen", 0);

	// 全屏 Quad（NDC 坐标，不需要 MVP）
	float vertices[] = {
		// Position    // TexCoord
		-1.0f, -1.0f,  0.0f, 0.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,
		 1.0f,  1.0f,  1.0f, 1.0f,
		-1.0f,  1.0f,  0.0f, 1.0f,
	};
	uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

	auto vb = LI::VertexBuffer::Create(vertices, sizeof(vertices));
	vb->SetLayout({
		{ LI::ShaderDataType::Float2, "a_Position" },
		{ LI::ShaderDataType::Float2, "a_TexCoord" }
	});

	m_FullscreenQuad = LI::VertexArray::Create();
	m_FullscreenQuad->AddVertexBuffer(vb);
	m_FullscreenQuad->SetIndexBuffer(LI::IndexBuffer::Create(indices, 6));
}

void GameLayer::RenderPostProcess()
{
	float flash = glm::clamp(m_FlashTimer, 0.0f, 1.0f);

	m_Framebuffer->BindColorAttachment(0);
	m_PostShader->Bind();
	m_PostShader->SetFloat("u_FlashIntensity", flash);

	LI::RenderCommand::DrawIndexed(m_FullscreenQuad);
}

// -------------------------------------------------------------------------
// Update
// -------------------------------------------------------------------------

void GameLayer::OnUpdate(LI::Timestep ts)
{
	float dt = ts;

	if (m_State == GameState::Playing)
		UpdatePlaying(dt);

	// 死亡红屏计时衰减
	if (m_FlashTimer > 0.0f)
		m_FlashTimer -= dt * 2.0f;

	// 相机跟随
	m_Camera.SetPosition({ m_Player.Position.x + HalfW * 0.2f, 0.0f, 0.0f });

	// --- 第一 Pass：场景渲染到 FBO ---
	m_Framebuffer->Bind();
	LI::RenderCommand::SetClearColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	LI::RenderCommand::Clear();
	RenderScene();
	m_Framebuffer->Unbind();

	// --- 第二 Pass：后处理全屏 Quad 画到屏幕 ---
	LI::RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 1.0f });
	LI::RenderCommand::Clear();
	RenderPostProcess();
}

void GameLayer::UpdatePlaying(float dt)
{
	// --- 水平速度随时间加速 ---
	m_Player.HSpeed = glm::min(m_Player.HSpeed + HAccel * dt, HSpeedMax);

	// --- 空格加速（消耗能量，只影响垂直速度）---
	// 能量耗尽后锁定，必须恢复到 20 才能再次激活，防止微量能量触发闪烁加速
	static constexpr float BoostUnlockThreshold = 20.0f;
	if (m_BoostLocked && m_Player.Energy >= BoostUnlockThreshold)
		m_BoostLocked = false;

	bool wantBoost = LI::Input::IsKeyPressed(LI_KEY_SPACE);
	bool boosting  = wantBoost && !m_BoostLocked && m_Player.Energy > 0.0f;

	if (boosting)
	{
		m_Player.Energy = glm::max(0.0f, m_Player.Energy - EnergyDrain * dt);
		if (m_Player.Energy <= 0.0f)
			m_BoostLocked = true;
	}
	else
		m_Player.Energy = glm::min(MaxEnergy, m_Player.Energy + EnergyRegen * dt);

	float effectiveVSpeed = boosting ? m_Player.VSpeed * BoostMul : m_Player.VSpeed;

	// --- 垂直输入（上/下 或 W/S）---
	float vy = 0.0f;
	if (LI::Input::IsKeyPressed(LI_KEY_UP)   || LI::Input::IsKeyPressed(LI_KEY_W))
		vy =  effectiveVSpeed;
	if (LI::Input::IsKeyPressed(LI_KEY_DOWN) || LI::Input::IsKeyPressed(LI_KEY_S))
		vy = -effectiveVSpeed;

	m_Player.Position.x += m_Player.HSpeed * dt;
	m_Player.Position.y += vy * dt;

	float halfPH = m_Player.Size.y * 0.5f;
	m_Player.Position.y = glm::clamp(m_Player.Position.y, -HalfH + halfPH, HalfH - halfPH);

	m_Distance = m_Player.Position.x;

	SpawnObstacles(dt);

	float cullX = m_Player.Position.x - HalfW - 2.0f;
	m_Obstacles.erase(
		std::remove_if(m_Obstacles.begin(), m_Obstacles.end(),
			[cullX](const Obstacle& o) { return o.Position.x + o.Size.x * 0.5f < cullX; }),
		m_Obstacles.end());

	CheckCollisions();
}

void GameLayer::SpawnObstacles(float dt)
{
	m_SpawnTimer -= dt;
	if (m_SpawnTimer > 0.0f) return;

	m_SpawnInterval = glm::max(0.35f, 1.5f - m_Player.HSpeed * 0.04f);
	m_SpawnTimer    = m_SpawnInterval;

	float gapSize  = glm::max(1.5f, 4.0f - m_Player.HSpeed * 0.1f);
	float gapRange = HalfH * 2.0f - gapSize - 2.0f;

	std::uniform_real_distribution<float> gapDist(-gapRange * 0.5f, gapRange * 0.5f);
	std::uniform_real_distribution<float> wDist(0.4f, 1.2f);

	float gapCenter = gapDist(s_Rng);
	float spawnX    = m_Player.Position.x + HalfW + 2.0f;
	float blockW    = wDist(s_Rng);

	// 随机选择颜色（黑 / 黄 / 蓝），同一列上下两块颜色相同
	std::uniform_int_distribution<int> colorDist(0, 2);
	BlockColor blockColor = static_cast<BlockColor>(colorDist(s_Rng));
	glm::vec4 renderColor;
	switch (blockColor) {
		case BlockColor::Yellow: renderColor = { 1.0f, 0.85f, 0.0f, 1.0f }; break;
		case BlockColor::Blue:   renderColor = { 0.1f, 0.45f, 1.0f, 1.0f }; break;
		default:                 renderColor = { 0.1f, 0.1f,  0.1f, 1.0f }; break;
	}

	float topEdge = gapCenter + gapSize * 0.5f;
	float topH    = HalfH - topEdge;
	Obstacle top;
	top.Position  = { spawnX, HalfH - topH * 0.5f };
	top.Size      = { blockW, topH };
	top.Type      = ObstacleType::Death;
	top.ColorType = blockColor;
	top.Color     = renderColor;
	m_Obstacles.push_back(top);

	float botEdge = gapCenter - gapSize * 0.5f;
	float botH    = HalfH + botEdge;
	Obstacle bot;
	bot.Position  = { spawnX, -HalfH + botH * 0.5f };
	bot.Size      = { blockW, botH };
	bot.Type      = ObstacleType::Death;
	bot.ColorType = blockColor;
	bot.Color     = renderColor;
	m_Obstacles.push_back(bot);
}

void GameLayer::CheckCollisions()
{
	float px  = m_Player.Position.x, py  = m_Player.Position.y;
	float phw = m_Player.Size.x * 0.5f, phh = m_Player.Size.y * 0.5f;

	for (auto& ob : m_Obstacles)
	{
		float ohw = ob.Size.x * 0.5f, ohh = ob.Size.y * 0.5f;
		bool hit = px + phw > ob.Position.x - ohw &&
		           px - phw < ob.Position.x + ohw &&
		           py + phh > ob.Position.y - ohh &&
		           py - phh < ob.Position.y + ohh;
		if (!hit) continue;

		// 判断颜色是否匹配（黑色始终死亡）
		bool sameColor =
			(ob.ColorType == BlockColor::Yellow && m_Player.ColorIndex == PlayerColor::Yellow) ||
			(ob.ColorType == BlockColor::Blue   && m_Player.ColorIndex == PlayerColor::Blue);

		if (sameColor)
		{
			// 同色：得分（每块只计一次）
			if (!ob.Scored)
			{
				ob.Scored = true;
				m_Score++;
			}
		}
		else
		{
			// 异色或黑色：死亡
			m_State      = GameState::GameOver;
			m_FlashTimer = 1.0f;
			return;
		}
	}
}

// -------------------------------------------------------------------------
// Render
// -------------------------------------------------------------------------

void GameLayer::RenderScene()
{
	LI::Renderer2D::ResetStats();
	LI::Renderer2D::BeginScene(m_Camera);

	for (const auto& ob : m_Obstacles)
		LI::Renderer2D::DrawQuad(ob.Position, ob.Size, ob.Color);

	LI::Renderer2D::DrawQuad(m_Player.Position, m_Player.Size, m_Player.GetColor());

	// --- 能量条（跟随相机，固定在画面底部）---
	float camCenterX  = m_Player.Position.x + HalfW * 0.2f;
	float barMaxWidth = HalfW * 1.6f;   // 能量条最大宽度（占屏幕 80%）
	float barHeight   = 0.25f;
	float barY        = -HalfH + barHeight * 0.5f + 0.1f;

	float energyRatio = m_Player.Energy / MaxEnergy;
	float greenWidth  = barMaxWidth * energyRatio;
	float grayWidth   = barMaxWidth * (1.0f - energyRatio);
	float barLeft     = camCenterX - barMaxWidth * 0.5f;

	// 绿色段（已有能量，左侧）
	if (greenWidth > 0.0f)
		LI::Renderer2D::DrawQuad(
			{ barLeft + greenWidth * 0.5f, barY },
			{ greenWidth, barHeight },
			{ 0.2f, 0.9f, 0.3f, 1.0f });

	// 灰色段（已消耗能量，右侧）
	if (grayWidth > 0.0f)
		LI::Renderer2D::DrawQuad(
			{ barLeft + greenWidth + grayWidth * 0.5f, barY },
			{ grayWidth, barHeight },
			{ 0.3f, 0.3f, 0.3f, 1.0f });

	// --- 右上角分数（跟随相机，固定在画面右上方）---
	if (m_Font)
	{
		std::string scoreStr = "Score: " + std::to_string(m_Score);
		float textH = 0.55f;  // 文字高度（世界坐标）
		float textW = LI::TextRenderer::MeasureWidth(scoreStr, textH, m_Font);
		float textX = camCenterX + HalfW - textW - 0.2f;  // 右对齐，留 0.2 边距
		float textY = HalfH - textH - 0.15f;               // 顶部留 0.15 边距

		LI::TextRenderer::DrawText(scoreStr,
			{ textX, textY },
			textH,
			{ 1.0f, 1.0f, 1.0f, 1.0f },
			m_Font);
	}

	LI::Renderer2D::EndScene();
}

// -------------------------------------------------------------------------
// ImGui
// -------------------------------------------------------------------------

void GameLayer::OnImGuiRender()
{
	ImGui::Begin("ColorfulBlock");

	const char* stateNames[] = { "Menu", "Playing", "Game Over" };
	ImGui::Text("State    : %s", stateNames[(int)m_State]);
	ImGui::Text("Distance : %.1f", m_Distance);
	ImGui::Text("Speed    : %.2f", m_Player.HSpeed);
	ImGui::Separator();

	auto stats = LI::Renderer2D::GetStats();
	ImGui::Text("DrawCalls: %d", stats.DrawCalls);
	ImGui::Text("Quads    : %d", stats.QuadCount);
	ImGui::Separator();

	if (m_State != GameState::Playing)
	{
		if (ImGui::Button("Start") || LI::Input::IsKeyPressed(LI_KEY_ENTER))
		{
			ResetGame();
			m_State = GameState::Playing;
		}
	}

	ImGui::End();
}

// -------------------------------------------------------------------------
// Events
// -------------------------------------------------------------------------

void GameLayer::OnEvent(LI::Event& e)
{
	LI::EventDispatcher dispatcher(e);
	dispatcher.Dispatch<LI::KeyPressedEvent>(LI_BIND_EVENT_FN(GameLayer::OnKeyPressed));
}

bool GameLayer::OnKeyPressed(LI::KeyPressedEvent& e)
{
	if (e.GetKeyCode() == LI_KEY_ENTER)
	{
		ResetGame();
		m_State = GameState::Playing;
		return true;
	}
	if (e.GetKeyCode() == LI_KEY_LEFT_SHIFT || e.GetKeyCode() == LI_KEY_RIGHT_SHIFT)
	{
		if (m_State == GameState::Playing)
		{
			m_Player.ColorIndex = (m_Player.ColorIndex == PlayerColor::Yellow)
				? PlayerColor::Blue
				: PlayerColor::Yellow;
		}
		return true;
	}
	return false;
}
