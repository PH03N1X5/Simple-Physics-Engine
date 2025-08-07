#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "raylib.h"
#include "raymath.h"

#define swap(T, a, b) do { T t = (a); (a) = (b); (b) = (t); } while(0)

typedef struct Player {
    Rectangle hitbox;
    Vector2 v;
    Color color;
} Player;


const Color BACKGROUND = {0x21, 0x21, 0x21, 0xff};
/* const Color BACKGROUND = {0x01, 0x01, 0x01, 0xff}; */

#define GAS    1<<0 
#define LIQUID 1<<1
#define SOLID  1<<2

#define IS_GAS(material)    (material.phase & GAS)
#define IS_LIQUID(material) (material.phase & LIQUID)
#define IS_SOLID(material)  (material.phase & SOLID)

typedef struct material {
    float density;

    float viscosity;

    /* float conductivity; */
    /* float floatiness; */
    /* float flamability; */

    // we need an explicit variable here to store the state of the material 
    // since we can't deduce it only based on the density unfortunately
    // 1 - GAS 
    // 2 - LIQUID
    // 4 - SOLID
    // i chose to use powers of 2 to easily test type using a mask
    char phase;

    Color color;
} material_t;

typedef enum {
    AIR,
    WATER,
    SMOKE,
    SAND,
    /* OIL, */
    /* WOOD, */
    MATERIAL_COUNT,
} material;

const material_t materials[MATERIAL_COUNT] = {
    [AIR]   = {.color = BACKGROUND, .phase = GAS,    .density = 1.2e0, .viscosity = 1 ,  },
    [WATER] = {.color = BLUE    , .phase = LIQUID, .density = 1e3,   .viscosity = 6,   },
    [SMOKE] = {.color = GRAY    , .phase = GAS,    .density = 1.1e0, .viscosity = 2,   },
    [SAND]  = {.color = YELLOW  , .phase = SOLID,  .density = 1.8e3, .viscosity = 0    },
    /* [OIL]   = {  .density = 2, .color = GRAY        }, */
    /* [WOOD]  = {  .density = 5, .color = BROWN       }, */
};

int mod(int a, int b);
float d(Vector2 a, Vector2 b);
float frand();
float nfrand();

void DrawParticle(material_t particle);

int main() {
    Camera2D camera_screen = {.zoom = 1.0f};
    const Rectangle screen = {
	.width = 1280,
	.height = 720,
	/* .width = 1366, */
	/* .height = 768, */
    };

    Camera2D camera_game= {.zoom = 1.0f};
    const Rectangle game = {
	.width = 160,
	.height = 90,
    };


    const float virtual_ratio = (float)screen.width/game.width;

    typedef enum {
	NORTH = 0,
	SOUTH,
	WEST,
	EAST,
    } DIRECTION;

    Rectangle wall[4] = {
	{0, +game.height, game.width, game.height}, // N old S
	{0, -game.height, game.width, game.height}, // S old N
	{-game.width, 0,  game.width, game.height}, // W
	{+game.width, 0,  game.width, game.height}, // E
    };

    material_t environment[(int)game.width][(int)game.height];
    material_t environment_buffer[(int)game.width][(int)game.height];
    for (int x = 0; x < game.width; x++)
	for (int y = 0; y < game.height; y++)
	    environment[x][y] = materials[AIR];

    const float g = 9.81; // gravitational constant

    int tick = 0; //timer

    InitWindow(screen.width, screen.height, "Noita clone");
    RenderTexture2D target = LoadRenderTexture(screen.width, screen.height);
    /* SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR); */
    /**/
    Shader s_dither = LoadShader(0, "../shaders/dither.fs");
    int s_dither_width  = GetShaderLocation(s_dither, "width");
    int s_dither_height = GetShaderLocation(s_dither, "heigth");

    Shader s_effect = LoadShader(0, "../shaders/effect.fs");
    int s_effect_width  = GetShaderLocation(s_effect, "width");
    int s_effect_height = GetShaderLocation(s_effect, "height");
    SetShaderValue(s_effect, s_effect_width, &screen.width,  SHADER_UNIFORM_INT);
    SetShaderValue(s_effect, s_effect_height, &screen.height, SHADER_UNIFORM_INT);

    /* Shader s_upscaling = LoadShader(0, "../shaders/upscaling.fs"); */
    /* int s_upscaling_ratio = GetShaderLocation(s_effect, "width"); */
    /* SetShaderValue(s_effect, s_upscaling_ratio, &(Vector2){target.texture.width, target.texture.height}, SHADER_UNIFORM_VEC2); */

    SetTargetFPS(60);
    while (!WindowShouldClose()) {
	Vector2 mouse_pos = GetMousePosition();

        mouse_pos.x /= virtual_ratio;
        mouse_pos.y /= virtual_ratio;

	BeginTextureMode(target);
	BeginMode2D(camera_game);

	memcpy(environment_buffer, environment, sizeof(environment));
	for (int y = game.height-1; y >= 0; y--)
	    /* for (int y = 0; y < game.height; y++) */
	    for (int x = 0; x < game.width; x++)
	    {
		Vector2 d = {0};
		material_t *current = &environment_buffer[x][y];
		material_t *target;

		switch (current->phase)
		{
		    int t;
		    case GAS:
			// N
			d.x = x;
			d.y = y-1;
			target = &environment_buffer[(int)d.x][(int)d.y];
			if (CheckCollisionPointRec(d, game) && current->density < target->density)
			    swap(material_t, *target, *current);

			t = current->viscosity*nfrand();
			// NW & NE
			d.x = x + t;
			target = &environment_buffer[(int)d.x][(int)d.y];
			if (CheckCollisionPointRec(d, game) && current->density < target->density)
			    swap(material_t, *target, *current);

			d.x = x - t;
			target = &environment_buffer[(int)d.x][(int)d.y];
			if (CheckCollisionPointRec(d, game) && current->density < target->density)
			    swap(material_t, *target, *current);

			// W & E
			d.y = y;
			d.x = x - t;
			target = &environment_buffer[(int)d.x][(int)d.y];
			if (CheckCollisionPointRec(d, game) && current->density < target->density)
			    swap(material_t, *target, *current);

			d.x = x - t;
			target = &environment_buffer[(int)d.x][(int)d.y];
			if (CheckCollisionPointRec(d, game) && current->density < target->density)
			    swap(material_t, *target, *current);

			break;
		    case LIQUID:
			// S
			d.x = x;
			d.y = y+1;
			target = &environment_buffer[(int)d.x][(int)d.y];
			if (CheckCollisionPointRec(d, game) && current->density > target->density)
			    swap(material_t, *target, *current);

			t = (rand()&1)?-1:1;
			// SW & SE
			d.x = x + t;
			target = &environment_buffer[(int)d.x][(int)d.y];
			if (CheckCollisionPointRec(d, game) && current->density > target->density)
			    swap(material_t, *target, *current);

			d.x = x - t;
			target = &environment_buffer[(int)d.x][(int)d.y];
			if (CheckCollisionPointRec(d, game) && current->density > target->density)
			    swap(material_t, *target, *current);

			t = current->viscosity*nfrand();
			// W & E
			d.y = y;
			d.x = x - t;
			target = &environment_buffer[(int)d.x][(int)d.y];
			if (CheckCollisionPointRec(d, game) && current->density > target->density)
			    swap(material_t, *target, *current);

			d.x = x - t;
			target = &environment_buffer[(int)d.x][(int)d.y];
			if (CheckCollisionPointRec(d, game) && current->density > target->density)
			    swap(material_t, *target, *current);

			break;
		    case SOLID:
			d.x = x;
			d.y = y+1;
			target = &environment_buffer[(int)d.x][(int)d.y];
			if (CheckCollisionPointRec(d, game) && current->density > target->density)
			    swap(material_t, *target, *current);

			t = (rand()&1)?-1:1;
			d.x = x + t;
			target = &environment_buffer[(int)d.x][(int)d.y];
			if (CheckCollisionPointRec(d, game) && current->density > target->density)
			    swap(material_t, *target, *current);

			d.x = x - t;
			target = &environment_buffer[(int)d.x][(int)d.y];
			if (CheckCollisionPointRec(d, game) && current->density > target->density)
			    swap(material_t, *target, *current);
			break;
		}
	    }
	memcpy(environment, environment_buffer, sizeof(environment));

	EndMode2D();
	EndTextureMode();



	ClearBackground(BLACK);
	BeginTextureMode(target);
	BeginMode2D(camera_game);
	DrawRectangleRec(screen, BACKGROUND);  
	EndMode2D();
	EndTextureMode();

	/* BeginTextureMode(target); */
	/* EndTextureMode(); */

	if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
	{

	    environment[(int)mouse_pos.x]  [(int)mouse_pos.y] = materials[rand()%MATERIAL_COUNT];
	    environment[(int)mouse_pos.x-1][(int)mouse_pos.y] = materials[rand()%MATERIAL_COUNT];
	    environment[(int)mouse_pos.x+1][(int)mouse_pos.y] = materials[rand()%MATERIAL_COUNT];
	    environment[(int)mouse_pos.x]  [(int)mouse_pos.y+1] = materials[rand()%MATERIAL_COUNT];
	    environment[(int)mouse_pos.x-1][(int)mouse_pos.y+1] = materials[rand()%MATERIAL_COUNT];
	    environment[(int)mouse_pos.x+1][(int)mouse_pos.y+1] = materials[rand()%MATERIAL_COUNT];
	    environment[(int)mouse_pos.x]  [(int)mouse_pos.y-1] = materials[rand()%MATERIAL_COUNT];
	    environment[(int)mouse_pos.x-1][(int)mouse_pos.y-1] = materials[rand()%MATERIAL_COUNT];
	    environment[(int)mouse_pos.x+1][(int)mouse_pos.y-1] = materials[rand()%MATERIAL_COUNT];
	}
	/* environment[(int)mouse_pos.x][(int)mouse_pos.y] = materials[MATERIAL_SAND]; */

	BeginTextureMode(target);
	BeginMode2D(camera_game);
	for (int x = 0; x < game.width; x++)
	    for (int y = 0; y < game.height; y++)
		DrawRectangle(x*virtual_ratio, y*virtual_ratio, virtual_ratio, virtual_ratio, environment[x][y].color);
	EndMode2D();
	EndTextureMode();

	/* BeginShaderMode(s_effect); */
	/* BeginShaderMode(shader_upscaling); */
	/* BeginShaderMode(s_dither); */

	BeginDrawing();

	BeginMode2D(camera_screen);
	DrawTexturePro(
	    target.texture,
	    /* game, */
	    /* screen, */
	    (Rectangle) {0, 0, target.texture.width, -target.texture.height},
	    (Rectangle) {0, 0, screen.width, screen.height},
	    (Vector2) {0, 0}, // origin
	    0.0,
	    WHITE);
	EndMode2D();
	/* EndShaderMode(); */

	DrawFPS(0, 0);

	tick++;

	EndDrawing();
    }
    CloseWindow();
    return 0;
}

float frand()
{
    return (float)rand()/RAND_MAX;
}

float nfrand()
{
    return (float)2*rand()/RAND_MAX-1;
}

int mod(int a, int b)

{
    return (a%b + b)%b;
}

float d(Vector2 a, Vector2 b)
{
    return sqrt((a.x-b.x)*(a.x-b.x) + (a.y-b.y)*(a.y-b.y));
}
