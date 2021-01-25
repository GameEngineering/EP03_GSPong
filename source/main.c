#define GS_IMPL
#include <gs/gs.h>

#define GS_IMMEDIATE_DRAW_IMPL
#include <gs/util/gs_idraw.h>

#define GS_ASSET_IMPL
#include <gs/util/gs_asset.h>

/*======================
// Constants and Defines
======================*/
#define GAME_FIELDX		10.f
#define GAME_FIELDY 	10.f
#define PADDLE_WIDTH 	20.f
#define PADDLE_HEIGHT 	80.f
#define PADDLE_SPEED 	10.f
#define BALL_SPEED 		5.f
#define BALL_WIDTH 		10.f
#define BALL_HEIGHT 	10.f

#define window_size(...) 	gs_platform_window_sizev(gs_platform_main_window())
#define paddle_dims(...) 	gs_v2(PADDLE_WIDTH, PADDLE_HEIGHT)
#define ball_dims(...) 		gs_v2(BALL_WIDTH, BALL_HEIGHT)

/*=========
// Paddle
==========*/

typedef enum paddle_side {
	PADDLE_LEFT,
	PADDLE_RIGHT,
	PADDLE_COUNT
} paddle_side;

typedef struct paddle_t {
	gs_vec2 position;
} paddle_t;

/*======
// Ball
======*/

typedef struct ball_t {
	gs_vec2 position;
	gs_vec2 velocity;
} ball_t;

/*===========
// Game Data
===========*/

typedef struct game_data_t
{
	gs_command_buffer_t gcb;
	gs_immediate_draw_t gsi;
	gs_asset_manager_t  gsa;
	paddle_t paddles[PADDLE_COUNT];
	ball_t ball;
	uint32_t score[PADDLE_COUNT];
	gs_asset_t font;
	gs_asset_t ball_hit_audio;
	gs_asset_t score_audio;
} game_data_t;

// Forward declares
void draw_game(game_data_t* gd);
void init_ball(game_data_t* gd);
void update_paddles(game_data_t* gd);
void update_ball(game_data_t* gd);
void play_sound(gs_asset_manager_t* am, gs_asset_t src, float volume);

void app_init()
{
	// Grab user data pointer from framework
	game_data_t* gd = gs_engine_user_data(game_data_t);

	// Initialize utilities
	gd->gcb = gs_command_buffer_new();
	gd->gsi = gs_immediate_draw_new();
	gd->gsa = gs_asset_manager_new();

	// Initialize paddles
	gs_vec2 pd = paddle_dims();
	gs_vec2 ws = window_size();
	gd->paddles[PADDLE_LEFT].position = gs_v2(pd.x * 2.f, ws.y * 0.5f);
	gd->paddles[PADDLE_RIGHT].position = gs_v2(ws.x - 3.f * pd.x, ws.y * 0.5f);

	// Initialize ball
	init_ball(gd);

	// Load font asset
	gd->font = gs_assets_load_from_file(&gd->gsa, gs_asset_font_t, 
		"./assets/bit9x9.ttf", 48);

	// Load audio assets
	gd->ball_hit_audio = gs_assets_load_from_file(&gd->gsa, gs_asset_audio_t, 
		"./assets/ball_hit.wav");	
	gd->score_audio = gs_assets_load_from_file(&gd->gsa, gs_asset_audio_t, 
		"./assets/score.wav");
}

void app_update()
{
	// Check for input to close app
	if (gs_platform_key_pressed(GS_KEYCODE_ESC)) gs_engine_quit();

	// Get game data
	game_data_t* gd = gs_engine_user_data(game_data_t);

	// Updates
	update_paddles(gd);
	update_ball(gd);

	// Draw game
	draw_game(gd);
}

void app_shutdown()
{
}

gs_aabb_t paddle_aabb(paddle_t paddle)
{
	gs_aabb_t aabb = {0};
	aabb.min = paddle.position;
	aabb.max = gs_vec2_add(aabb.min, paddle_dims());
	return aabb;
}

gs_aabb_t ball_aabb(ball_t ball)
{
	gs_aabb_t aabb = {0};
	aabb.min = ball.position;
	aabb.max = gs_vec2_add(aabb.min, ball_dims());
	return aabb;
}

void play_sound(gs_asset_manager_t* am, gs_asset_t src, float volume)
{
	gs_asset_audio_t* ap = gs_assets_getp(am, gs_asset_audio_t, src);
	gs_audio_play_source(ap->hndl, volume);
}

void init_ball(game_data_t* gd)
{
	gs_vec2 ws = window_size();
	gd->ball.position = gs_v2((ws.x - BALL_WIDTH) * 0.5f, (ws.y - BALL_HEIGHT) * 0.5f);
	gd->ball.velocity = gs_v2(-1.f, -1.f);
}

void update_ball(game_data_t* gd)
{
	gs_vec2 ws = window_size();

	// Move ball based on its previous velocity
	gd->ball.position.x += gd->ball.velocity.x * BALL_SPEED;
	gd->ball.position.y += gd->ball.velocity.y * BALL_SPEED;

	bool need_pos_reset = false;
	bool need_ball_reset = false;

	// Check against bottom and top walls
	if (
		(gd->ball.position.y > ws.y - GAME_FIELDY - BALL_HEIGHT) ||
		(gd->ball.position.y < GAME_FIELDY)
	)
	{
		gd->ball.velocity.y *= -1.f;
		need_pos_reset = true;
	}

	// Check right wall
	if (gd->ball.position.x > ws.x - GAME_FIELDX - BALL_WIDTH) {
		gd->score[PADDLE_LEFT]++;
		need_ball_reset = true;
	}

	// Check left wall
	if (gd->ball.position.x < GAME_FIELDX) {
		gd->score[PADDLE_RIGHT]++;
		need_ball_reset = true;
	}

	// Check for collision against paddles
	gs_aabb_t laabb = paddle_aabb(gd->paddles[PADDLE_LEFT]);
	gs_aabb_t raabb = paddle_aabb(gd->paddles[PADDLE_RIGHT]);
	gs_aabb_t baabb = ball_aabb(gd->ball);
	if (
		gs_aabb_vs_aabb(&laabb, &baabb) ||
		gs_aabb_vs_aabb(&raabb, &baabb)
	)
	{
		gd->ball.velocity.x *= -1.f;
		need_pos_reset = true;
	}

	// Reset position
	if (need_pos_reset) {
		gd->ball.position.y += gd->ball.velocity.y * BALL_SPEED;
		gd->ball.position.x += gd->ball.velocity.x * BALL_SPEED;
		play_sound(&gd->gsa, gd->ball_hit_audio, 0.5f);
	}

	// Reset ball if scored
	if (need_ball_reset) {
		play_sound(&gd->gsa, gd->score_audio, 0.5f);
		init_ball(gd);
	}
}

void update_paddles(game_data_t* gd)
{
	gs_vec2 ws = window_size();
	float* y = NULL;
	float min = GAME_FIELDY;
	float max = ws.y - PADDLE_HEIGHT - min;

	// Left paddle movement
	y = &gd->paddles[PADDLE_LEFT].position.y;
	if (gs_platform_key_down(GS_KEYCODE_W)) {
		*y = gs_clamp(*y - PADDLE_SPEED, min, max);
	}
	if (gs_platform_key_down(GS_KEYCODE_S)) {
		*y = gs_clamp(*y + PADDLE_SPEED, min, max);
	}

	// Right paddle movement
	y = &gd->paddles[PADDLE_RIGHT].position.y;
	if (gs_platform_key_down(GS_KEYCODE_UP)) {
		*y = gs_clamp(*y - PADDLE_SPEED, min, max);
	}
	if (gs_platform_key_down(GS_KEYCODE_DOWN)) {
		*y = gs_clamp(*y + PADDLE_SPEED, min, max);
	}
}

void draw_game(game_data_t* gd)
{
	// Cache pointers
	gs_command_buffer_t* gcb = &gd->gcb;
	gs_immediate_draw_t* gsi = &gd->gsi;
	gs_asset_manager_t*  gsa = &gd->gsa;
	gs_asset_font_t*     fp  = gs_assets_getp(gsa, gs_asset_font_t, gd->font);

	// Window size
	gs_vec2 ws = window_size();

	// 2D Camera (for screen coordinates)
	gsi_camera2D(gsi);

	// Game Field
	gsi_rect(gsi, GAME_FIELDX, GAME_FIELDY, 
		ws.x - GAME_FIELDX, ws.y - GAME_FIELDY, 
		255, 255, 255, 255, GS_GRAPHICS_PRIMITIVE_LINES);

	// Game field dividing line
	const float y_offset = 5.f;
	gs_vec2 div_dim = gs_v2(5.f, 10.f);
	int32_t num_steps = (ws.y - GAME_FIELDY * 2.f) / (div_dim.y + y_offset);
	for (uint32_t i = 0; i <= num_steps; ++i)
	{
		gs_vec2 a = gs_v2((ws.x - div_dim.x) * 0.5f, 
			GAME_FIELDY + i * (div_dim.y + y_offset));
		gs_vec2 b = gs_v2(a.x + div_dim.x, a.y + div_dim.y);
		gsi_rectv(gsi, a, b, gs_color_alpha(GS_COLOR_WHITE, 100), 
			GS_GRAPHICS_PRIMITIVE_TRIANGLES);
	}

	// Paddles
	for (uint32_t i = 0; i < PADDLE_COUNT; ++i)
	{
		gs_vec2 a = gd->paddles[i].position;
		gs_vec2 b = gs_v2(a.x + PADDLE_WIDTH, a.y + PADDLE_HEIGHT);
		gsi_rectv(gsi, a, b, GS_COLOR_WHITE, GS_GRAPHICS_PRIMITIVE_TRIANGLES);
	}

	// Ball
	{
		gs_vec2 a = gd->ball.position;
		gs_vec2 b = gs_v2(a.x + BALL_WIDTH, a.y + BALL_HEIGHT);
		gsi_rectv(gsi, a, b, GS_COLOR_WHITE, GS_GRAPHICS_PRIMITIVE_TRIANGLES);
	}

	// Title
	gsi_text(gsi, ws.x * 0.5f - 101.f, 100.f, "PONG", fp, 
		false, 255, 255, 255, 255);

	// Scores
	for (uint32_t i = 0; i < PADDLE_COUNT; ++i)
	{
		gs_snprintfc(score_buf, 256, "%zu", gd->score[i]);
		gsi_text(gsi, ws.x * 0.5f - 75.f + 107.f * i, 150.f,
			score_buf, fp, false, 255, 255, 255, 255);
	}

	// Final immediate draw submit and render pass
	gsi_render_pass_submit(gsi, gcb, gs_color(20, 20, 20, 255));
	// Final graphics backend command buffer submit
	gs_graphics_submit_command_buffer(gcb);
}

// Globals
game_data_t gdata = {0};

gs_app_desc_t gs_main(int32_t argc, char** argv)
{
	return (gs_app_desc_t){
		.window_width = 800,
		.window_height = 600,
		.window_title = "Pong",
		.init = app_init,
		.update = app_update,
		.shutdown = app_shutdown,
		.user_data = &gdata
	};
}