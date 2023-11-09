#pragma once

#include "MovableTexture2D.h"

#include <glm/glm.hpp>

class Paddle
	: public MovableTexture2D
{
public:
	Paddle(TextureAsset &texture, const float speed);

	virtual void render(SDL_Renderer* renderer) override;

	void moveRight(float delta);
	void moveLeft(float delta);
};

