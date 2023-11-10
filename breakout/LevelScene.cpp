#include "LevelScene.h"
#include "Game.h"
#include "Paddle.h"
#include "Ball.h"
#include "ImpenetrableBrick.h"
#include "NormalBrick.h"

// assets
#include "LevelConfigurationAsset.h"
#include "FontAsset.h"
#include "SFXAsset.h"
#include "TextureAsset.h"

// collision
#include "AABBCollisionDetector.h"

#include <sstream>
#include <iostream>

LevelScene::LevelScene(Game& game, unsigned int level) : Scene(game), m_level(level), m_points(0), m_lives(3), m_state(LevelState::Starting)
{
	loadLevelConfigurationAsset(level);
	loadBackgroundAsset();
	loadPaddleTextureAsset();
	loadBallTextureAsset();
	loadBrickTextureAssets();
	loadBrickSFXAssets();
	loadFontAssets();
	createGameObjects();
	generateBricks();
	setupHUD();
	setupRoundStartText();
}

void LevelScene::processEvents()
{
	SDL_Event event;

	// check for window close event
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_QUIT:
			getGameRef().stopRunning();
			break;
		}
	}
}

void LevelScene::update(float delta)
{
	const Uint8* state = SDL_GetKeyboardState(NULL);
	if (state[SDL_SCANCODE_LEFT])
	{
		m_paddle->moveLeft(delta);
	}
	if (state[SDL_SCANCODE_RIGHT])
	{
		m_paddle->moveRight(delta);
	}
	if (state[SDL_SCANCODE_SPACE]) {
		// start the game if not already started
		m_state = LevelState::Playing;
	}

	if (m_state == LevelState::Playing) {
		m_ball->updatePosition(delta);

		// check for collisions
		checkCollisions();
	}
}

void LevelScene::render(SDL_Renderer* renderer)
{
	renderLevelBackground(renderer);
	renderLevelBricks(renderer);
	renderBall(renderer);
	renderPaddle(renderer);
	renderHUD(renderer);
	renderRoundStartText(renderer);
}

void LevelScene::loadLevelConfigurationAsset(unsigned int level)
{
	std::stringstream path_ss;
	path_ss << "./assets/levels/level-" << level << ".xml";
	auto level_path = path_ss.str();

	m_assetManager.loadAsset<LevelConfigurationAsset>(level_path, "LevelConfiguration");
}

void LevelScene::loadBackgroundAsset()
{
	std::stringstream path_ss;

	auto& level_config = m_assetManager.getAsset<LevelConfigurationAsset>("LevelConfiguration");

	// create game objects from the level configuration
	auto background_texture = level_config.getBackgroundTexture();

	path_ss << "./assets/textures/" << background_texture;

	auto background_path = path_ss.str();

	m_assetManager.loadAsset<TextureAsset>(background_path, background_texture, getGameRef().getRenderer());
}

void LevelScene::loadBrickTextureAssets()
{
	auto& level_config = m_assetManager.getAsset<LevelConfigurationAsset>("LevelConfiguration");

	for (auto &bt : level_config.getBrickTypes()) {
		std::stringstream path_ss;

		auto texture_name = bt.second.getBrickTextureName();

		path_ss << "./assets/textures/" << texture_name;

		auto texture_path = path_ss.str();

		m_assetManager.loadAsset<TextureAsset>(texture_path, texture_name, getGameRef().getRenderer());
	}
}

void LevelScene::loadPaddleTextureAsset()
{
	std::stringstream path_ss;

	auto& level_config = m_assetManager.getAsset<LevelConfigurationAsset>("LevelConfiguration");

	auto paddle_texture = level_config.getPaddleTexture();

	path_ss << "./assets/textures/" << paddle_texture;

	auto texture_path = path_ss.str();

	m_assetManager.loadAsset<TextureAsset>(texture_path, paddle_texture, getGameRef().getRenderer());
}

void LevelScene::loadBallTextureAsset()
{
	std::stringstream path_ss;

	auto& level_config = m_assetManager.getAsset<LevelConfigurationAsset>("LevelConfiguration");

	auto ball_texture = level_config.getBallTexture();

	path_ss << "./assets/textures/" << ball_texture;

	auto texture_path = path_ss.str();

	m_assetManager.loadAsset<TextureAsset>(texture_path, ball_texture, getGameRef().getRenderer());
}

void LevelScene::loadBrickSFXAssets()
{
	auto& level_config = m_assetManager.getAsset<LevelConfigurationAsset>("LevelConfiguration");

	for (auto& bt : level_config.getBrickTypes()) {
		std::stringstream hit_path_ss;
		std::stringstream break_path_ss;

		auto hit_sfx_name = bt.second.getHitSoundName();
		auto break_sfx_name = bt.second.getBreakSoundName();

		hit_path_ss << "./assets/sfx/" << hit_sfx_name;
		break_path_ss << "./assets/sfx/" << break_sfx_name;

		m_assetManager.loadAsset<SFXAsset>(hit_path_ss.str(), hit_sfx_name);
		if (!bt.second.isInfinite()) {
			m_assetManager.loadAsset<SFXAsset>(break_path_ss.str(), break_sfx_name);
		}
	}
}

void LevelScene::loadFontAssets()
{
	m_assetManager.loadAsset<FontAsset>("./assets/fonts/Roboto-Bold.ttf", "HUDFont", 100);
	m_assetManager.loadAsset<FontAsset>("./assets/fonts/ARCADECLASSIC.ttf", "TextFont", 100);
}

void LevelScene::generateBricks()
{
	auto& level_config = m_assetManager.getAsset<LevelConfigurationAsset>("LevelConfiguration");

	auto& bricks_layout = level_config.getBricksLayout();
	auto& brick_types = level_config.getBrickTypes();

	// use this position to calculate positions for each brick
	glm::vec2 brick_position(m_background->getPosition().x, m_background->getPosition().y + level_config.getRowSpacing());

	// use constant calculated size for each brick depending on a number of bricks and column spacing
	const auto bricks_row_count = level_config.getColumnCount();
	glm::vec2 brick_size = glm::vec2((m_background->getSize().x - (bricks_row_count + 1) * level_config.getColumnSpacing()) / (float)bricks_row_count, 15);

	// |- COL spacing -| |----| ... |----| |- COL spacing-|
	// |- ROW spacing -|
	// |- COL spacing -| |----| ... |----| |- COL spacing-|
	// NUM of col spacings = N_Bricks - 1 + 2 = N_Bricks + 1
	// Apply row spacing each time you enter a new row

	// generate bricks layout
	m_bricks.reserve(level_config.getRowCount());
	for (unsigned int i = 0; i < level_config.getRowCount(); i++) {
		m_bricks.push_back(std::vector<std::unique_ptr<Brick>>());
		brick_position.x = m_background->getPosition().x + (float)level_config.getColumnSpacing();
		for (unsigned int j = 0; j < level_config.getColumnCount(); j++) {
			auto& current_brick_sym = bricks_layout[i][j];

			if (current_brick_sym != '_') {
				auto& brick_type = brick_types[current_brick_sym];

				auto& brick_texture = m_assetManager.getAsset<TextureAsset>(brick_type.getBrickTextureName());

				if (brick_type.isInfinite()) {
					// add impenetrable brick to the list
					m_bricks[i].push_back(std::make_unique<ImpenetrableBrick>(brick_texture, brick_type, brick_position, brick_size));
				}
				else {
					// add normal brick to the list
					m_bricks[i].push_back(std::make_unique<NormalBrick>(brick_texture, brick_type, brick_position, brick_size));
				}
			}
			brick_position.x += brick_size.x + level_config.getColumnSpacing();
		}
		brick_position.y += brick_size.y + level_config.getRowSpacing();
	}
}

void LevelScene::createGameObjects()
{
	// get window size
	auto& level_config = m_assetManager.getAsset<LevelConfigurationAsset>("LevelConfiguration");

	auto &game_ref = getGameRef();
	auto renderer = game_ref.getRenderer();

	auto window_size = getWindowSize();

	// get all texture refs
	auto &paddle_texture = m_assetManager.getAsset<TextureAsset>(level_config.getPaddleTexture());
	auto &ball_texture = m_assetManager.getAsset<TextureAsset>(level_config.getBallTexture());
	auto &background_texture = m_assetManager.getAsset<TextureAsset>(level_config.getBackgroundTexture());

	// get fonts
	auto& hud_font = m_assetManager.getAsset<FontAsset>("HUDFont");
	auto& text_font = m_assetManager.getAsset<FontAsset>("TextFont");

	// create game objects
	m_paddle = std::make_unique<Paddle>(paddle_texture, level_config.getPaddleSpeed());
	m_ball = std::make_unique<Ball>(ball_texture, level_config.getBallSpeed());
	m_background = std::make_unique<Background>(background_texture, window_size);
	m_hud = std::make_unique<HeadsUpDisplay>(hud_font, m_lives, m_points, m_level, getWindowSize());
	m_roundStartText = std::make_unique<Text>(text_font);

	// setup collision detector
	m_collisionDetector = std::make_unique<AABBCollisionDetector>();
}

void LevelScene::setupHUD()
{

}

void LevelScene::setupRoundStartText()
{
	auto bg_pos = m_background->getPosition();
	auto bg_size = m_background->getSize();

	// set the text in the middle
	auto text_position = bg_pos + bg_size / 2.0f - bg_size / 4.0f;
	auto text_size = glm::vec2(bg_size.x / 2.0f, 50);

	m_roundStartText->setText("press space to start the game  !");
	m_roundStartText->setColor(SDL_Color{ 255, 255, 255 });
	m_roundStartText->setPosition(text_position);
	m_roundStartText->setSize(text_size);
}

void LevelScene::checkCollisions()
{
	if (m_collisionDetector->checkCollision(*m_ball, *m_paddle)) {
		std::cout << "Collision detected" << std::endl;
		m_ball->setDirection(glm::vec2(0.2, -1));
	}
}

void LevelScene::renderLevelBackground(SDL_Renderer* renderer)
{
	m_background->render(renderer);
}

void LevelScene::renderLevelBricks(SDL_Renderer* renderer)
{
	for (auto& i : m_bricks) {
		for (auto& j : i) {
			j->render(renderer);
		}
	}
}

void LevelScene::renderPaddle(SDL_Renderer* renderer)
{
	m_paddle->render(renderer);
}

void LevelScene::renderBall(SDL_Renderer* renderer)
{
	m_ball->render(renderer);
}

void LevelScene::renderHUD(SDL_Renderer* renderer)
{
	m_hud->render(renderer);
}

void LevelScene::renderRoundStartText(SDL_Renderer* renderer)
{
	if (m_state == LevelState::Starting) {
		m_roundStartText->render(renderer);
	}
}

glm::ivec2 LevelScene::getWindowSize()
{
	glm::ivec2 window_size = glm::ivec2(0);
	SDL_GetWindowSize(getGameRef().getWindow(), &window_size.x, &window_size.y);

	return window_size;
}
