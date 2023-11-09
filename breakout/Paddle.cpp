#include "Paddle.h"

#include <stdexcept>
#include <iostream>

Paddle::Paddle(TextureAsset& texture, const float speed) : MovableTexture2D(texture)
{
	setSpeed(speed);
	setPosition(glm::vec2(400, 780));
	setSize(glm::vec2(60, 10));
}

void Paddle::render(SDL_Renderer* renderer)
{
	// render on position
	SDL_FRect rect;

	rect = { getPosition().x - getSize().x / 2, getPosition().y - getSize().y / 2, getSize().x, getSize().y};

	SDL_RenderCopyF(renderer, getTextureAsset().getTexture(), NULL, &rect);
}

void Paddle::moveRight(float delta)
{
	// set direction for moving
	setDirection(glm::vec2(1, 0));

	// pre-calculate delta and check if colliding with screen
	// TODO: maybe update to be resolution dependant - 800 used as screen width constant
	auto move_delta = getSpeed() * delta;
	if (!(getPosition().x + move_delta > 800 - getSize().x / 2)) {
		updatePosition(delta);
	}
}

void Paddle::moveLeft(float delta)
{
	setDirection(glm::vec2(-1, 0));

	auto move_delta = getSpeed() * delta;
	if (!(getPosition().x - move_delta < getSize().x / 2)) {
		updatePosition(delta);
	}
}
