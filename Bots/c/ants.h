#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#define NEAR_HOME_DIST_SQ (2*Info->viewradius_sq)
#define VERY_NEAR_HOME_DIST_SQ (2*Info->attackradius_sq)

#ifdef DEBUG
# define LOG(format,args...) fprintf(stderr,format,##args)
#else
# define LOG(format,args...)
#endif

enum {
    cm_FOOD,
    cm_HILL,
    cm_UNSEEN,
    cm_VIS,
    cm_BATTLE,
    cm_DEFENSE,
    cm_BFS,
    cm_TOTAL,
};
enum {
    w_RAND=cm_TOTAL,
    w_MOMEN,
    w_TOTAL,
};

enum{
    st_STARTGAME,
    st_MIDGAME,
    st_ENDGAME,
    st_TOTAL,
};

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

    double *cost_map[cm_TOTAL];
    struct attackers_t *attacked_by;
    int *vis_tmp;
    char *momentum[2];
    int cur_momentum_buf;
    int game_strategy;

    struct game_state * Game;
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
    struct my_ant *my_hills;
    struct basic_ant *enemy_ants;
    struct basic_ant *enemy_hills;
    struct food *food;
    struct basic_ant *dead_ants;

    int my_count;
    int my_hill_count;
    int enemy_hill_count;
    int enemy_count;
    int food_count;
    int dead_count;

    int my_ant_index;
};

#define MAX_PLAYERS   		  10
#define MAP_UNSEEN          ((uint8_t)0)
#define MAP_WATER           ((uint8_t)1)
#define MAP_LAND            ((uint8_t)2)
#define MAP_FOOD            ((uint8_t)4)
//#define BACKGROUND_BIT      ((uint8_t)0)
#define NEAR_HOME_BIT       ((uint8_t)0x08)
#define ANT_BIT             ((uint8_t)0x10)
#define HILL_BIT            ((uint8_t)0x20)
#define DEAD_BIT            ((uint8_t)0x40)
#define MOVE_BIT            ((uint8_t)0x80)
#define BACKGROUND_MASK     ((uint8_t)ANT_BIT|HILL_BIT|DEAD_BIT)
#define OWNER_MASK          ((uint8_t)0x7)

static inline int GET_OWNER(uint8_t c){
    return (c&OWNER_MASK);
}
static inline int IS_LAND(uint8_t c){
	return (0==(c&BACKGROUND_MASK)) && (GET_OWNER(c)==MAP_LAND);
}
static inline int IS_WATER(uint8_t c){
	return (0==(c&BACKGROUND_MASK)) && (GET_OWNER(c)==MAP_WATER);
}
static inline int IS_UNSEEN(uint8_t c){
	return (0==(c&BACKGROUND_MASK)) && (GET_OWNER(c)==MAP_UNSEEN);
}
static inline int IS_FOOD(uint8_t c){
	return (0==(c&BACKGROUND_MASK)) && (GET_OWNER(c)==MAP_FOOD);
}
static inline int IS_ANT(uint8_t c){
	return !!(c&ANT_BIT);
}
static inline int IS_MY_ANT(uint8_t c){
	return  (c&ANT_BIT) && (GET_OWNER(c)==0);
}
static inline int IS_ENEMY_ANT(uint8_t c){
	return (c&ANT_BIT) && (GET_OWNER(c)!=0);
}
static inline int IS_HILL(uint8_t c){
	return !!(c&HILL_BIT);
}
static inline int IS_ENEMY_HILL(uint8_t c){
	return (c&HILL_BIT) && (GET_OWNER(c)!=0);
}
static inline int IS_MY_HILL(uint8_t c){
	return (c&HILL_BIT) && (GET_OWNER(c)==0);
}
static inline int IS_DEAD(uint8_t c){
	return !!(c&DEAD_BIT);
}
static inline int IS_MOVE(uint8_t c){
	return !!(c&MOVE_BIT);
}
static inline int IS_NEAR_HOME(uint8_t c){
	return !!(c&NEAR_HOME_BIT);
}

static inline int IS_OBJECT(uint8_t c){
	return IS_FOOD(c)||IS_HILL(c)||IS_ANT(c)||IS_DEAD(c)||IS_MOVE(c);
}
static inline int IS_BACKGROUND(uint8_t c){
	return (IS_UNSEEN(c) || IS_LAND(c) || IS_WATER(c));
}
#if 1
static inline void sanity_prints()
{
    int i=0;
    for(i=0;i<256;i++)
    {
        fprintf(stderr,"%d: %s %s %s %s %s %s %s %s %s %s  %s %s %d\n",
                i,
                IS_UNSEEN(i)?"u":" ",
                IS_WATER(i)?"w":" ",
                IS_LAND(i)?"l":" ",
                IS_HILL(i)?"h":" ",
                IS_MY_HILL(i)?"mh":"  ",
                IS_FOOD(i)?"f":" ",
                IS_ANT(i)?"a":" ",
                IS_MY_ANT(i)?"ma":"  ",
                IS_DEAD(i)?"d":" ",
                IS_MOVE(i)?"mo":"  ",
                IS_OBJECT(i)?"o":" ",
                IS_BACKGROUND(i)?"b": "",
                GET_OWNER(i)
               );
    }
}
#endif


#define INDEX_AT(r,c) (r*Info->cols+c)
#define AT_INDEX(r,c,offset) ({c=offset%Info->cols; r=(offset-c)/Info->cols;})

#define WRAP_R(r) ( (r+Info->rows)%Info->rows )
#define WRAP_C(c) ( (c+Info->cols)%Info->cols )

#define NORTH(r,c) INDEX_AT(WRAP_R(r-1),       c )
#define  EAST(r,c) INDEX_AT(       r   ,WRAP_C(c+1))
#define SOUTH(r,c) INDEX_AT(WRAP_R(r+1),       c )
#define  WEST(r,c) INDEX_AT(       r   ,WRAP_C(c-1))


void render_map(struct game_info *Info);
void do_turn(struct game_state *Game, struct game_info *Info);
void move(int index, char dir, struct game_state* Game, struct game_info* Info);
void _init_map(char *data, struct game_info *game_info);
void _init_ants(char *data, struct game_info *game_info);
void _init_game(struct game_info *game_info, struct game_state *game_state);
int edist_sq(int row1, int col1, int row2, int col2, struct game_info *Info);
double edist(int row1, int col1, int row2, int col2, struct game_info *Info);

int near_home_calc(struct game_info *Info, int offset, int rad);

