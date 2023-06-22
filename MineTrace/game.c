#include "game.h"
#include "frame.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include "game.h"

const int textureWidth = 256;
const int textureHeight = 64;
const int blockTextureSize = 16;

const int mapSize = 32;

const float range = 30.0f;

void loadTexture(struct Game* game) {
	FILE* fileBytes = NULL;
	errno_t err = fopen_s(&fileBytes, "blocks.ppm", "rb");

	if (err != 0) {
		puts("Failed to open texture file.");
		exit(0);
	}

	// Skip header:
	int line = 0;
	while (line < 3) {
		if (fgetc(fileBytes) == '\n') ++line;
	}

	int textureSize = textureWidth * textureHeight;
	game->texturePixels = calloc(textureSize, sizeof(int));

	if (!game->texturePixels) {
		puts("Failed to allocate texture pixels.");
		exit(0);
	}

	// Load pixels:
	int pixelI = 0;
	int byteR = '\0';
	while (game->texturePixels != NULL && pixelI < textureSize) {
		int byteR = fgetc(fileBytes);
		int byteG = fgetc(fileBytes);
		int byteB = fgetc(fileBytes);

		game->texturePixels[pixelI] = 0xff000000 | ((byteR << 16) | (byteG << 8) | byteB);

		++pixelI;
	}
}

void generateMap(struct Game* game) {
	int mapVoxelCount = mapSize * mapSize * mapSize;
	game->map = malloc((size_t)mapVoxelCount);

	if (!game->map) {
		puts("Failed to allocate map.");
		exit(0);
	}

	for (int i = 0; i < mapVoxelCount; ++i) {
		game->map[i] = rand() % 100 > 90 ? 1 : 0;
	}
}

bool hitMap(struct Game* game, struct IVec3 position) {
	struct IVec3 wrappedPosition = {
		position.x & (mapSize - 1),
		position.y & (mapSize - 1),
		position.z & (mapSize - 1),
	};

	return game->map[wrappedPosition.x + wrappedPosition.y * mapSize + wrappedPosition.z * mapSize * mapSize] == 1;
}

int sign(float x) {
	if (x > 0) return 1;
	if (x < 0) return -1;
	return 0;
}

int Raycast(struct Game* game, struct Vec3 start, struct Vec3 direction) {
	// Add a small bias to prevent landing perfectly on block boundaries,
	// otherwise there will be visual glitches in that case.
	start.x += 1e-4f;
	start.y += 1e-4f;
	start.z += 1e-4f;

	// DDA Voxel traversal to find the first voxel hit by the ray:
	struct IVec3 tileDir = {
		sign(direction.x),
		sign(direction.y),
		sign(direction.z),
	};
	struct Vec3 rayStep = {
		fabsf(1.0f / direction.x),
		fabsf(1.0f / direction.y),
		fabsf(1.0f / direction.z),
	};
	struct Vec3 initialStep = { 0 };

	if (direction.x > 0.0) {
		initialStep.x = (ceilf(start.x) - start.x) * rayStep.x;
	} else {
		initialStep.x = (start.x - floorf(start.x)) * rayStep.x;
	}

	if (direction.y > 0.0) {
		initialStep.y = (ceilf(start.y) - start.y) * rayStep.y;
	} else {
		initialStep.y = (start.y - floorf(start.y)) * rayStep.y;
	}

	if (direction.z > 0.0) {
		initialStep.z = (ceilf(start.z) - start.z) * rayStep.z;
	} else {
		initialStep.z = (start.z - floorf(start.z)) * rayStep.z;
	}

	struct Vec3 distToNext = initialStep;
	struct IVec3 block = {
		(int)floorf(start.x),
		(int)floorf(start.y),
		(int)floorf(start.z),
	};
	float lastDistToNext = 0.0f;

	bool hitBlock = hitMap(game, block);
	int lastMove = 0;
	while (!hitBlock && lastDistToNext < range) {
		if (distToNext.x < distToNext.y && distToNext.x < distToNext.z) {
			lastDistToNext = distToNext.x;
			distToNext.x += rayStep.x;
			block.x += tileDir.x;
			lastMove = 0;
		} else if (distToNext.y < distToNext.z) {
			lastDistToNext = distToNext.y;
			distToNext.y += rayStep.y;
			block.y += tileDir.y;
			lastMove = 1;
		} else {
			lastDistToNext = distToNext.z;
			distToNext.z += rayStep.z;
			block.z += tileDir.z;
			lastMove = 2;
		}

		hitBlock = hitMap(game, block);
	}

	if (!hitBlock) return 0;

	float distanceTravelled = lastDistToNext;
	struct Vec3 position = {
		start.x + distanceTravelled * direction.x,
		start.y + distanceTravelled * direction.y,
		start.z + distanceTravelled * direction.z,
	};
	struct Vec3 absPosition = {
		fabsf(position.x),
		fabsf(position.y),
		fabsf(position.z),
	};
	int color = 0xffff0000;

	// Find texture uv, multiply by texture size then
	// wrap to stay within the texture's bounds.
	// "a ^ (x-1)" is the same as "a % x".
	if (lastMove == 1) {
		int u = (int)(absPosition.x * blockTextureSize) % blockTextureSize;
		int v = (int)(absPosition.z * blockTextureSize) % blockTextureSize;

		color = game->texturePixels[u + v * textureWidth];
	} else {
		int u = (int)((absPosition.x + absPosition.z) * blockTextureSize) % blockTextureSize;
		int v = ((int)(absPosition.y * blockTextureSize) % blockTextureSize);

		color = game->texturePixels[u + v * textureWidth];
	}

	float fog = fmaxf(0.0f, 1.0f - (distanceTravelled / (float)range));
	int r = (int)(((color & 0x00ff0000) >> 16) * fog);
	int g = (int)(((color & 0x0000ff00) >> 8) * fog);
	int b = (int)((color & 0x000000ff) * fog);
	return 0xff000000 | (r << 16) | (g << 8) | b;
}

// Quake fast inverse square root.
float inverseSqrt(float x)
{
	float x2 = x * 0.5F;
	float y = x;
	long i = *(long*)&y;
	i = 0x5f3759df - (i >> 1);
	y = *(float*)&i;
	y = y * (1.5f - (x2 * y * y));

	return y;
}

void start(struct Game* game) {
	loadTexture(game);
	generateMap(game);

	game->frameCount = 99;
}

void end(struct Game* game) {
	free(game->texturePixels);
	free(game->map);
}

void draw(struct Game* game, struct Frame* frame) {
	float aspectRatio = frame->width / (float)frame->height;
	float height = 2.0f;
	float width = aspectRatio * height;
	float fov = 45.0f;
	struct Vec3 horizontal = { width, 0.0f, 0.0f };
	struct Vec3 vertical = { 0.0f, height, 0.0f };
	float focalLength = 1.0f;

	// A time value used for animation, has no relation to any real-life time units.
	float time = ++game->frameCount * 0.05f;

	struct Vec3 start = {
		50.5f,
		50.5f,
		50.5f + time,
	};
	struct Vec3 lowerLeftCorner = {
		start.x - horizontal.x * 0.5f - vertical.x * 0.5f,
		start.y - horizontal.y * 0.5f - vertical.y * 0.5f,
		start.z - horizontal.z * 0.5f - vertical.z * 0.5f - focalLength,
	};

	float angle = sinf(time) * 0.25f;

	for (int y = 0; y < frame->height; ++y) {
		float v = y / (float)frame->height;
		for (int x = 0; x < frame->width; ++x) {
			float u = x / (float)frame->width;
			struct Vec3 direction = {
				lowerLeftCorner.x + u * horizontal.x + v * vertical.x - start.x,
				lowerLeftCorner.y + u * horizontal.y + v * vertical.y - start.y,
				lowerLeftCorner.z + u * horizontal.z + v * vertical.z - start.z,
			};

			direction.z *= -1.0f;
			
			// Normalize direction:
			/*
			float directionMagnitude = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
			direction.x /= directionMagnitude;
			direction.y /= directionMagnitude;
			direction.z /= directionMagnitude;
			*/
			float directionInvMagnitude = inverseSqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);

			// Multiply direction by rotation "matrix":
			struct Vec3 rotatedDirection = {
				direction.x * cosf(angle) + direction.z * sinf(angle),
				direction.y,
				direction.x * -sinf(angle) + direction.z * cosf(angle),
			};

			int color = Raycast(game, start, rotatedDirection);
			frame->pixels[x + y * frame->width] = color;
		}
	}
}