#include "allegro5/allegro.h"
#include "allegro5/allegro_image.h"
#include "allegro5/allegro_primitives.h"
#include "allegro5/allegro_font.h"
#include "allegro5/allegro_ttf.h"
#include "allegro5/allegro_audio.h"
#include "allegro5/allegro_acodec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "libSocket/client.h"

#define TILEMAP_HEIGHT 1184
#define TILEMAP_WIDTH 960
#define TILE_SIZE 16

#define MENU_TEXT_HEIGHT 94
#define FPS 60
#define N_FLAGS 2
#define MAX_PLAYERS 4
#define N_BULLETS 10
#define BULLET_SPEED 10
#define PLAYER_SPEED 2
#define COS45 0.7071
#define ZOOM 2
#define BOUND 8

#define TRANSPARENCY_TIMER FPS*4.0

#define COLOR_TRANSPARENCY(PERCENT) al_map_rgba_f(PERCENT, PERCENT, PERCENT, PERCENT)
#define COLOR_BLACK al_map_rgb(0,0,0)
#define COLOR_WHITE al_map_rgb(255,255,255)
#define COLOR_MAX al_map_rgb(105,190,40)
#define COLOR_MID al_map_rgb(255,212,84)
#define COLOR_MIN al_map_rgb(244,67,54)
#define COLOR_PLAYER al_map_rgb(0,200,0)

#define N_SKINS 6
#define SKIN_H 21
#define SKIN_W 16
#define N_SKIN_FRAMES 3
#define SKIN_FRAME_SPEED 5

#define NUMBERS_W 70
#define NUMBERS_H 64

#define WALKABLE 1

enum Sponsors {CIN, GRADS, SEGMENTATION_FAULT};
enum TileMapObjects {WALL = 2, TREE, CAVE, FLAG_1, FLAG_2, BASE_1, BASE_2};
enum Teams{TEAM_1, TEAM_2};
enum Directions {UP, DOWN, LEFT, RIGHT};
enum Actions {MOVE, FIRE, CAVE_IN, DROP_FLAG};
enum Events {MOVED, FIRED, CATCHED_ENEMY_FLAG, CATCHED_TEAM_FLAG, DROPPED_ENEMY_FLAG, DROPPED_TEAM_FLAG, WON, RESPAWNED, BOOTED, DISCONNECTED};
enum Screens {CREDITS, EXIT, IP, KEYBOARD, MAIN_MENU, MANUAL, OPENING, PLAYER, PLAYER_ID, SETTINGS, SKIN, INIT_GAME, GAME, GAME_ENDS, GAME_EXIT};
enum Keys {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_W, KEY_S, KEY_A, KEY_D, KEY_Q, KEY_E, KEY_TAB, KEY_ENTER, KEY_ESCAPE};

typedef struct{
	short int x;
	short int y;
} Point;

typedef struct{
	float x;
	float y;
} FloatPoint;

typedef struct{
	float vx;
	float vy;
} Vector;

typedef struct{
	char id;
	char team;
	char live;
	char kills;
	char deaths;
	char skin;
	char name[13];
	
	bool has_team_flag;
	bool has_enemy_flag;
	
	Vector speed;
	FloatPoint mouse;
	Point pos;
} Player;

typedef struct{
	char *map;
	char rows;
	char cols;
	short int n_caves;
	short int n_trees;
} Map;

typedef struct{
	char r;
	char c;
	char type;
} TileObject;

typedef struct{
	char player_id;
	char live;
	Vector speed;
	FloatPoint pos;
} Bullet;

typedef struct{
	char src_r;
	char src_c;
	bool has_catched;
	TileObject tile;
} Flag;

typedef struct{
	short int spawn_r;
	short int spawn_c;
} Team;

typedef struct{
	Player players[MAX_PLAYERS];
	Flag flags[N_FLAGS];
	Bullet bullets[N_BULLETS*MAX_PLAYERS];
	char victorious_team;
	bool ends;
} ServerPacket;

typedef struct{
	char id;
	char name[13];
	char skin;
	Point pos;
	Point mouse;
	Vector speed;
	char event;
} ClientPacket;

typedef struct{
	char id;
	Point pos;
} BootPacket;

typedef struct{
	ALLEGRO_MUTEX *mutex;
	ServerPacket *game;
} ThreadData;

//Inicializar bitmaps
void initBitmaps();
void initSkins();
void initAmplifiedSkins();

//Destruir Bitmaps
void destroyBitmaps();

//Menu
void drawBackground();
void drawGameStart(char start_button_frame);
void drawHeader(char header);
void drawOptions(char n_options, char options[], char selected, bool confirmed);

void writeIP(ALLEGRO_EVENT event, char str[]);
void writeID(ALLEGRO_EVENT event, char str[]);
void showTextInput(char str[]);

char optionSelectedByMouse(char n_options, char options[], int pos_x, int pos_y);
char skinSelectedByMouse(int mouse_x, int mouse_y);
void showAllSkins(char skin, char skin_frame);

//TileMap
void loadTileMapMatrix();
void showTileMapMatrix();
char getTileContentByPosition(int x, int y);
void setTileContentByPosition(int x, int y, char tile_id);
char getTileContent(char r, char c);
void setTileContent(char r, char c, char tile_id);

bool isWalkable(int x, int y);
bool isPassable(int x, int y);

void catchFlags(Player *player);
void catchEnemyFlag(Player *player);
void catchTeamFlag(Player *player);
void dropFlags(Player *player);
void dropEnemyFlag(Player *player);
void dropTeamFlag(Player *player);

void caveIn(Point *pos);

void respawnPlayer(Player *player);

//Game
void drawBullets();
void drawFlags();
void drawBases();
void drawLife(Point *pos, int life, int w, int h);
void drawTrees();

void APCSkinAnimation(char id);
void playerSkinAnimation(Point *pos, FloatPoint *mouse_pos, Vector *v);
void drawPlayers(char id, Point *pos);

void drawShadows();

void drawHUD(Player *player, int w, int h);
void drawGame(Player *player, int w, int h);

//Server
void sendToServer(Player *player, char event);
void *rcvFromServer(ALLEGRO_THREAD *thr, void *arg);

ALLEGRO_COLOR color_teams[2];

ALLEGRO_BITMAP *bitmap_sponsor_cin = NULL;
ALLEGRO_BITMAP *bitmap_sponsor_grads = NULL;
ALLEGRO_BITMAP *bitmap_sponsor_segmentation = NULL;
ALLEGRO_BITMAP *bitmap_life_status_bar = NULL;
ALLEGRO_BITMAP *bitmap_map = NULL;
ALLEGRO_BITMAP *bitmap_tree = NULL;
ALLEGRO_BITMAP *bitmap_tree_shadow = NULL;
ALLEGRO_BITMAP *bitmap_flags[4] = {NULL, NULL, NULL, NULL};
ALLEGRO_BITMAP *bitmap_skins[N_SKINS] = {NULL, NULL, NULL, NULL, NULL};
ALLEGRO_BITMAP *bitmap_skins_amplified[N_SKINS] = {NULL, NULL, NULL, NULL, NULL};
ALLEGRO_BITMAP *bitmap_heart = NULL;
ALLEGRO_BITMAP *bitmap_snowball = NULL;
ALLEGRO_BITMAP *bitmap_numbers_base = NULL;
ALLEGRO_BITMAP *bitmap_numbers_blue = NULL;
ALLEGRO_BITMAP *bitmap_numbers_red = NULL;
ALLEGRO_BITMAP *bitmap_score = NULL;
ALLEGRO_BITMAP *bitmap_go = NULL;
ALLEGRO_BITMAP *bitmap_finish = NULL;
ALLEGRO_BITMAP *bitmap_bases[2] = {NULL, NULL};
ALLEGRO_BITMAP *bitmap_you_win = NULL;
ALLEGRO_BITMAP *bitmap_you_lose = NULL;

ALLEGRO_BITMAP *bitmap_background = NULL, *bitmap_game_start = NULL, *bitmap_start_button[2], *bitmap_cin_logo = NULL;
ALLEGRO_BITMAP *bitmap_left_arrow = NULL, *bitmap_right_arrow = NULL;
ALLEGRO_BITMAP *bitmap_credits_text = NULL, *bitmap_keyboard_text = NULL, *bitmap_manual_text = NULL;
ALLEGRO_BITMAP *bitmap_double_kill = NULL, *bitmap_triple_kill = NULL;
ALLEGRO_BITMAP *bitmap_options[13];
ALLEGRO_BITMAP *bitmap_headers[13];

ALLEGRO_FONT *font_upheavtt_94 = NULL;
ALLEGRO_FONT *font_upheavtt_30 = NULL;

Map *map = NULL;
short int caves_graph[10000];
TileObject *trees = NULL;
TileObject *caves = NULL;
TileObject *bases = NULL;

Team teams[N_FLAGS];

ServerPacket *game = NULL;
Flag *flags = NULL;
Bullet *bullets = NULL;
Player *players = NULL;
Player *player = NULL;

int w, h;
int skinFrame = 0;
bool my_player_fired = false;
bool previous_player_fired[MAX_PLAYERS];

char player_dir = DOWN;
char players_dirs[MAX_PLAYERS];
char player_frame = 1;
char players_frames[MAX_PLAYERS];
Point previous_position[MAX_PLAYERS];

ALLEGRO_AUDIO_STREAM *game_music = NULL;
ALLEGRO_AUDIO_STREAM *menu_music = NULL;
ALLEGRO_SAMPLE *hit_sound = NULL;
ALLEGRO_SAMPLE *throwing_sound = NULL;
ALLEGRO_SAMPLE *selected_sound = NULL;

int main(){
	game = (ServerPacket *) malloc(sizeof(ServerPacket));
	flags = game->flags;
	bullets = game->bullets;
	players = game->players;
	
	loadTileMapMatrix();
	showTileMapMatrix();
	ALLEGRO_TIMER *timer = NULL;
	ALLEGRO_EVENT_QUEUE *queue = NULL;
	ALLEGRO_DISPLAY *display = NULL;
	
	al_init();
	al_init_image_addon();
	al_init_primitives_addon();
	al_init_font_addon();
	al_init_ttf_addon();
	al_install_mouse();
	al_install_keyboard();
	al_install_audio();
	al_init_acodec_addon();
	al_reserve_samples(1);
	
	color_teams[0] = al_map_rgb(51,153,255);
	color_teams[1] = al_map_rgb(255,51,51);
	font_upheavtt_94 = al_load_font("upheavtt.ttf", 94, 0);
	font_upheavtt_30 = al_load_font("upheavtt.ttf", 30, 0);
	game_music = al_load_audio_stream("the_great_caper.ogg", 4, 1024);
	menu_music = al_load_audio_stream("play.ogg", 4, 1024);
	hit_sound = al_load_sample("hit_sound.ogg");
	throwing_sound = al_load_sample("throwing_sound.ogg");
	selected_sound = al_load_sample("selected_sound.ogg");
	al_attach_audio_stream_to_mixer(game_music, al_get_default_mixer());
	al_attach_audio_stream_to_mixer(menu_music, al_get_default_mixer());
	al_set_audio_stream_playmode(game_music, ALLEGRO_PLAYMODE_LOOP);
	al_set_audio_stream_playmode(menu_music, ALLEGRO_PLAYMODE_LOOP);
	al_set_audio_stream_playing(game_music, false);
	al_set_audio_stream_playing(menu_music, false);
	

  	ALLEGRO_MONITOR_INFO info;
  	al_get_monitor_info(0, &info);
  	int res_x_comp = info.x2 - info.x1, res_y_comp = info.y2 - info.y1;
	w = res_x_comp;
	h = res_y_comp;

  	al_set_new_display_flags(ALLEGRO_FULLSCREEN_WINDOW);
	display = al_create_display(w, h);
  	//display = al_create_display(800, 640);
  
	al_set_window_title(display, "BUILD ZERO");
	al_clear_to_color(COLOR_BLACK);

	initBitmaps();
	initSkins();
	initAmplifiedSkins();
	al_set_target_bitmap(al_get_backbuffer(display));

	queue = al_create_event_queue();
	timer = al_create_timer(1.0/FPS);
	
	al_register_event_source(queue, al_get_keyboard_event_source());
	al_register_event_source(queue, al_get_mouse_event_source());
	al_register_event_source(queue, al_get_display_event_source(display));
	al_register_event_source(queue, al_get_timer_event_source(timer));


	BootPacket *boot = (BootPacket *) malloc(sizeof(BootPacket));
	char ServerIP[16];
	strcpy(ServerIP, "127.0.0.1");
	Point pos = {0, 0};
	Vector v = {0, 0};
	FloatPoint mouse_pos = {0, 0};
	
	ALLEGRO_EVENT event;
	register int i;
	bool pressed_keys[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	bool dir[] = {0, 0, 0, 0};
	bool actions[] = {0, 0, 0, 0};
	
	bool intro = true;
	register float opening_timer = 0, intro_color_intensity, intro_color_intensity_timer = 0;
	register char current_sponsor = CIN, flag_drop_timer, fire_timer, cave_in_timer, skin_timer = 0;
	register short int respawn_timer, win_timer, finish_timer = 3*FPS;
	char respawn_frame, win_frame, skin_frame = 0;
	
	int mouse_x = 0;
	int mouse_y = 0;
	
	char name[13];
	name[0] = '\0';
	char skin = 0;
	
	int n_options = 0;
	char options[] = {0, 0, 0, 0, 0, 0, 0};
	char mouse_select = 0, keyboard_select = 0, old_select = 0;
	bool confirmed = false;
	char menu_delay = 0;
	bool mouse_button_down = false, mouse_moved = false;
	char start_button_timer = 0, start_button_frame = 0;
	int x, y;

	bool redraw = true, run = true;
	bool in_base = false;
	al_start_timer(timer);

	ThreadData data;
	ALLEGRO_THREAD *thread = NULL;
	data.game = (ServerPacket *) malloc(sizeof(ServerPacket));
	data.mutex = al_create_mutex();
	
	char current_screen = OPENING, parent_menu = EXIT;
	ALLEGRO_TRANSFORM transform;
	char old_live;
	while(run){
  		w = al_get_display_width(display), h = al_get_display_height(display);
		al_wait_for_event(queue, &event);
		if(event.type == ALLEGRO_EVENT_DISPLAY_CLOSE) run = 0;
		//Key down event
		if(event.type == ALLEGRO_EVENT_KEY_DOWN){
			if(event.keyboard.keycode == ALLEGRO_KEY_TAB) pressed_keys[KEY_TAB] = 1;
			if(event.keyboard.keycode == ALLEGRO_KEY_ENTER) pressed_keys[KEY_ENTER] = 1;
			if(event.keyboard.keycode == ALLEGRO_KEY_ESCAPE) pressed_keys[KEY_ESCAPE] = 1;
			if(event.keyboard.keycode == ALLEGRO_KEY_UP) pressed_keys[KEY_UP] = 1;
			if(event.keyboard.keycode == ALLEGRO_KEY_DOWN) pressed_keys[KEY_DOWN] = 1;
			if(event.keyboard.keycode == ALLEGRO_KEY_LEFT) pressed_keys[KEY_LEFT] = 1;
			if(event.keyboard.keycode == ALLEGRO_KEY_RIGHT) pressed_keys[KEY_RIGHT] = 1;
			if(event.keyboard.keycode == ALLEGRO_KEY_W){
				dir[UP] = 1;
				pressed_keys[KEY_W] = 1;
			}
			if(event.keyboard.keycode == ALLEGRO_KEY_S){
				dir[DOWN] = 1;
				pressed_keys[KEY_S] = 1;
			}
			if(event.keyboard.keycode == ALLEGRO_KEY_A){
				dir[LEFT] = 1;
				pressed_keys[KEY_A] = 1;
			}
			if(event.keyboard.keycode == ALLEGRO_KEY_D){
				dir[RIGHT] = 1;
				pressed_keys[KEY_D] = 1;
			}
			if(event.keyboard.keycode == ALLEGRO_KEY_E) actions[CAVE_IN] = 1;
			if(event.keyboard.keycode == ALLEGRO_KEY_Q) actions[DROP_FLAG] = 1;
		}
		//Key char event
		if(event.type == ALLEGRO_EVENT_KEY_CHAR){
			if(current_screen == PLAYER) writeID(event, name);
			if(current_screen == IP) writeIP(event, ServerIP);
		}
		//Key up event
		if(event.type == ALLEGRO_EVENT_KEY_UP){
			if(event.keyboard.keycode == ALLEGRO_KEY_TAB) pressed_keys[KEY_TAB] = 0;
			if(event.keyboard.keycode == ALLEGRO_KEY_ENTER) pressed_keys[KEY_ENTER] = 0;
			if(event.keyboard.keycode == ALLEGRO_KEY_ESCAPE) pressed_keys[KEY_ESCAPE] = 0;
			if(event.keyboard.keycode == ALLEGRO_KEY_UP) pressed_keys[KEY_UP] = 0;
			if(event.keyboard.keycode == ALLEGRO_KEY_DOWN) pressed_keys[KEY_DOWN] = 0;
			if(event.keyboard.keycode == ALLEGRO_KEY_LEFT) pressed_keys[KEY_LEFT] = 0;
			if(event.keyboard.keycode == ALLEGRO_KEY_RIGHT) pressed_keys[KEY_RIGHT] = 0;
			if(event.keyboard.keycode == ALLEGRO_KEY_W){
				dir[UP] = 0;
				pressed_keys[KEY_W] = 0;
			}
			if(event.keyboard.keycode == ALLEGRO_KEY_S){
				dir[DOWN] = 0;
				pressed_keys[KEY_S] = 0;
			}
			if(event.keyboard.keycode == ALLEGRO_KEY_A){
				dir[LEFT] = 0;
				pressed_keys[KEY_A] = 0;
			}
			if(event.keyboard.keycode == ALLEGRO_KEY_D){
				dir[RIGHT] = 0;
				pressed_keys[KEY_D] = 0;
			}
			if(event.keyboard.keycode == ALLEGRO_KEY_E) actions[CAVE_IN] = 0;
			if(event.keyboard.keycode == ALLEGRO_KEY_Q) actions[DROP_FLAG] = 0;
		}
		//Mouse axes
		if(event.type == ALLEGRO_EVENT_MOUSE_AXES){
			mouse_x = event.mouse.x;
			mouse_y = event.mouse.y;
			mouse_moved = true;
		}
		//Mouse button down event
		if(event.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
			mouse_x = event.mouse.x;
			mouse_y = event.mouse.y;
			mouse_button_down = true;
			if(current_screen == GAME && player->live){
				if(event.mouse.button == 1 && !fire_timer){
					my_player_fired = true;
					player->pos = pos;
					player->mouse.x = event.mouse.x - w * 0.5;
					player->mouse.y = event.mouse.y - h * 0.5;
					mouse_pos = player->mouse;
					al_play_sample(throwing_sound, 1.0, ALLEGRO_AUDIO_PAN_NONE, 1.0, ALLEGRO_PLAYMODE_ONCE, NULL);
					sendToServer(player, FIRED);
					fire_timer = 30;
				}
			}
		}
		//Timer event
		if(event.type == ALLEGRO_EVENT_TIMER){
			if(current_screen < INIT_GAME){
				if(!menu_delay){
					al_identity_transform(&transform);
					al_scale_transform(&transform, 1, 1);
					al_use_transform(&transform);
					al_clear_to_color(COLOR_BLACK);
					//Menu FSM(Finite state machine)
					switch(current_screen) {
						case OPENING:{
							if(intro) {
								if(opening_timer < TRANSPARENCY_TIMER/3) ++intro_color_intensity_timer;
								else if(opening_timer > 2*TRANSPARENCY_TIMER/3) --intro_color_intensity_timer;
								intro_color_intensity = 3*(intro_color_intensity_timer/(TRANSPARENCY_TIMER));
								++opening_timer;
								switch(current_sponsor) {
									case CIN: {
										al_clear_to_color(COLOR_WHITE);
										al_draw_tinted_bitmap(bitmap_sponsor_cin, COLOR_TRANSPARENCY(intro_color_intensity),  w/2 - al_get_bitmap_width(bitmap_sponsor_cin)/2, h/2 - al_get_bitmap_height(bitmap_sponsor_cin)/2, 0);
										break;
									}
									case GRADS: {
										al_clear_to_color(COLOR_BLACK);
										al_draw_tinted_bitmap(bitmap_sponsor_grads, COLOR_TRANSPARENCY(intro_color_intensity),  w/2 - al_get_bitmap_width(bitmap_sponsor_grads)/2, h/2 - al_get_bitmap_height(bitmap_sponsor_grads)/2, 0);
										break;
									}
									case SEGMENTATION_FAULT: {
										al_clear_to_color(COLOR_WHITE);
										al_draw_tinted_bitmap(bitmap_sponsor_segmentation, COLOR_TRANSPARENCY(intro_color_intensity),  w/2 - al_get_bitmap_width(bitmap_sponsor_segmentation)/2, h/2 - al_get_bitmap_height(bitmap_sponsor_segmentation)/2, 0);
										break;
									}
									default: {
										intro = false;
										break;
									}
								}
								if(opening_timer >= TRANSPARENCY_TIMER) {
									opening_timer = intro_color_intensity_timer = 0;
									++current_sponsor;
								}
								if(pressed_keys[KEY_ENTER]){
									intro = false;
									menu_delay = 15;
								}
							} else {
								drawBackground();
								drawGameStart(start_button_frame);
								if(!start_button_timer){
									start_button_frame = (start_button_frame+1)%4;
									start_button_timer = 30;
								}else{
									start_button_timer--;
								}
								n_options = 0;
								if(pressed_keys[KEY_ENTER]){
									current_screen = MAIN_MENU;
									menu_delay = 15;
									keyboard_select = mouse_select = 0;
								}
								al_set_audio_stream_playing(game_music, false);
								al_set_audio_stream_playing(menu_music, true);
							}
							break;
						}
						case MAIN_MENU:{
							drawBackground();
							drawHeader(MAIN_MENU);
							n_options = 4;
							options[0] = IP;
							options[1] = SETTINGS;
							options[2] = CREDITS;
							options[3] = EXIT;
							parent_menu = EXIT;
							al_set_audio_stream_playing(game_music, false);
							al_set_audio_stream_playing(menu_music, true);
							break;
						}
						case IP:{
							drawBackground();
							drawHeader(IP);
							showTextInput(ServerIP);
							n_options = 0;
							if(pressed_keys[KEY_ENTER]){
								current_screen = INIT_GAME;
								keyboard_select = mouse_select = 0;
							}
							parent_menu = MAIN_MENU;
							break;
						}
						case SETTINGS:{
							drawBackground();
							drawHeader(SETTINGS);
							n_options = 3;
							options[0] = PLAYER;
							options[1] = KEYBOARD;
							options[2] = MANUAL;
							parent_menu = MAIN_MENU;
							break;
						}
						case CREDITS:{
							al_clear_to_color(COLOR_BLACK);
							drawHeader(CREDITS);
							x = w/2 - al_get_bitmap_width(bitmap_credits_text)/2;
							y = h/7*1.5;
							al_draw_bitmap(bitmap_credits_text, x, y, 0);
							n_options = 0;
							parent_menu = MAIN_MENU;
							if(pressed_keys[KEY_ENTER]){
								current_screen = parent_menu;
								menu_delay = 15;
								keyboard_select = mouse_select = 0;
							}
							parent_menu = MAIN_MENU;
							break;
						}
						case PLAYER:{
							drawBackground();
							drawHeader(PLAYER_ID);
							n_options = 0;
							showTextInput(name);
							if(mouse_button_down){
								skin = (skinSelectedByMouse(mouse_x, mouse_y) >= 0) ? skinSelectedByMouse(mouse_x, mouse_y) : skin;
								mouse_button_down = 0;
							}
							if(pressed_keys[KEY_LEFT]){
								skin = (skin-1) % N_SKINS;
								if(skin < 0) skin += N_SKINS;
								menu_delay = 15;
							}
							if(pressed_keys[KEY_RIGHT]){
								skin = (skin+1) % N_SKINS;
								menu_delay = 15;
							}
							showAllSkins(skin, skin_frame);
							if(!(skin_timer % 4)){
								skin_frame = ++skin_frame%3;
							}
							skin_timer = ++skin_timer%FPS;
							parent_menu = SETTINGS;
							if(pressed_keys[KEY_ENTER]){
								current_screen = parent_menu;
								menu_delay = 15;
								keyboard_select = mouse_select = 0;
							}
							break;
						}
						case MANUAL:{
							drawBackground();
							drawHeader(MANUAL);
							x = w/2 - al_get_bitmap_width(bitmap_manual_text)/2;
							y = h/7*2;
							al_draw_bitmap(bitmap_manual_text, x, y, 0);
							n_options = 0;
							parent_menu = SETTINGS;
							if(pressed_keys[KEY_ENTER]){
								current_screen = parent_menu;
								menu_delay = 15;
								keyboard_select = mouse_select = 0;
							}
							break;
						}
						case KEYBOARD:{
							drawBackground();
							drawHeader(KEYBOARD);
							x = w/2 - al_get_bitmap_width(bitmap_keyboard_text)/2;
							y = h/7*2;
							al_draw_bitmap(bitmap_keyboard_text, x, y, 0);
							n_options = 0;
							parent_menu = SETTINGS;
							if(pressed_keys[KEY_ENTER]){
								current_screen = parent_menu;
								menu_delay = 15;
								keyboard_select = mouse_select = 0;
							}
							break;
						}
						case EXIT:{
							run = false;
							al_rest(0.01);
							break;
						}
					}
					if(n_options){
						if(mouse_moved){
							mouse_select = optionSelectedByMouse(n_options, options, mouse_x, mouse_y);
							if(mouse_select >= 0){
								keyboard_select = mouse_select;
							}
							menu_delay = 15;
							mouse_moved = 0;
						}
						if(pressed_keys[KEY_UP]){
							keyboard_select = (keyboard_select-1) % n_options;
							if(keyboard_select < 0) keyboard_select += n_options;
							menu_delay = 15;
						}
						if(pressed_keys[KEY_DOWN]){
							keyboard_select = (keyboard_select+1) % n_options;
							menu_delay = 15;
						}
						if(pressed_keys[KEY_ENTER]){
							confirmed = true;
							current_screen = options[keyboard_select];
							menu_delay = 15;
						}
						if(mouse_button_down){
							if(mouse_select >= 0){
								confirmed = true;
								current_screen = options[keyboard_select];
								menu_delay = 15;
							}
							mouse_button_down = 0;
						}
						if(keyboard_select != old_select){
							al_play_sample(selected_sound, 1.0, 0.0, 1.0, ALLEGRO_PLAYMODE_ONCE, NULL);
							old_select = keyboard_select;
						}
						drawOptions(n_options, options, keyboard_select, confirmed);
					}
					if(pressed_keys[KEY_ESCAPE]){
						current_screen = parent_menu;
						menu_delay = 15;
						keyboard_select = mouse_select = 0;
					}
					confirmed = false;
					redraw = true;
				}else{
					menu_delay--;
				}
			}
			if(current_screen >= INIT_GAME){
				switch(current_screen){
					//Connect to server
					case INIT_GAME:{
						printf("Initializing server...\n");
						connectToServer(ServerIP);
						recvMsgFromServer(boot, WAIT_FOR_IT);
						recvMsgFromServer(game, WAIT_FOR_IT);
						player = players + boot->id;
						player->has_team_flag = false;
						player->has_enemy_flag = false;
						strcpy(player->name, name);
						player->skin = skin;
						sendToServer(player, BOOTED);
						recvMsgFromServer(game, WAIT_FOR_IT);
						v.vx = 0, v.vy = 0;
						pos = player->pos;
						*(data.game) = *game;
						thread = al_create_thread(rcvFromServer, &data);
						al_start_thread(thread);
						printf("Player %d from team %d initialized in (%d, %d)!\n", player->id, player->team, player->pos.x, player->pos.y);
						for(i = 0; i < MAX_PLAYERS; i++){
							previous_position[i] = players[i].pos, players[i].skin = 0, players_frames[i] = 1, players_dirs[i] = DOWN, previous_player_fired[i] = false;
						}
						flag_drop_timer = 0, fire_timer = 0, cave_in_timer = 0, respawn_timer = 360, win_timer = 360, finish_timer = FPS*3;
						respawn_frame = 5, win_frame = 5;
						old_live = 5;
						al_set_audio_stream_playing(menu_music, false);
						al_set_audio_stream_playing(game_music, true);
						current_screen = GAME;
						break;
					}
					//Game
					case GAME:{
						n_options = 0;
						if(player->live){
							v.vx = v.vy = 0;
							if(dir[UP]){
								if(!dir[LEFT] && !dir[RIGHT]){
									v.vy -= PLAYER_SPEED;
								}else{
									if(dir[LEFT]){
										v.vx -= 1;
									}
									if(dir[RIGHT]){
										v.vx += 1;
									}
									v.vy -= 1;
								}
							}
							if(dir[DOWN]){
								if(!dir[LEFT] && !dir[RIGHT]){
									v.vy += PLAYER_SPEED;
								}else{
									if(dir[LEFT]){
										v.vx -= 1;
									}
									if(dir[RIGHT]){
										v.vx += 1;
									}
									v.vy += 1;
								}
							}
							if(!dir[UP] && !dir[DOWN]){
								if(dir[LEFT]){
									v.vx -= PLAYER_SPEED;
								}
								if(dir[RIGHT]){
									v.vx += PLAYER_SPEED;
								}
							}
							if(v.vx || v.vy){
								if(isWalkable(pos.x + v.vx, pos.y)){
									pos.x += v.vx;
								}
								if(isWalkable(pos.x, pos.y + v.vy)){
									pos.y += v.vy;
								}
							}
							player->speed = v;
							player->pos = pos;
							if(!flag_drop_timer){
								catchFlags(player);
							}
							if(actions[CAVE_IN] && !cave_in_timer){
								if(getTileContentByPosition(pos.x, pos.y) == CAVE){
									caveIn(&pos);
								}
								cave_in_timer = 30;
							}
							if(actions[DROP_FLAG]){
								dropFlags(player);
								flag_drop_timer = 60;
							}
							if(skinFrame % 3 == 0){
								sendToServer(player, MOVED);
							}
							if(player->live < old_live){
								al_play_sample(hit_sound, 1.0, ALLEGRO_AUDIO_PAN_NONE, 1.0, ALLEGRO_PLAYMODE_ONCE, NULL);
								old_live = player->live;
							}
							if(player->has_team_flag && player->has_enemy_flag && getTileContentByPosition(pos.x, pos.y) == BASE_1+player->team){
								in_base = true;
								if(win_timer % FPS){
									win_frame = win_timer / FPS;
								}
								if(win_timer){
									win_timer--;
								}
								if(!win_frame){
									sendToServer(player, WON);
								}
							}else{
								in_base = false;
								win_frame = 5;
								win_timer = 360;
							}
							if(fire_timer) fire_timer--;
							if(cave_in_timer) cave_in_timer--;
							if(flag_drop_timer) flag_drop_timer--;
						}else{
							if(respawn_frame == 5){
								if(player->has_team_flag){
									dropTeamFlag(player);
								}
								if(player->has_enemy_flag){
									dropEnemyFlag(player);
								}
							}
							if(!respawn_timer){
								sendToServer(player, RESPAWNED);
								respawnPlayer(player);
								pos = player->pos;
								respawn_timer = 360;
								respawn_frame = 5;
								old_live = 5;
							}else{
								if(respawn_timer % FPS){
									respawn_frame = respawn_timer / FPS;
								}
								respawn_timer--;
							}
						}
						al_lock_mutex(data.mutex);
						*(game) = *(data.game);
						al_unlock_mutex(data.mutex);
						player->pos = pos;
						player->mouse = mouse_pos;
						player->speed = v;
						if(pressed_keys[KEY_ESCAPE]){
							current_screen = GAME_EXIT;
							menu_delay = 15;
						}
						if(game->ends){
							al_destroy_thread(thread);
							sendToServer(player, DISCONNECTED);
							current_screen = GAME_ENDS;
						}
						break;
					}
					//Game end
					case GAME_ENDS:{
						if(pressed_keys[KEY_ESCAPE]){
							current_screen = MAIN_MENU;
							menu_delay = 15;
							keyboard_select = mouse_select = 0;
							mouse_button_down = 0;
						}
						break;
					}
					//Game exit
					case GAME_EXIT:{
						al_destroy_thread(thread);
						sendToServer(player, DISCONNECTED);
						current_screen = MAIN_MENU;
						menu_delay = 15;
						keyboard_select = mouse_select = 0;
						mouse_button_down = 0;
						break;
					}
				}
				redraw = true;
			}
		}
		if (redraw && al_is_event_queue_empty(queue)) {
			redraw = false;
			int ref_y = (-h*0.5+50)/ZOOM+pos.y;
			switch(current_screen) {
				case GAME:{
					al_identity_transform(&transform);
					al_translate_transform(&transform, -player->pos.x, -player->pos.y);
					al_scale_transform(&transform, ZOOM, ZOOM);
					al_translate_transform(&transform, w*0.5, h*0.5);
					al_use_transform(&transform);
					al_clear_to_color(COLOR_BLACK);
					drawGame(player, w, h);
					drawHUD(player, w, h);
					if(!player->live){
						if(respawn_frame){
							if(player->id < MAX_PLAYERS/2) al_draw_bitmap_region(bitmap_numbers_blue, NUMBERS_W*(respawn_frame-1), 0, NUMBERS_W, NUMBERS_H, pos.x - NUMBERS_W/2, ref_y+10, 0);
							else al_draw_bitmap_region(bitmap_numbers_red, NUMBERS_W*(respawn_frame-1), 0, NUMBERS_W, NUMBERS_H, pos.x - NUMBERS_W/2, ref_y+10, 0);
						}else{
							al_draw_bitmap(bitmap_go, pos.x-al_get_bitmap_width(bitmap_go)/2, ref_y+10, 0);
						}
					}
					if(in_base){
						al_draw_bitmap_region(bitmap_numbers_base, NUMBERS_W*(win_frame-1), 0, NUMBERS_W, NUMBERS_H, pos.x - NUMBERS_W/2, ref_y+10, 0);
					}
					skinFrame = ++skinFrame%FPS;
					break;
				}
				case GAME_ENDS:{
					al_identity_transform(&transform);
					al_translate_transform(&transform, -player->pos.x, -player->pos.y);
					al_scale_transform(&transform, ZOOM, ZOOM);
					al_translate_transform(&transform, w*0.5, h*0.5);
					al_use_transform(&transform);
					al_clear_to_color(COLOR_BLACK);
					drawGame(player, w, h);
					if(finish_timer) {
						al_draw_bitmap(bitmap_finish, pos.x-al_get_bitmap_width(bitmap_finish)/2, pos.y-al_get_bitmap_height(bitmap_finish)/2, 0);
						--finish_timer;
					} else {
						if(player->team == game->victorious_team){
							al_draw_bitmap(bitmap_you_win, pos.x-al_get_bitmap_width(bitmap_you_win)/2, ref_y+10, 0);
						}else{
							al_draw_bitmap(bitmap_you_lose, pos.x-al_get_bitmap_width(bitmap_you_lose)/2, ref_y+10, 0);
						}
					}
					break;
				}
			}
			al_flip_display();
		}
	}
	destroyBitmaps();
    al_destroy_font(font_upheavtt_94);
	al_destroy_font(font_upheavtt_30);
	al_destroy_mutex(data.mutex);
	al_destroy_event_queue(queue);
	al_destroy_timer(timer);
	al_destroy_display(display);
}
void initBitmaps() {
	register int i;
	bitmap_sponsor_cin = al_load_bitmap("bitmaps/logos/sponsor_cin_opening.png");
	bitmap_sponsor_grads = al_load_bitmap("bitmaps/logos/sponsor_graphics.png");
	bitmap_sponsor_segmentation = al_load_bitmap("bitmaps/logos/sponsor_segmentation_fault.png");
	bitmap_background = al_load_bitmap("bitmaps/backgrounds/background.png");
	bitmap_game_start = al_load_bitmap("bitmaps/backgrounds/game_start.png");
	bitmap_start_button[0] = al_load_bitmap("bitmaps/texts/press_enter_white.png");
	bitmap_start_button[1] = al_load_bitmap("bitmaps/texts/press_enter_black.png");
	bitmap_cin_logo = al_load_bitmap("bitmaps/logos/cin_logo.png");
	bitmap_skins[0] = al_load_bitmap("bitmaps/skins/skin_0.png");
	bitmap_skins[1] = al_load_bitmap("bitmaps/skins/skin_1.png");
	bitmap_skins[2] = al_load_bitmap("bitmaps/skins/skin_2.png");
	bitmap_skins[3] = al_load_bitmap("bitmaps/skins/skin_3.png");
	bitmap_skins[4] = al_load_bitmap("bitmaps/skins/skin_4.png");
	for(i = 0; i < 13; i++){
		bitmap_options[i] = bitmap_headers[i] = NULL;
	}
	
	bitmap_headers[IP] = al_load_bitmap("bitmaps/texts/ip_page.png");
	bitmap_headers[KEYBOARD] = al_load_bitmap("bitmaps/texts/keyboard_page.png");
	bitmap_headers[MANUAL] = al_load_bitmap("bitmaps/texts/manual_page.png");
	bitmap_headers[MAIN_MENU] = al_load_bitmap("bitmaps/texts/menu_page.png");
	bitmap_headers[SETTINGS] = al_load_bitmap("bitmaps/texts/options_page.png");
	bitmap_headers[CREDITS] = al_load_bitmap("bitmaps/texts/credits_page.png");
	bitmap_headers[PLAYER_ID]  = al_load_bitmap("bitmaps/texts/player_id_page.png");
	bitmap_headers[PLAYER]  = al_load_bitmap("bitmaps/texts/character_page.png");

	bitmap_options[PLAYER] = al_load_bitmap("bitmaps/texts/character_all.png");
	bitmap_options[CREDITS] = al_load_bitmap("bitmaps/texts/credits_all.png");
	bitmap_options[EXIT] = al_load_bitmap("bitmaps/texts/exit_all.png");
	bitmap_options[KEYBOARD] = al_load_bitmap("bitmaps/texts/keyboard_all.png");
	bitmap_options[MANUAL] = al_load_bitmap("bitmaps/texts/manual_all.png");
	bitmap_options[SETTINGS] = al_load_bitmap("bitmaps/texts/options_all.png");
	bitmap_options[IP] = al_load_bitmap("bitmaps/texts/start_game_all.png");

	bitmap_left_arrow = al_load_bitmap("bitmaps/texts/left_arrow_all.png");
	bitmap_right_arrow = al_load_bitmap("bitmaps/texts/right_arrow_all.png");
	
	bitmap_credits_text = al_load_bitmap("bitmaps/texts/credits_text.png");
	bitmap_keyboard_text = al_load_bitmap("bitmaps/texts/keyboard_text.png");
	bitmap_manual_text = al_load_bitmap("bitmaps/texts/manual_text.png");

	bitmap_double_kill = al_load_bitmap("bitmaps/texts/double_kill.png");
	bitmap_triple_kill = al_load_bitmap("bitmaps/texts/triple_kill.png");
	
	bitmap_map = al_load_bitmap("map.png");
	bitmap_tree = al_load_bitmap("tree.png");
    bitmap_tree_shadow = al_load_bitmap("tree_shadow.png");
	bitmap_heart = al_load_bitmap("heart.png");
	bitmap_snowball = al_load_bitmap("snowball.png");
	bitmap_flags[0] = al_load_bitmap("flag_blue.png");
	bitmap_flags[1] = al_load_bitmap("flag_red.png");
	bitmap_flags[2] = al_load_bitmap("flag_captured_blue.png");
	bitmap_flags[3] = al_load_bitmap("flag_captured_red.png");
	bitmap_bases[0] = al_load_bitmap("base_blue.png");
	bitmap_bases[1] = al_load_bitmap("base_red.png");
	bitmap_you_win = al_load_bitmap("bitmaps/texts/you_win.png");
	bitmap_you_lose = al_load_bitmap("bitmaps/texts/you_lose.png");
	bitmap_numbers_base = al_load_bitmap("bitmaps/texts/numbers_base.png");
	bitmap_numbers_blue = al_load_bitmap("bitmaps/texts/numbers_blue.png");
	bitmap_numbers_red = al_load_bitmap("bitmaps/texts/numbers_red.png");
	bitmap_score = al_load_bitmap("bitmaps/texts/score.png");
	bitmap_go = al_load_bitmap("bitmaps/texts/go.png");
	bitmap_finish = al_load_bitmap("bitmaps/texts/finish.png");
	bitmap_life_status_bar = al_load_bitmap("life_status_bar.png");
}
void destroyBitmaps() {
	register int i;
	
	al_destroy_bitmap(bitmap_sponsor_cin);
	al_destroy_bitmap(bitmap_sponsor_grads);
	al_destroy_bitmap(bitmap_sponsor_segmentation);
	
	al_destroy_bitmap(bitmap_background);
    al_destroy_bitmap(bitmap_cin_logo);
	
	for(i = 0; i < 13; i++){
		if(bitmap_options[i] != NULL) al_destroy_bitmap(bitmap_options[i]);
		if(bitmap_headers[i] != NULL) al_destroy_bitmap(bitmap_headers[i]);
	}
	
    al_destroy_bitmap(bitmap_left_arrow);
    al_destroy_bitmap(bitmap_right_arrow);
    
    al_destroy_bitmap(bitmap_double_kill);
    al_destroy_bitmap(bitmap_triple_kill);
    
	al_destroy_bitmap(bitmap_map);
	al_destroy_bitmap(bitmap_tree);
    al_destroy_bitmap(bitmap_tree_shadow);
    al_destroy_bitmap(bitmap_heart);
    al_destroy_bitmap(bitmap_snowball);
    for(i = 0; i < 4; i++){
		al_destroy_bitmap(bitmap_flags[i]);
	}
	al_destroy_bitmap(bitmap_numbers_base);
	al_destroy_bitmap(bitmap_numbers_blue);
	al_destroy_bitmap(bitmap_numbers_red);
	al_destroy_bitmap(bitmap_score);
	al_destroy_bitmap(bitmap_bases[0]);
	al_destroy_bitmap(bitmap_bases[1]);
	al_destroy_bitmap(bitmap_you_win);
    al_destroy_bitmap(bitmap_you_lose);
    al_destroy_bitmap(bitmap_go);
	al_destroy_bitmap(bitmap_finish);
	al_destroy_bitmap(bitmap_life_status_bar);
    
}
void initSkins() {
	bitmap_skins[0] = al_load_bitmap("bitmaps/skins/skin_0.png");
	bitmap_skins[1] = al_load_bitmap("bitmaps/skins/skin_1.png");
	bitmap_skins[2] = al_load_bitmap("bitmaps/skins/skin_2.png");
	bitmap_skins[3] = al_load_bitmap("bitmaps/skins/skin_3.png");
	bitmap_skins[4] = al_load_bitmap("bitmaps/skins/skin_4.png");
	bitmap_skins[5] = al_load_bitmap("bitmaps/skins/skin_5.png");
}
void initAmplifiedSkins(){
	register int i;
	for(i = 0; i < N_SKINS; i++){
		bitmap_skins_amplified[i] = al_create_bitmap(3*SKIN_W*6, SKIN_H*6);
		al_set_target_bitmap(bitmap_skins_amplified[i]);
		ALLEGRO_TRANSFORM transform;
		al_identity_transform(&transform);
		al_scale_transform(&transform, 6, 6);
		al_use_transform(&transform);
		al_clear_to_color(al_map_rgba(0,0,0,0));
		al_draw_bitmap_region(bitmap_skins[i], 0, SKIN_H, 3*SKIN_W, SKIN_H, 0, 0, 0);
	}
}
void drawBackground(){
	int x = w/2 - al_get_bitmap_width(bitmap_background)/2;
	int y = h/2 - al_get_bitmap_height(bitmap_background)/2;
	al_draw_bitmap(bitmap_background, x, y, 0);
}
void drawGameStart(char start_button_frame){
	int x = w/2 - al_get_bitmap_width(bitmap_game_start)/2;
	int y = h/2 - al_get_bitmap_height(bitmap_game_start)/2;
	al_draw_bitmap(bitmap_game_start, x, y, 0);
	x = w/2 - al_get_bitmap_width(bitmap_start_button[0])/2;
	y = h/2 + al_get_bitmap_height(bitmap_start_button[0]) - 50;
	if(start_button_frame == 0) al_draw_bitmap(bitmap_start_button[0], x, y, 0);
	if(start_button_frame == 2) al_draw_bitmap(bitmap_start_button[1], x, y, 0);
}
void drawHeader(char header){
	int x = w/2 - al_get_bitmap_width(bitmap_headers[header])/2;
	int y = h/7;
	al_draw_bitmap(bitmap_headers[header], x, y, 0);
}

void drawOptions(char n_options, char options[], char selected, bool confirmed){
	register int i;
	int y = h/7*2;
	int x, bitmap_width;
	ALLEGRO_BITMAP *bitmap = NULL;
	for(i = 0; i < n_options; i++){
		bitmap = bitmap_options[options[i]];
		bitmap_width = al_get_bitmap_width(bitmap);
		x = w/2 - bitmap_width/2;
		if(i == selected){
			if(confirmed){
				al_draw_bitmap_region(bitmap, 0, MENU_TEXT_HEIGHT*2, bitmap_width, MENU_TEXT_HEIGHT, x, y, 0);
			}else{
				al_draw_bitmap_region(bitmap, 0, MENU_TEXT_HEIGHT*1, bitmap_width, MENU_TEXT_HEIGHT, x, y, 0);
			}
		}else{
			al_draw_bitmap_region(bitmap, 0, MENU_TEXT_HEIGHT*0, bitmap_width, MENU_TEXT_HEIGHT, x, y, 0);
		}
		y += h/7;
	}
	bitmap_width = al_get_bitmap_width(bitmap_cin_logo);
	x = w/2 - bitmap_width/2;
	al_draw_bitmap(bitmap_cin_logo, x, y, 0);
}
void writeIP(ALLEGRO_EVENT event, char str[]){
	if(strlen(str) <= 15){
		char temp[] = {event.keyboard.unichar, '\0'};
		if(event.type == ALLEGRO_EVENT_KEY_CHAR){
			if ((event.keyboard.unichar >= '0' && event.keyboard.unichar <= '9') || event.keyboard.unichar == '.'){
				strcat(str, temp);
			}
		}
	}
	if(strlen(str)){
		if(event.keyboard.keycode == ALLEGRO_KEY_BACKSPACE){
			str[strlen(str)-1] = '\0';
		}
	}
}
void writeID(ALLEGRO_EVENT event, char str[]){
	if(strlen(str) <= 12){
	    char temp[] = {event.keyboard.unichar, '\0'};
		if(event.type == ALLEGRO_EVENT_KEY_CHAR){
    		if (event.keyboard.unichar >= 'A' && event.keyboard.unichar <= 'Z'){
				temp[0] = temp[0]-'A'+'a';
				strcat(str, temp);
        	}else if ((event.keyboard.unichar >= '0' && event.keyboard.unichar <= '9') || (event.keyboard.unichar >= 'a' && event.keyboard.unichar <= 'z')){
				strcat(str, temp);
			}
		}
	}
	if(strlen(str)){
		if(event.keyboard.keycode == ALLEGRO_KEY_BACKSPACE){
			str[strlen(str)-1] = '\0';
		}
	}
}
void showTextInput(char str[]){
	int y = h/7*2;
	al_draw_rounded_rectangle(50, y, w-100, y+94, 5, 5, al_map_rgb(255, 255, 255), 7);
	if (strlen(str) > 0){
		al_draw_text(font_upheavtt_94, al_map_rgb(0, 0, 0), w / 2, y, ALLEGRO_ALIGN_CENTRE, str);
	}
}
char optionSelectedByMouse(char n_options, char options[], int pos_x, int pos_y){
	int y = h/7*2;
	char index = (pos_y-y)/(h/7);
	if(index >= 0 && index < n_options){
		ALLEGRO_BITMAP *bitmap = bitmap_options[options[index]];
		int bitmap_width = al_get_bitmap_width(bitmap);
		int min_x = w/2 - bitmap_width/2;
		int max_x = w/2 + bitmap_width/2;
		if(pos_x >= min_x && pos_x <= max_x){
			return index;
		}
	}
	return -1;
}
char skinSelectedByMouse(int pos_x, int pos_y){
	int y = h - (SKIN_H*6 + 50);
	int x = pos_x - (w/2 - (3*(SKIN_W*6) + 2.5*48));
	int index = x / ((SKIN_W*6)+48);
	if(pos_y >= y && pos_y <= y+SKIN_H*6){
		if(index >= 0 && index < N_SKINS){
			return index;
		}
	}
	return -1;
}
void showAllSkins(char skin, char skin_frame){
	int y = h - (SKIN_H*6 + 50);
	int x = w/2 - (3*(SKIN_W*6) + 2.5*48);
	register char i;
	for(i = 0; i < N_SKINS; i++){
		if(i == skin){
			al_draw_bitmap_region(bitmap_skins_amplified[i], 6*SKIN_W*skin_frame, 0, 6*SKIN_W, 6*SKIN_H, x, y, 0);
		}else{
			al_draw_bitmap_region(bitmap_skins_amplified[i], 6*SKIN_W, 0, 6*SKIN_W, 6*SKIN_H, x, y, 0);
		}
		x += (SKIN_W*6)+48;
	}
}
void loadTileMapMatrix(){
	register int i, j, k, l;
	map = (Map *) malloc(sizeof(Map));
	map->rows = TILEMAP_HEIGHT/TILE_SIZE;
	map->cols = TILEMAP_WIDTH/TILE_SIZE;
	map->map = (char *) malloc(sizeof(char)*map->rows*map->cols);
	FILE *file = fopen("mapClient.txt", "r");
	for(i = 0; i < N_FLAGS; i++){
		fscanf(file, " %hi %hi", &teams[i].spawn_r, &teams[i].spawn_c);
	}
	fscanf(file, " %hi %hi", &map->n_caves, &map->n_trees);
	caves = (TileObject *) malloc(sizeof(TileObject)*map->n_caves);
	trees = (TileObject *) malloc(sizeof(TileObject)*map->n_trees);
	bases = (TileObject *) malloc(sizeof(TileObject)*8);
	short src, dest;
	for(i = 0; i < map->n_caves; i++){
		fscanf(file, " %hi %hi", &src, &dest);
		caves_graph[src] = dest;
	}
	char b, r, c;
	j = 0, k = 0, l = 0;
	bool base_1_founded = 0, base_2_founded = 0;
	for(i = 0; i < map->rows*map->cols; i++){
		b = fgetc(file);
		if(b == '\n') b = fgetc(file);
		if(isalpha(b)){
			b-='A'-10;
		}else if(isdigit(b)){
			b-='0';
		}
		r = i/map->cols;
		c = i%map->cols;
		if(b == TREE){
			trees[j].r = r;
			trees[j].c = c;
			trees[j].type = b;
			j++;
		}else if(b == CAVE){
			caves[l].r = r;
			caves[l].c = c;
			caves[l].type = b;
			l++;
		}else if(b == BASE_1 && !base_1_founded){
			bases[0].r = r;
			bases[0].c = c;
			bases[0].type = b;
			base_1_founded = true;
		}else if(b == BASE_2 && !base_2_founded){
			bases[1].r = r;
			bases[1].c = c;
			bases[1].type = b;
			base_2_founded = true;
		}
		*(map->map+i) = b;
	}
	fclose(file);
}
void showTileMapMatrix(){
	register int r, c;
	for(r = 0; r < map->rows; r++){
		printf("%d", getTileContent(r, 0));
		for(c = 1; c < map->cols; c++){
			printf(" %d", getTileContent(r, c));
		}
		printf("\n");
	}
}
char getTileContentByPosition(int x, int y){
	char r = y/TILE_SIZE, c = x/TILE_SIZE;
	return *(map->map+(r*map->cols + c));
}
char getTileContent(char r, char c){
	return *(map->map+(r*map->cols + c));
}
void setTileContentByPosition(int x, int y, char tile_id){
	char r = y/TILE_SIZE, c = x/TILE_SIZE;
	*(map->map+(r*map->cols+c)) = tile_id;
}
void setTileContent(char r, char c, char tile_id){
	*(map->map+(r*map->cols+c)) = tile_id;
}
bool isWalkable(int x, int y){
	char r = y/TILE_SIZE, c = x/TILE_SIZE;
	if(!isPassable(x+SKIN_W/3, y) || !isPassable(x-SKIN_W/3, y) || !isPassable(x+SKIN_W/3, y+SKIN_H/2) || !isPassable(x-SKIN_W/3, y+SKIN_H/2)){
		return 0;
	}
	return 1;
}
bool isPassable(int x, int y){
	char r = y/TILE_SIZE, c = x/TILE_SIZE;
	if(x < 0 || y < 0 || x > TILEMAP_WIDTH || y > TILEMAP_HEIGHT) return 0;
	if(getTileContent(r, c) == WALL) return 0;
	if(getTileContent(r, c) == TREE) return 0;
	if(!getTileContent(r, c)) return 0;
	return 1;
}
void catchFlags(Player *player){
	if(!player->has_team_flag){
		catchTeamFlag(player);
	}
	if(!player->has_enemy_flag){
		catchEnemyFlag(player);
	}
}
void catchEnemyFlag(Player *player){
	char enemy_flag = FLAG_1 + !player->team;
	if(getTileContentByPosition(player->pos.x, player->pos.y) == enemy_flag){
		sendToServer(player, CATCHED_ENEMY_FLAG);
	}
}
void catchTeamFlag(Player *player){
	char team_flag = FLAG_1 + player->team;
	if(getTileContentByPosition(player->pos.x, player->pos.y) == team_flag){
		sendToServer(player, CATCHED_TEAM_FLAG);
	}
}
void dropFlags(Player *player){
	if(player->has_team_flag){
		dropTeamFlag(player);
	}else if(player->has_enemy_flag){
		dropEnemyFlag(player);
	}
}
void dropEnemyFlag(Player *player){
	if(getTileContentByPosition(player->pos.x, player->pos.y) == WALKABLE){
		sendToServer(player, DROPPED_ENEMY_FLAG);
	}
}
void dropTeamFlag(Player *player){
	if(getTileContentByPosition(player->pos.x, player->pos.y) == WALKABLE){
		sendToServer(player, DROPPED_TEAM_FLAG);
	}
}
void caveIn(Point *pos){
	char r = pos->y/TILE_SIZE, c = pos->x/TILE_SIZE;
	short int dest = caves_graph[r*100+c];
	pos->y = (dest/100)*TILE_SIZE+SKIN_H/2;
	pos->x = (dest%100)*TILE_SIZE+SKIN_W/2;
	player_dir = DOWN;
}
void respawnPlayer(Player *player){
	player->pos.x = teams[player->team].spawn_c*TILE_SIZE;
	player->pos.y = teams[player->team].spawn_r*TILE_SIZE;
}
void drawGame(Player *player, int w, int h){
	al_draw_bitmap(bitmap_map, 0, 0, 0);
	drawShadows();
	drawBases();
	drawFlags();
	playerSkinAnimation(&player->pos, &player->mouse, &player->speed);
	APCSkinAnimation(player->id);
	drawPlayers(player->id, &player->pos);
	drawBullets();
	drawTrees();
}
void drawHUD(Player *player, int w, int h){
	drawLife(&player->pos, player->live, w, h);
}
void drawShadows(){
	register int i;
	int x, y;
	for(i = 0; i < map->n_trees; i++){
		x = trees[i].c*TILE_SIZE;
		y = trees[i].r*TILE_SIZE;
		al_draw_bitmap(bitmap_tree_shadow, x-7, y-23, 0);
	}
}
void drawBullets(){
	register int i;
	for(i = 0; i < N_BULLETS*MAX_PLAYERS; i++){
		if(bullets[i].live){
			al_draw_bitmap(bitmap_snowball, bullets[i].pos.x - TILE_SIZE/2, bullets[i].pos.y - TILE_SIZE/2, 0);
		}
	}
}
void drawFlags(){
	register int i;
	int x, y;
	for(i = 0; i < N_FLAGS; i++){
		if(!flags[i].has_catched){
			setTileContent(flags[i].tile.r, flags[i].tile.c, FLAG_1 + i);
			x = flags[i].tile.c*TILE_SIZE;
			y = flags[i].tile.r*TILE_SIZE;
			al_draw_bitmap(bitmap_flags[i], x, y, 0);
		}
	}
}
void drawBases(){
	register int i;
	int x, y;
	for(i = 0; i < N_FLAGS; i++){
		al_draw_bitmap(bitmap_bases[bases[i].type - BASE_1], bases[i].c*TILE_SIZE, bases[i].r*TILE_SIZE, 0);
	}
}
void drawLife(Point *pos, int life, int w, int h) {
	register int i;
	int ref_x = (-w*0.5+50)/ZOOM+pos->x, ref_y = (-h*0.5+50)/ZOOM+pos->y, square_size = 30, x = 0, n_squares = life;
	float ratio = 0;
	for(i = 0; i < n_squares; i++){
		al_draw_bitmap(bitmap_heart, ref_x+x, ref_y, 0);
		x += square_size + 1;
	}
	if(player->has_team_flag){
		al_draw_bitmap(bitmap_flags[player->team+2], ref_x, ref_y + 1.1*square_size, 0);
	}
	if(player->has_enemy_flag){
		al_draw_bitmap(bitmap_flags[!player->team+2], ref_x + square_size + 1, ref_y + 1.1*square_size, 0);
	}
	if(player->kills || player->deaths){
		if(!player->deaths){
			ratio = player->kills;
		}else{
			ratio = (float)player->kills/(float)player->deaths;
		}
	}
	al_draw_bitmap(bitmap_score, (ref_x+w/2)-(50)-al_get_bitmap_width(bitmap_score), ref_y, 0);
	al_draw_textf(font_upheavtt_30, al_map_rgb(102,0,153), (ref_x+w/2)-(50)-5*al_get_bitmap_width(bitmap_score)/6, ref_y+al_get_bitmap_height(bitmap_score)/2-5, ALLEGRO_ALIGN_CENTER, "%d", player->kills);
	al_draw_textf(font_upheavtt_30, al_map_rgb(102,0,153), (ref_x+w/2)-(50)-al_get_bitmap_width(bitmap_score)/2, ref_y+al_get_bitmap_height(bitmap_score)/2-5, ALLEGRO_ALIGN_CENTER, "%d", player->deaths);
	al_draw_textf(font_upheavtt_30, al_map_rgb(102,0,153), (ref_x+w/2)-(50)-al_get_bitmap_width(bitmap_score)/6, ref_y+al_get_bitmap_height(bitmap_score)/2-5, ALLEGRO_ALIGN_CENTER, "%.2f", ratio);
}
void drawTrees(){
	register int i;
	int x, y;
	for(i = 0; i < map->n_trees; i++){
		x = trees[i].c*TILE_SIZE;
		y = trees[i].r*TILE_SIZE;
		al_draw_bitmap(bitmap_tree, x-7, y-23, 0);
	}
}


void drawPlayer(char id, char team, Point *pos, char life){
	al_draw_bitmap_region(bitmap_skins[players[id].skin], players_frames[id]*SKIN_W, players_dirs[id]*SKIN_H, SKIN_W, SKIN_H, pos->x - SKIN_W/2, pos->y - SKIN_H/2, 0);
	al_draw_filled_triangle(pos->x, pos->y-SKIN_H/2-3, pos->x-SKIN_W/2+3, pos->y-SKIN_H/2-8, pos->x+SKIN_W/2-3, pos->y-SKIN_H/2-8, color_teams[team]);
	al_draw_bitmap(bitmap_life_status_bar, pos->x-SKIN_H/2, pos->y+(SKIN_H/2)+2, 0);
	
	ALLEGRO_COLOR life_status_bar_color;
	if(life >= 4) life_status_bar_color = COLOR_MAX;
	else if(life == 3) life_status_bar_color = COLOR_MID;
	else life_status_bar_color = COLOR_MIN;
	register int i;
	int x = 0;
	for(i = 0; i < life; i++) {
		al_draw_filled_rectangle(pos->x-SKIN_H/2 + x+1, pos->y+(SKIN_H/2)+3, pos->x-SKIN_H/2 + 3 + x+1, pos->y+(SKIN_H/2)+5, life_status_bar_color);
		x += 4;
	}
}

int cmp_y (const void *a, const void *b){
	return players[*(char *) a].pos.y - players[*(char *) b].pos.y;
}

void drawPlayers(char id, Point *pos){
	register char i;
	char apc_id;
	char draw_priority[MAX_PLAYERS];
	for(i = 0; i < MAX_PLAYERS; i++) draw_priority[i] = i;
	qsort(draw_priority, MAX_PLAYERS, sizeof(char), cmp_y);
	for(i = 0; i < MAX_PLAYERS; i++) {
		apc_id = draw_priority[i];
		if(apc_id != id) {
			if(players[apc_id].live) drawPlayer(apc_id, players[apc_id].team, &players[apc_id].pos, players[apc_id].live);
			if(skinFrame%4 == 0) {
				if(previous_position[apc_id].x != players[apc_id].pos.x) previous_position[apc_id].x = players[apc_id].pos.x;
				if(previous_position[apc_id].y != players[apc_id].pos.y) previous_position[apc_id].y = players[apc_id].pos.y;
			}
		}else if(player->live){
			al_draw_filled_triangle(pos->x, pos->y-SKIN_H/2-3, pos->x-SKIN_W/2+3, pos->y-SKIN_H/2-8, pos->x+SKIN_W/2-3, pos->y-SKIN_H/2-8, color_teams[player->team]);
			al_draw_filled_rectangle(pos->x-SKIN_W/2+3, pos->y-SKIN_H/2-8, pos->x+SKIN_W/2-3, pos->y-SKIN_H/2-10, COLOR_PLAYER);
			al_draw_bitmap_region(bitmap_skins[player->skin], player_frame*SKIN_W, player_dir*SKIN_H, SKIN_W, SKIN_H, pos->x - SKIN_W/2, pos->y - SKIN_H/2, 0);
		}
	}
}

void APCSkinAnimation(char id) {
	register char i;
	char apc_id;
	for(i = 0; i < MAX_PLAYERS; i++) {
		if(i != id && players[i].live) {
			if(previous_position[i].x == players[i].pos.x && previous_position[i].y == players[i].pos.y || ((skinFrame%SKIN_FRAME_SPEED == 0) && previous_player_fired[i])) {
				players_frames[i] = 1;
			} else {
				if(previous_position[i].x == players[i].pos.x) {
					if(previous_position[i].y > players[i].pos.y) players_dirs[i] = UP;
					else players_dirs[i] = DOWN;
				
				} else { 
					if(previous_position[i].x > players[i].pos.x) players_dirs[i] = LEFT;
					else players_dirs[i] = RIGHT;
				
				}
				players_frames[i] = ++players_frames[i]%N_SKIN_FRAMES;
			}
			previous_player_fired[i] = false;
		}
	}
	float tang;
	for(i = 0; i < N_BULLETS*MAX_PLAYERS; i++) {
		if(bullets[i].live && bullets[i].player_id != id) {
			apc_id = bullets[i].player_id;
			if(apc_id != id && players[apc_id].live) {
				if(bullets[i].live >= 25) {
					tang = bullets[i].speed.vy/bullets[i].speed.vx;
					if(bullets[i].speed.vx != 0 && fabs(tang) <= 1){
						if(bullets[i].speed.vx > 0){
							players_dirs[apc_id] = RIGHT;
						}else{
							players_dirs[apc_id] = LEFT;
						}
					}else{
						if(bullets[i].speed.vy > 0){
							players_dirs[apc_id] = DOWN;
						}else{
							players_dirs[apc_id] = UP;
						}
					}
				}
				players_frames[apc_id] = ++players_frames[apc_id]%N_SKIN_FRAMES;
				previous_player_fired[apc_id] = true;
			}
		}
	}
}

void playerSkinAnimation(Point *pos, FloatPoint *mouse_pos, Vector *v) {
	float tang;
	if(skinFrame%4 == 0) {
		if(my_player_fired) {
			tang = mouse_pos->y/mouse_pos->x;
			if(mouse_pos->x != 0 && fabs(tang) <= 1){
				if(mouse_pos->x > 0){
					player_dir = RIGHT;
				}else{
					player_dir = LEFT;
				}
			}else{
				if(mouse_pos->y > 0){
					player_dir = DOWN;
				}else{
					player_dir = UP;
				}
			}
			my_player_fired = false;
			player_frame = ++player_frame%N_SKIN_FRAMES;
		} else {
			if(v->vx == 0 && v->vy == 0) {
				player_frame = 1;
			} else {
				if(v->vy == PLAYER_SPEED || v->vy == -PLAYER_SPEED) {
					if(v->vy < 0){
						player_dir = UP;
					}else{
						player_dir = DOWN;
					}
				} else {
					if(v->vx < 0){
						player_dir = LEFT;
					}else{
						player_dir = RIGHT;
					}
				}
				player_frame = ++player_frame%N_SKIN_FRAMES;
			}
		}
	}
}

void *rcvFromServer(ALLEGRO_THREAD *thr, void *arg){
	ThreadData *data  = (ThreadData*) arg;
	while(!al_get_thread_should_stop(thr)){
		al_lock_mutex(data->mutex);
		recvMsgFromServer(data->game, DONT_WAIT);
		al_unlock_mutex(data->mutex);
		al_rest(0.01);
	}
	return NULL;
}
void sendToServer(Player *player, char event){
	ClientPacket packet;
	if(player != NULL){
		strcpy(packet.name, player->name);
		packet.skin = player->skin;
		packet.speed.vx = player->speed.vx;
		packet.speed.vy = player->speed.vy;
		packet.pos.x = player->pos.x;
		packet.pos.y = player->pos.y;
		packet.mouse.x = player->mouse.x;
		packet.mouse.y = player->mouse.y;
	}
	packet.event = event;
	sendMsgToServer(&packet, sizeof(ClientPacket));
}
