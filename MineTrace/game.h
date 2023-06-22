#pragma once

#include "frame.h"

struct Vec3 {
	float x;
	float y;
	float z;
};

struct IVec3 {
	int x;
	int y;
	int z;
};

struct Game {
	int32_t frameCount;
	int* texturePixels;
	char* map;
};

void start(struct Game* game);
void draw(struct Game* game, struct Frame* frame);
void end(struct Game* game);