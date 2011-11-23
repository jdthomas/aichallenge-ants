#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

// this header is basically self-documenting

struct game_info {
	int loadtime;
	int turntime;
	int rows;
	int cols;
	int turns;
	int viewradius_sq;
	int attackradius_sq;
	int spawnradius_sq;
    int seed;
	uint8_t *map;
};

struct basic_ant {
    int row;
    int col;
    char player;
};

struct my_ant {
    int id;
    int row;
    int col;
};

struct food {
    int row;
    int col;
};

struct game_state {
    struct my_ant *my_ants;
    struct basic_ant *enemy_ants;
    struct food *food;
    struct basic_ant *dead_ants;
    
    int my_count;
    int enemy_count;
    int food_count;
    int dead_count;

    int my_ant_index;
};

#define MAX_PLAYERS   		  9
#define UNSEEN      ((uint8_t)0)
#define WATER       ((uint8_t)UNSEEN+MAX_PLAYERS+1)
#define LAND        ((uint8_t)WATER+MAX_PLAYERS+1)
#define HILL        ((uint8_t)LAND+MAX_PLAYERS+1)
#define FOOD        ((uint8_t)HILL+MAX_PLAYERS+1)
#define ANT_OFFSET  ((uint8_t)FOOD+MAX_PLAYERS+1)
#define HILL_OFFSET ((uint8_t)ANT_OFFSET+MAX_PLAYERS+1)
#define DEAD_OFFSET ((uint8_t)HILL_OFFSET+MAX_PLAYERS+1)
#define MOVE_OFFSET ((uint8_t)DEAD_OFFSET+MAX_PLAYERS+1)

static inline int IS_LAND(uint8_t c){
	return c==LAND;
}
static inline int IS_WATER(uint8_t c){
	return c==WATER;
}
static inline int IS_MY_ANT(uint8_t c){
	return (c == ANT_OFFSET);
}
static inline int IS_ANT(uint8_t c){
	return (c >= ANT_OFFSET) && (c <= ANT_OFFSET+MAX_PLAYERS);
}
static inline int IS_HILL(uint8_t c){
	return (c >= HILL_OFFSET) && (c <= HILL_OFFSET+MAX_PLAYERS);
}
static inline int IS_ENEMY_HILL(uint8_t c){
	return (c >= HILL_OFFSET+1) && (c <= HILL_OFFSET+MAX_PLAYERS);
}
static inline int IS_MY_HILL(uint8_t c){
	return (c == HILL_OFFSET);
}
static inline int IS_DEAD(uint8_t c){
	return (c >= DEAD_OFFSET) && (c <= DEAD_OFFSET+MAX_PLAYERS);
}
static inline int IS_MOVE(uint8_t c){
	return (c >= MOVE_OFFSET) && (c <= MOVE_OFFSET+MAX_PLAYERS);
}
static inline int IS_UNSEEN(uint8_t c){
	return c==UNSEEN;
}
static inline int IS_FOOD(uint8_t c){
	return c==FOOD;
}
static inline int IS_OBJECT(uint8_t c){
	return IS_FOOD(c)||IS_HILL(c)||IS_ANT(c)||IS_DEAD(c)||IS_MOVE(c);
}
static inline int IS_BACKGROUND(uint8_t c){
	return (IS_UNSEEN(c) || IS_LAND(c) || IS_WATER(c));
}

#define INDEX_AT(r,c) (r*Info->cols+c)
#define AT_INDEX(r,c,offset) ({c=offset%Info->cols; r=(offset-c)/Info->cols;})

#define WRAP_R(r) ( (r+Info->rows)%Info->rows )
#define WRAP_C(c) ( (c+Info->cols)%Info->cols )

#define NORTH(r,c) INDEX_AT(WRAP_R(r-1),       c )
#define  EAST(r,c) INDEX_AT(       r   ,WRAP_C(c+1))
#define SOUTH(r,c) INDEX_AT(WRAP_R(r+1),       c )
#define  WEST(r,c) INDEX_AT(       r   ,WRAP_C(c-1))


void render_map(struct game_info *Info);
float distance(int row1, int col1, int row2, int col2, struct game_info *Info);
void do_turn(struct game_state *Game, struct game_info *Info);
void move(int index, char dir, struct game_state* Game, struct game_info* Info);
void _init_map(char *data, struct game_info *game_info);
void _init_ants(char *data, struct game_info *game_info);
void _init_game(struct game_info *game_info, struct game_state *game_state);
