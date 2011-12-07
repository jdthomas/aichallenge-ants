#include <assert.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <unistd.h>

#include "ants.h"

#define BFS_ENABLE 1

#define USE_MOMENTUM 1
//
//#define DIFFUSE_BATTLE
#define TIMEOUT_PROTECTION
//#define DIFFUSE_VIS
#define ANTS_PER_DEFENDER 8
#define DEFEND_HILL 1
//#define NEAR_HOME_DIST_SQ 100
#define NEAR_HOME_DIST_SQ (2*Info->viewradius_sq)
#define DEFEND_HILL_SCORE 0.99
#define STRATEGY_STARTGAME_TURNS  10


#define PLT_BFS     (1<< 0)
#define PLT_SCORE   (1<< 1)
#define PLT_FOOD    (1<< 2)
#define PLT_HILL    (1<< 3)
#define PLT_UNSEEN  (1<< 4)
#define PLT_BATTLE  (1<< 5)
#define PLT_VISION  (1<< 6)
#define PLT_DEFENSE (1<< 7)
#ifdef DEBUG
# define PLOT_DUMP (0|PLT_SCORE|PLT_FOOD)
#else
# define PLOT_DUMP (0)
#endif

// TODO:
// [/] 1. defence
// [x] 1.1 if my_count > 8/hill: assign closest ant to hill as defence
// [/] 1.2 Kamikaze attack enemys 'near' home
// [X] 1.3 Move defend home markers to separate layer (same as current defense?)
// [ ] 1.4 ??? Instead of defense layer, enemylocation * distance to hill ???
// 				Simple as difusing enemys and diffusing my_hills and multiply in calc_score
// [X] 2. remember seen stuff until see the cell and it is gone
// [X] 3. slight preference for momentum?
// [X] 4. once dead, attack bases, ignore food.
// [/] 5. sort my ants by degrees of freedom and move most constrained ones
//        first? Or sort them by best moves? n**2? move people who "stayed put"
//        last move first? Randomize the order I iterate atns to move them?
//        The problem trying to solve is when two ants toggle places in two
//        local minima. -- currently randomized, seems to help.
// [X] 6. distance -> edist edist_sq functions, save from doing sqrt so much
// [/] 7. Move globals into game_info?
// [/] 8. random_walk_04p_01 bug, equal distance food confuses us -- should be
//        fixed by using BFS only for first 10 turns; may want to tune this.
// [ ] 9. test timeout protection, possibly add two-staged timeout, so can move
//        some ants in a turn based on stale info.
// [ ] 10. Recently been layer? "for c in cells: if MyANT(c): c-=X; elif c<0: c+=1" diffused?
// [ ] 11. Add attacking enemies to the BFS list


#define MAX_ATTACKERS 50 /* FIXME: size of perimeter of attack radius */
struct attackers_t {
	int atkr_count;
	int atkrs[MAX_ATTACKERS];
};
double alpha[cm_TOTAL] = {0.8, 0.8, 0.8, 0.8, 0.8, 0.8};
//double alpha[cm_TOTAL] = {0.2, 0.2, 0.2, 0.2, 0.2, 0.2};
char weight_labels[w_TOTAL] = {
    [cm_FOOD]    = 'f',
    [cm_HILL]    = 'h',
    [cm_UNSEEN]  = 'u',
    [cm_VIS]     = 'v',
    [cm_BATTLE]  = 'b',
    [cm_BFS]     = 's',
    [cm_DEFENSE] = 'd',
    [w_RAND]     = 'r',
    [w_MOMEN]    = 'm',
};
double weights[st_TOTAL][w_TOTAL] = {
    [st_STARTGAME] = {
        [cm_BFS]     = 1.0,

        [cm_FOOD]    = 0.0,
        [cm_HILL]    = 0.0,
        [cm_UNSEEN]  = 0.0,
        [cm_VIS]     = 0.0,
        [cm_BATTLE]  = 0.0,
        [cm_DEFENSE] = 0.0,
        [w_RAND]     = 0.0,
        [w_MOMEN]    = 0.0,
    },
    [st_MIDGAME] = {
        [cm_FOOD]    = 2.5,
        [cm_HILL]    = 5.0,
        [cm_UNSEEN]  = 1.0,
        [cm_VIS]     = 0.0125,
        [cm_BATTLE]  = 1.0,
        [cm_BFS]     = 1.0,
        [cm_DEFENSE] = 5.0,
        [w_RAND]     = 1e-10,//
        [w_MOMEN]    = 0.0125,
    },
    [st_ENDGAME] = {
        [cm_HILL]    = 5.0,
        [cm_UNSEEN]  = 1.0,
        [cm_BATTLE]  = 1.0,
        [cm_BFS]     = 1.0,
        [w_RAND]     = 1e-10,
        [w_MOMEN]    = 0.0125,

        [cm_FOOD]    = 0.0,
        [cm_VIS]     = 0.0,
        [cm_DEFENSE] = 0.0,
    }
};
static int need_reset = 1;
static jmp_buf buf;
static int debug_on=0;


/******** BEGAIN BFS DATA STRUCTS ********/
struct q_data_t {
    int offset;
    int distance;
};
struct dequeue_t {
    struct q_data_t * data;
    int push;
    int pop;
    int len;
};

void queue_reinit(struct dequeue_t *Q)
{
    Q->push = Q->pop = 0;
}
void queue_init(struct dequeue_t *Q, int row, int col)
{
    Q->len = row*col;
    Q->data = malloc(Q->len*sizeof(struct q_data_t));
    queue_reinit(Q);
}
void queue_push(struct dequeue_t* Q, struct q_data_t *item)
{
    Q->data[Q->push] = *item;
    Q->push = (Q->push+1)%Q->len;
}
void queue_pop(struct dequeue_t* Q, struct q_data_t *item)
{
    *item = Q->data[Q->pop];
    Q->pop = (Q->pop+1)%Q->len;
}
int queue_is_empty(struct dequeue_t *Q)
{
	return Q->push == Q->pop;
}
struct set_t {
    uint32_t *bitmask;
    int size_in_blocks;
    int row,col;
};
inline void SET_BIT(uint32_t *buf, int bit)
{
    *buf |= 1<<bit;
}
inline int GET_BIT(uint32_t *buf, int bit)
{
    return !!(*buf & (1<<bit));
}
#define SET_BLOCK_SIZE_BY (sizeof(uint32_t))
#define SET_BLOCK_SIZE_BI (SET_BLOCK_SIZE_BY*8)
void set_reinit(struct set_t * s)
{
    memset(s->bitmask,0,SET_BLOCK_SIZE_BY*s->size_in_blocks);
}
void set_init(struct set_t * s, int row, int col)
{
    s->row=row;
    s->col=col;
	int max_item_index = row*col;
    s->size_in_blocks = (max_item_index+SET_BLOCK_SIZE_BI-1)/SET_BLOCK_SIZE_BI;
    s->bitmask = malloc(SET_BLOCK_SIZE_BY*s->size_in_blocks);
    set_reinit(s);
	//fprintf(stderr,"Created set, max size: %d %ld %d\n", s->size_in_blocks, SET_BLOCK_SIZE_BI, max_item_index);
}
void set_destroy(struct set_t * s)
{
    free(s->bitmask);
}
void set_insert(struct set_t *s, int offset)
{
    int ui_num = offset/SET_BLOCK_SIZE_BI;
    int bit_num = offset%SET_BLOCK_SIZE_BI;
	//fprintf(stderr,"Set insert at %d,[%d][%d]\n", offset,ui_num,bit_num);
    SET_BIT(&s->bitmask[ui_num],bit_num);
}
int set_is_member(struct set_t * s, int offset)
{
    int ui_num = offset/SET_BLOCK_SIZE_BI;
    int bit_num = offset%SET_BLOCK_SIZE_BI;
	//fprintf(stderr,"Set check at %d,[%d][%d]\n", offset,ui_num,bit_num);
    return GET_BIT(&s->bitmask[ui_num],bit_num);
}
/******** END BFS DATA STRUCTS ********/

static struct set_t bfs_seen; // Alloc once, zero out each turn
static struct dequeue_t bfs_queue;// Alloc once to (sizeof(q_data_t)*Info->rows*Info->cols), reuse



void timeout(int sig) {
    siglongjmp(buf, 1);
}

double timevaldiff(struct timeval *starttime, struct timeval *finishtime)
{
  double msec;
  msec=(finishtime->tv_sec-starttime->tv_sec)*1000;
  msec+=(finishtime->tv_usec-starttime->tv_usec)/1000.0;
  return msec;
}

// returns the absolute value of a number; used in distance function

inline int abs(int x) {
    return (x >= 0)?x:-x;
}

inline int min(int x,int y) {
    return (x < y)?x:y;
}
inline int max(int x,int y) {
    return (x > y)?x:y;
}


// returns the distance between two items on the grid accounting for map wrapping

int edist_sq(int row1, int col1, int row2, int col2, struct game_info *Info)
{
    int dr, dc;
    int abs1, abs2;

    abs1 = abs(row1 - row2);
    abs2 = Info->rows - abs1;

    if (abs1 > abs2)
        dr = abs2;
    else
        dr = abs1;

    abs1 = abs(col1 - col2);
    abs2 = Info->cols - abs1;

    if (abs1 > abs2)
        dc = abs2;
    else
        dc = abs1;

	return dr*dr + dc*dc;
}
double edist(int row1, int col1, int row2, int col2, struct game_info *Info)
{
    return sqrt(edist_sq(row1,col1,row2,col2,Info));
}

// sends a move to the tournament engine and keeps track of ants new location

void move(int index, char dir, struct game_state* Game, struct game_info* Info)
{
    fprintf(stdout, "O %i %i %c\n", Game->my_ants[index].row, Game->my_ants[index].col, dir);

    /* Update ant's current position */
    // :TODO: DO I need this? Remove?
    switch (dir) {
        case 'N':
            if (Game->my_ants[index].row != 0)
                Game->my_ants[index].row -= 1;
            else
                Game->my_ants[index].row = Info->rows - 1;
            break;
        case 'E':
            if (Game->my_ants[index].col != Info->cols - 1)
                Game->my_ants[index].col += 1;
            else
                Game->my_ants[index].col = 0;
            break;
        case 'S':
            if (Game->my_ants[index].row != Info->rows - 1)
                Game->my_ants[index].row += 1;
            else
                Game->my_ants[index].row = 0;
            break;
        case 'W':
            if (Game->my_ants[index].col != 0)
                Game->my_ants[index].col -= 1;
            else
                Game->my_ants[index].col = Info->cols - 1;
            break;
    }

    int offset = INDEX_AT(Game->my_ants[index].row,Game->my_ants[index].col);
	Info->map[offset] |= MOVE_BIT;
#if USE_MOMENTUM
    Info->momentum[Info->cur_momentum_buf][offset] = dir;
#endif
}

int get_neighbor(struct game_info *Info, int n, int cur_loc, int *new_loc, char *dir)
{
    int old_r,old_c;
	int r=0, c=0;
	char d=-1;
    switch(n) {
        case 0:
            r=-1;c=0;d='N';
            break;
        case 1:
            r=0;c=1;d='E';
            break;
        case 2:
            r=1;c=0;d='S';
            break;
        case 3:
            r=0;c=-1;d='W';
            break;
        case 4:
            r=0;c=0;d='X';
            break;
    }
    AT_INDEX(old_r,old_c,cur_loc);
	r = (old_r+r+Info->rows)%Info->rows;
	c = (old_c+c+Info->cols)%Info->cols;
    if(new_loc) *new_loc = INDEX_AT(r,c);
	if(dir) *dir=d;
	//LOG("neighbor of (%d,%d): (%d,%d)\n", cur_loc->row, cur_loc->col, new_loc->row, new_loc->col);
	return 0;
}

// just a function that returns the string on a given line for i/o
// you don't need to worry about this

char *get_line(char *text)
{
    char *tmp_ptr = text;
    int len = 0;

    while (*tmp_ptr != '\n') {
        ++tmp_ptr;
        ++len;
    }

    char *return_str = malloc(len + 1);
    memset(return_str, 0, len + 1);

    int i = 0;
    for (; i < len; ++i) {
        return_str[i] = text[i];
    }

    return return_str;
}

// main, communicates with tournament engine

void quit_now(int sig) {
    exit(0);
}

int main(int argc, char *argv[])
{
    int action = -1;
    struct sigaction act;
    memset(&act,0,sizeof(act));
    act.sa_handler = quit_now;
    sigaction (SIGKILL, & act, 0);

    struct game_info Info={0};
    struct game_state Game={0};
    Info.map = 0;
    Info.Game=&Game;

    Game.my_ants = 0;
    Game.enemy_ants = 0;
    Game.food = 0;
    Game.dead_ants = 0;

	int i,j;
	for(i=0;i<argc;i++)
	{
		if( strcmp(argv[i],"--debug") ==0){
			debug_on=1;
		}else if( strcmp(argv[i],"--weights") == 0 ){
			int j;
			i++;
			for(j=0;j<w_TOTAL&&i+j<argc;j++)
			{
				char *e;
				double w = strtod(argv[i+j],&e);
				if( e == argv[i+j]) break; // not a double?
				weights[st_MIDGAME][j]=w;
                // :FIXME:
			}
			i+=j;
		}else if( strcmp(argv[i],"--diffuse") == 0 ){
			int j;
			i++;
			for(j=0;j<cm_TOTAL&&i+j<argc;j++)
			{
				char *e;
				double w = strtod(argv[i+j],&e);
				if( e == argv[i+j]) break; // not a double?
				alpha[j]=w;
			}
			i+=j;
		}
	}

#define LOG_FILE_NAME "/tmp/MyBot_c.%d.log"
    char * log_file_name = malloc(strlen(LOG_FILE_NAME)+20);
    sprintf(log_file_name,LOG_FILE_NAME,getpid());
    FILE *f_tmp;
#ifdef DEBUG
	if(debug_on)
		f_tmp = freopen(log_file_name,"wa+",stderr);
	else
#endif
		f_tmp = freopen("/dev/null","wa+",stderr);
    free(log_file_name);


    for(j=0;j<3;j++)
        for(i=0;i<w_TOTAL;i++)
            LOG("weight[%d][%d] = %lf\n", j, i, weights[j][i]);

	// Some prints to check my new map data handling.
    //sanity_prints();

    while (1) { // While game going on ...
        int initial_buffer = 100000;

        char *data = malloc(initial_buffer);
        if(!data) exit(1);
        memset(data, 0, initial_buffer);

        *data = '\n';

        char *ins_data = data + 1;

        int i = 0;

        while (1) { // While reading a command
            ++i;

            if (i > initial_buffer) {
                initial_buffer *= 2;
                int tmp_offset = ins_data - data;
                data = realloc(data, initial_buffer);
                // UGG, who wrote origional, realloc can move you
                ins_data = data + tmp_offset;
                memset(data+initial_buffer/2, 0, initial_buffer/2);
            }

            *ins_data = getchar();

            if (*ins_data == EOF) {
                exit(0);
            }
            if (*ins_data == '\n') {
                char *backup = ins_data;

                while (*(backup - 1) != '\n') {
                    --backup;
                }

                char *test_cmd = get_line(backup);

                if (strcmp(test_cmd, "go") == 0) {
                    action = 0;
                    free(test_cmd);
                    break;
                }
                else if (strcmp(test_cmd, "ready") == 0) {
                    action = 1;
                    free(test_cmd);
                    break;
                }
                free(test_cmd);
            }

            ++ins_data;
        }

        if (action == 0) {
            char *skip_line = data + 1;
            while (*++skip_line != '\n');
            ++skip_line;

            _init_map(skip_line, &Info);
            _init_game(&Info, &Game);
            do_turn(&Game, &Info);
            fprintf(stdout, "go\n");
            fflush(stdout);
        }
        else if (action == 1) {
            _init_ants(data + 1, &Info);

            Game.my_ant_index = -1;

            fprintf(stdout, "go\n");
            fflush(stdout);
        }

        free(data);
    }
}

void prepare_next_turn(struct game_info *Info)
{
    static int done_allocations=0;
    static int r,c;
    int i;
    if(!need_reset) return;
    if(!done_allocations) {
        r=Info->rows,c=Info->cols;
        for(i=0;i<cm_TOTAL;i++)
            if(!Info->cost_map[i]){
                Info->cost_map[i] = calloc(1,sizeof(double)*Info->rows*Info->cols);
                assert(Info->cost_map[i]);
            }
        if(!Info->attacked_by)
            Info->attacked_by = malloc(Info->rows*Info->cols*sizeof(struct attackers_t));
        assert(Info->attacked_by);
#if USE_MOMENTUM
        if(!Info->momentum[0])
            Info->momentum[0] = calloc(1,Info->rows*Info->cols);
        if(!Info->momentum[1])
            Info->momentum[1] = calloc(1,Info->rows*Info->cols);
        Info->cur_momentum_buf = 0;
        assert(Info->momentum[0] && Info->momentum[0]);
#endif
        set_init(&bfs_seen,Info->rows,Info->cols);
        assert(bfs_seen.bitmask);
        queue_init(&bfs_queue,Info->rows,Info->cols);
        assert(bfs_queue.data);
        done_allocations=1;
    }
    assert(r==Info->rows && c==Info->cols);
    set_reinit(&bfs_seen);
    queue_reinit(&bfs_queue);
    memset(Info->attacked_by, 0, Info->rows*Info->cols*sizeof(struct attackers_t));
    memset(Info->cost_map[cm_BATTLE],0,Info->rows*Info->cols*sizeof(double));
    memset(Info->cost_map[cm_VIS],0,Info->rows*Info->cols*sizeof(double));
    memset(Info->cost_map[cm_BFS],0,Info->rows*Info->cols*sizeof(double));
#if USE_MOMENTUM
    Info->cur_momentum_buf = 1-Info->cur_momentum_buf;
    memset(Info->momentum[Info->cur_momentum_buf],0,Info->rows*Info->cols);
#endif
    need_reset = 0;
}

double attack_this_enemy(struct basic_ant * e, struct game_info *Info)
{
	int eoffset = INDEX_AT(e->row,e->col);
	int i;
	int win =0, loss=0;
	//if( !Info->attacked_by[eoffset].atkr_count ) return 0.0; // This guy is not near anyone
	for(i=0;i<Info->attacked_by[eoffset].atkr_count;i++) { // For each ant this enemy can attack...
		int moffset = Info->attacked_by[eoffset].atkrs[i];
		if( Info->attacked_by[eoffset].atkr_count >
			Info->attacked_by[moffset].atkr_count )
			win++;
		if( Info->attacked_by[eoffset].atkr_count <
			Info->attacked_by[moffset].atkr_count )
			loss++;
#if 0
		//// Dump
		{
			int er,ec,ev, mr,mc,mv;
			AT_INDEX(er,ec,eoffset); ev=Info->attacked_by[eoffset].atkr_count;
			AT_INDEX(mr,mc,moffset); mv=Info->attacked_by[moffset].atkr_count;
			LOG("(%d,%d)[%d] attacks (%d,%d)[%d] w=%d l=%d\n", er,ec,ev, mr,mc,mv, win,loss );
		}
#endif
	}
	if( win ) return 1.0;
	return -1.0;
}

/* */
inline double get_food_val(int offset, struct game_info *Info)
{
    if( IS_FOOD(Info->map[offset]) )
        return 1.0;
    if( IS_WATER(Info->map[offset]) ||
        IS_MY_ANT(Info->map[offset]) )
        return 0.0;
    return Info->cost_map[cm_FOOD][offset];
}

inline double get_hill_val(int offset, struct game_info *Info)
{
    if( IS_ENEMY_HILL(Info->map[offset]) )
        return 1.0;
    if( IS_WATER(Info->map[offset]) ||
        IS_MY_HILL(Info->map[offset]) ) // HACK: check my hill here to clear the -1 set below
        return 0.0;
    if ( IS_MY_ANT(Info->map[offset]) )
        return Info->cost_map[cm_HILL][offset]*0.5;  // I want to attack hills in numbers, diffuse partially through my ants
    return Info->cost_map[cm_HILL][offset];
}

inline double get_unseen_val(int offset, struct game_info *Info)
{
    if( IS_UNSEEN(Info->map[offset]) )
        return 1.0;
    if( IS_WATER(Info->map[offset]) ||
        IS_MY_ANT(Info->map[offset]) )
        return 0.0;
    return Info->cost_map[cm_UNSEEN][offset];
}

inline double get_visibility_val(int offset, struct game_info *Info)
{
    if( IS_WATER(Info->map[offset]) )
        return 0.0;
    if( IS_MY_ANT(Info->map[offset]) )
        return 0.0;
    return Info->cost_map[cm_VIS][offset];
}

inline double get_battle_val(int offset, struct game_info *Info)
{
    return Info->cost_map[cm_BATTLE][offset];
}

inline int near_home(int offset, struct game_info *Info)
{
    int r,c;
    AT_INDEX(r,c,offset);
    int h;
    for(h=0;h<Info->Game->my_hill_count;h++){
        struct my_ant *e = &Info->Game->my_hills[h];
        if( edist_sq(e->row,e->col,r,c,Info) < NEAR_HOME_DIST_SQ )
            return 1;
    }
    return 0;
}

inline double get_defense_val(int offset, struct game_info *Info)
{
    if( IS_WATER(Info->map[offset]) )
        return 0.0;
    if( IS_MY_ANT(Info->map[offset]) )
        return 0.0;
    if( IS_ENEMY_ANT(Info->map[offset]) && near_home(offset,Info) )
        return 1.0;
    return Info->cost_map[cm_DEFENSE][offset];
}

struct diffusion_params {
    double dx,dy;
    double dx2,dy2;
    double dt[cm_TOTAL];
    int configured;
};
inline void diffuse_step(int i, int j, struct game_info *Info, struct diffusion_params *dp)
{
    if(!dp->configured){
        int x;
        dp->dx = 1.0/Info->rows;
        dp->dy = 1.0/Info->cols;
        dp->dx2 = dp->dx*dp->dx;
        dp->dy2 = dp->dy*dp->dy;
        for(x=0;x<cm_TOTAL;x++)
            dp->dt[x] = dp->dx2*dp->dy2/( 2*alpha[x]*(dp->dx2+dp->dy2) );
        dp->configured = 1;
    }
    double uxx,uyy;
    int offset = INDEX_AT(i,j);
    int _n=NORTH(i,j),_e=EAST(i,j),_s=SOUTH(i,j),_w=WEST(i,j);

    /* FOOD */
    uxx = ( get_food_val(_n,Info) - 2*get_food_val(offset,Info) + get_food_val(_s,Info) )/dp->dx2;
    uyy = ( get_food_val(_w,Info) - 2*get_food_val(offset,Info) + get_food_val(_e,Info) )/dp->dy2;
    Info->cost_map[cm_FOOD][offset] = get_food_val(offset,Info)+dp->dt[cm_FOOD]*alpha[cm_FOOD]*(uxx+uyy);
    /* HILL */
    uxx = ( get_hill_val(_n,Info) - 2*get_hill_val(offset,Info) + get_hill_val(_s,Info) )/dp->dx2;
    uyy = ( get_hill_val(_w,Info) - 2*get_hill_val(offset,Info) + get_hill_val(_e,Info) )/dp->dy2;
    Info->cost_map[cm_HILL][offset] = get_hill_val(offset,Info)+dp->dt[cm_HILL]*alpha[cm_HILL]*(uxx+uyy);
    /* UNSEEN */
    uxx = ( get_unseen_val(_n,Info) - 2*get_unseen_val(offset,Info) + get_unseen_val(_s,Info) )/dp->dx2;
    uyy = ( get_unseen_val(_w,Info) - 2*get_unseen_val(offset,Info) + get_unseen_val(_e,Info) )/dp->dy2;
    Info->cost_map[cm_UNSEEN][offset] = get_unseen_val(offset,Info)+dp->dt[cm_UNSEEN]*alpha[cm_UNSEEN]*(uxx+uyy);
    /* VISIBILITY */
#ifdef DIFFUSE_VIS
    uxx = ( get_visibility_val(_n,Info) - 2*get_visibility_val(offset,Info) + get_visibility_val(_s,Info) )/dp->dx2;
    uyy = ( get_visibility_val(_w,Info) - 2*get_visibility_val(offset,Info) + get_visibility_val(_e,Info) )/dp->dy2;
    Info->cost_map[cm_VIS][offset] = get_visibility_val(offset,Info)+dp->dt[cm_VIS]*alpha[cm_VIS]*(uxx+uyy);
#endif
    /* DEFENSE */
#if 1
    uxx = ( get_defense_val(_n,Info) - 2*get_defense_val(offset,Info) + get_defense_val(_s,Info) )/dp->dx2;
    uyy = ( get_defense_val(_w,Info) - 2*get_defense_val(offset,Info) + get_defense_val(_e,Info) )/dp->dy2;
    Info->cost_map[cm_DEFENSE][offset] = get_defense_val(offset,Info)+dp->dt[cm_DEFENSE]*alpha[cm_DEFENSE]*(uxx+uyy);
#endif
    /* BATTLE */
#ifdef DIFFUSE_BATTLE
    uxx = ( get_battle_val(_n,Info) - 2*get_battle_val(offset,Info) + get_battle_val(_s,Info) )/dp->dx2;
    uyy = ( get_battle_val(_w,Info) - 2*get_battle_val(offset,Info) + get_battle_val(_e,Info) )/dp->dy2;
    Info->cost_map[cm_BATTLE][offset] = get_battle_val(offset,Info)+dp->dt[cm_BATTLE]*alpha[cm_BATTLE]*(uxx+uyy);
#endif
}

/* */
void diffuse_cost_map(struct game_state *Game, struct game_info *Info)
{
    int i,j;

	static struct timeval t_start;
	struct timeval t_end;
	gettimeofday(&t_start,NULL);
    { /* Special stuff for battle */
        double next_attack = sqrt(Info->attackradius_sq) + 1.0;
        int ec,mc;
        /* Build mapping of who can attack whom next round */
        /* TODO: cleanup this loop. loop over neighbors of enemy w/ smaller
         * attack raduis for better accuracy around water. */
        for(ec=0;ec<Game->enemy_count;ec++) {
            struct basic_ant *e = &Game->enemy_ants[ec];
            int eoffset = INDEX_AT(e->row,e->col);
            for(mc=0;mc<Game->my_count;mc++) {
                struct my_ant *m = &Game->my_ants[mc];
                int moffset = INDEX_AT(m->row,m->col);
                int n;
                for(n=0;n<5;n++) {
                    int noffset;
                    get_neighbor(Info, n, moffset, &noffset,NULL);
                    int nl_r, nl_c;
                    AT_INDEX(nl_r,nl_c,noffset);
                    if( edist(e->row,e->col,nl_r,nl_c,Info) < next_attack ) {
                        struct attackers_t * atk;
                        atk = &Info->attacked_by[eoffset];
                        atk->atkrs[atk->atkr_count++] = moffset;
                        atk = &Info->attacked_by[moffset];
                        atk->atkrs[atk->atkr_count++] = eoffset;
                        break;
                    }
                }
            }
        }

        int rad = 2*ceil(next_attack);
        rad+=2;
        for(ec=0;ec<Game->enemy_count;ec++) {
            struct basic_ant *e = &Game->enemy_ants[ec];
            double battle_val = attack_this_enemy(e,Info);
            LOG("attack enemy (%d,%d) score %lf\n", e->row, e->col, battle_val);
            double fill_circle = next_attack;
            for(i=-rad;i<=rad;i++) {
                for(j=-rad;j<=rad;j++) {
                    int r = WRAP_R(i+e->row);
                    int c = WRAP_C(j+e->col);
                    int offset = INDEX_AT(r,c);
                    double d =edist(e->row,e->col,r,c,Info);
                    if(d < fill_circle ) {
                        Info->cost_map[cm_BATTLE][offset]+=fill_circle*fill_circle*battle_val/d;
                    }
                }
            }
        }
    }
	gettimeofday(&t_end,NULL);
    LOG("Battle calc: %lf\n",timevaldiff(&t_start,&t_end));

	gettimeofday(&t_start,NULL);
    {/*build visibility table*/
        double fill_circle = sqrt(Info->viewradius_sq);
        int rad = 2*ceil(fill_circle);
        int mc;
        rad+=1;
        /* For each possible move mark if increases visibility */
        for(mc=0;mc<Game->my_count;mc++) {
            struct my_ant *m = &Game->my_ants[mc];
            int moffset = INDEX_AT(m->row,m->col);
            int n;
            int cur_vis=0;
            /* Tally how many cells are visibile by just me*/
            for(i=-rad;i<=rad;i++) {
                for(j=-rad;j<=rad;j++) {
                    int r = WRAP_R(i+m->row);
                    int c = WRAP_C(j+m->col);
                    int offset = INDEX_AT(r,c);
                    if(edist_sq(m->row,m->col,r,c,Info) < Info->viewradius_sq &&
                       Info->vis_tmp[offset]==1)
                        cur_vis+=1;
                }
            }
            for(n=0;n<4;n++) {
                int noffset;
                get_neighbor(Info, n, moffset, &noffset,NULL);
                int nl_r,nl_c;
                AT_INDEX(nl_r,nl_c,noffset);
                int next_vis=0;
                /* Tally how many cells are visibile noone but will be visible with move to nl */
                for(i=-rad;i<=rad;i++) {
                    for(j=-rad;j<=rad;j++) {
                        int r = WRAP_R(i+nl_r);
                        int c = WRAP_C(j+nl_c);
                        int offset = INDEX_AT(r,c);
                        if(edist_sq(nl_r,nl_c,r,c,Info) < Info->viewradius_sq &&
                           Info->vis_tmp[offset]<=1)
                            next_vis+=1;
                    }
                }
                Info->cost_map[cm_VIS][noffset] = cur_vis<next_vis?1.0:0.0;
                //LOG("Vis(%d,%d): %d -> %d: %lf\n", nl.row,nl.col,cur_vis, next_vis, Info->cost_map[cm_VIS][noffset]);
            }
        }
    }
	gettimeofday(&t_end,NULL);
    LOG("Vis calc: %lf\n",timevaldiff(&t_start,&t_end));

    int full_pass;
    struct diffusion_params dp={0};
	gettimeofday(&t_start,NULL);
    for(full_pass=0;full_pass<10;full_pass++) {
        /* Diffuse half pass 1 */
        for(i=0;i<Info->rows;i++)
            for(j=0;j<Info->cols;j++)
                diffuse_step(i,j,Info,&dp);
        /* Diffuse half pass 2 */
        for(i=Info->rows-1;i>=0;i--)
            for(j=Info->cols-1;j>=0;j--)
                diffuse_step(i,j,Info,&dp);
    }
	gettimeofday(&t_end,NULL);
    LOG("Diffuse calc: %lf\n",timevaldiff(&t_start,&t_end));
    /* Restore all items to their non-defused maxes */
    int defenders = min(4,Game->my_hill_count?(Game->my_count / (ANTS_PER_DEFENDER*Game->my_hill_count)):0);
	LOG("Defenders: %d (%d/%d)\n", defenders, Game->my_count, ANTS_PER_DEFENDER * Game->my_hill_count);
    for(i=0;i<Info->rows;i++)
        for(j=0;j<Info->cols;j++) {
            int offset = INDEX_AT(i,j);
            if(IS_ENEMY_HILL(Info->map[offset]))
                Info->cost_map[cm_HILL][offset] = 1.0;
            if(IS_MY_HILL(Info->map[offset])) {
                Info->cost_map[cm_HILL][offset] = -1.0;
#if DEFEND_HILL
                int d;
                const int dmap[4][2] = {{-1,-1},{-1,1},{1,1},{1,-1}};
                // mark up to these 4 defender squares as defence.
                //            [100 ___ 100]
                //            [___  0  ___]
                //            [100 ___ 100]
                for(d=0;d<defenders;d++) {
                    int r,c;
                    AT_INDEX(r,c,offset);
                    r = WRAP_R(r+dmap[d][0]);
                    c = WRAP_C(c+dmap[d][1]);
                    int doffset = INDEX_AT(r,c);
                    Info->cost_map[cm_DEFENSE][doffset] = DEFEND_HILL_SCORE;
                }
#endif
            }
        }
}

#if BFS_ENABLE
void bfs_cost_map(struct game_state *Game, struct game_info *Info)
{
    int i;
    int max_possible_dist = Info->rows+Info->cols; // :TODO: fix this
    struct timeval t_start, t_end;
    gettimeofday(&t_start,NULL);
    // Push all starting points onto the queue with scores:
    if(Info->game_strategy != st_ENDGAME)
        for (i = 0; i < Game->food_count; ++i) {
            struct q_data_t l;
            l.offset = INDEX_AT(Game->food[i].row,Game->food[i].col);
            l.distance = 0;
            queue_push(&bfs_queue,&l);
            set_insert(&bfs_seen,l.offset);
        }
    for (i = 0; i < Game->enemy_hill_count; ++i) {
        struct q_data_t l;
        l.offset = INDEX_AT(Game->enemy_hills[i].row,Game->enemy_hills[i].col);
        l.distance = 0;
        queue_push(&bfs_queue,&l);
        set_insert(&bfs_seen,l.offset);
    }
    LOG("Put %d items into bfs_queue\n", Game->food_count + Game->enemy_hill_count);

    while(!queue_is_empty(&bfs_queue)) {
        struct q_data_t l;
        queue_pop(&bfs_queue,(void*)&l);
        set_insert(&bfs_seen,l.offset);
        int offset = l.offset;
        int j=0,ndistance=l.distance+1;
        Info->cost_map[cm_BFS][offset] = (double)max(0,(max_possible_dist - l.distance)) / (double)max_possible_dist;
        for(j=0;j<4;j++) { //For each neighbor
            int noffset;
            get_neighbor(Info, j, offset, &noffset,NULL);
            if(IS_WATER(Info->map[noffset])) continue; // ... passable neighbor
            int nl_r,nl_c;
            AT_INDEX(nl_r,nl_c,noffset);

            if( !set_is_member(&bfs_seen,noffset) ) {
                l.offset = noffset;
                l.distance=ndistance;
                queue_push(&bfs_queue,&l);
                set_insert(&bfs_seen,noffset);
            }
        }
    }
    gettimeofday(&t_end,NULL);
    LOG("BFS calc: %lf\n",timevaldiff(&t_start,&t_end));
}
#endif

#if USE_MOMENTUM
inline char backwards(char d){
        switch(d){
            case 'n': return 's';
            case 'e': return 'w';
            case 's': return 'n';
            case 'w': return 'e';
            case 'N': return 'S';
            case 'E': return 'W';
            case 'S': return 'N';
            case 'W': return 'E';
            //case 'X': return 'X';
        }
        return d;
}
inline double calc_momentum(struct game_info *Info,int offset, char d) {
    char od = Info->momentum[1-Info->cur_momentum_buf][offset];
    if(od==d) return 1.0;
    if(od==backwards(d)) return 0.0;
    //:TODO: .66 for 90degrees off, .33 for stayput and was moving?
    return 0.5;
}
#endif

inline double calc_score(struct game_info *Info, int offset, int noffset, char d)
{
    double score = 0.0;
    double weight_total=0.0;
    int i;
    for(i=0;i<cm_TOTAL;i++) {
        //if(im_dead() && i==cm_FOOD) continue;
        weight_total += weights[Info->game_strategy][i];
        score += weights[Info->game_strategy][i] * Info->cost_map[i][noffset];
    }
    weight_total+=weights[Info->game_strategy][w_RAND];
    score += weights[Info->game_strategy][w_RAND]*rand()/(double)RAND_MAX;

#if USE_MOMENTUM
    weight_total+=weights[Info->game_strategy][w_MOMEN];
    score += weights[Info->game_strategy][w_MOMEN]*calc_momentum(Info,offset,d);
#endif

    return score / weight_total;
}

#if DEBUG
static void print_scores(struct game_info *Info,int offset,int noffset, char d, int raw)
{
    double weight_total=0.0;
    int i;
    for(i=0;i<cm_TOTAL;i++)
        weight_total += weights[Info->game_strategy][i];
    weight_total+=weights[Info->game_strategy][w_RAND];
#if USE_MOMENTUM
    weight_total+=weights[Info->game_strategy][w_MOMEN];
#endif
    for(i=0;i<cm_TOTAL;i++) {
        double w = raw?1.0:weights[Info->game_strategy][i];
        double wt = raw?1.0:weight_total;
        LOG("%c%f ", weight_labels[i], w * Info->cost_map[i][noffset] / wt);
    }
#if USE_MOMENTUM
    {
        double w = raw?1.0:weights[Info->game_strategy][w_MOMEN];
        double wt = raw?1.0:weight_total;
        LOG("%c%f",weight_labels[w_MOMEN], w*calc_momentum(Info,offset,d)/wt);
    }
#endif
}

static void render_plots(struct game_info *Info)
{
    if(!debug_on) return;
    int i,j;
    for(i=0;i<Info->rows;i++) {
# if (PLOT_DUMP&PLT_FOOD)
        fprintf(stderr,"plt food %03d: ", i);
        for(j=0;j<Info->cols;j++)
            fprintf(stderr,"%lf ", Info->cost_map[cm_FOOD][INDEX_AT(i,j)]);
        fprintf(stderr,"\n");
# endif
# if (PLOT_DUMP&PLT_HILL)
        fprintf(stderr,"plt hill %03d: ", i);
        for(j=0;j<Info->cols;j++)
            fprintf(stderr,"%lf ", Info->cost_map[cm_HILL][INDEX_AT(i,j)]);
        fprintf(stderr,"\n");
# endif
# if (PLOT_DUMP&PLT_UNSEEN)
        fprintf(stderr,"plt unseen %03d: ", i);
        for(j=0;j<Info->cols;j++)
            fprintf(stderr,"%lf ", Info->cost_map[cm_UNSEEN][INDEX_AT(i,j)]);
        fprintf(stderr,"\n");
# endif
# if (PLOT_DUMP&PLT_SCORE)
        fprintf(stderr,"plt score %03d: ", i);
        for(j=0;j<Info->cols;j++)
            fprintf(stderr,"%lf ", calc_score(Info,INDEX_AT(i,j),INDEX_AT(i,j),'X'));
        fprintf(stderr,"\n");
# endif
# if (PLOT_DUMP&PLT_BATTLE)
        fprintf(stderr,"plt battle %03d: ", i);
        for(j=0;j<Info->cols;j++)
            fprintf(stderr,"%lf ", Info->cost_map[cm_BATTLE][INDEX_AT(i,j)]);
        fprintf(stderr,"\n");
# endif
# if (PLOT_DUMP&PLT_VISION)
        fprintf(stderr,"plt vision %03d: ", i);
        for(j=0;j<Info->cols;j++)
            fprintf(stderr,"%lf ", Info->cost_map[cm_VIS][INDEX_AT(i,j)]);
            //fprintf(stderr,"%d.0 ", Info->vis_tmp[INDEX_AT(i,j)]);
        fprintf(stderr,"\n");
# endif
# if (PLOT_DUMP&PLT_DEFENSE)
        fprintf(stderr,"plt defense %03d: ", i);
        for(j=0;j<Info->cols;j++)
            fprintf(stderr,"%lf ", Info->cost_map[cm_DEFENSE][INDEX_AT(i,j)]);
        fprintf(stderr,"\n");
# endif
# if (PLOT_DUMP&PLT_BFS)
        fprintf(stderr,"plt bfs %03d: ", i);
        for(j=0;j<Info->cols;j++)
            fprintf(stderr,"%lf ", Info->cost_map[cm_BFS][INDEX_AT(i,j)]);
        fprintf(stderr,"\n");
# endif
    }
}
#endif

void do_turn(struct game_state *Game, struct game_info *Info)
{
	static int turn_count = 0;
    struct sigaction act;
    memset(&act,0,sizeof(act));
    act.sa_handler = timeout;
    sigaction (SIGALRM, & act, 0);

    int i;

	turn_count++;
	static struct timeval t_start;
	struct timeval t_end;
	gettimeofday(&t_end,NULL);
	LOG("--------------------------------------%d--------------------------------------%lf\n",
        turn_count, timevaldiff(&t_end,&t_start));
	t_start=t_end;
#ifdef TIMEOUT_PROTECTION
    if (setjmp(buf) != 0)
        goto turn_done;
    useconds_t u = ualarm( 1000*(Info->turntime-20),0);
    u=u; // Unused var warning
#endif

    /// UPDATE STRATEGY MODE
    if(turn_count>STRATEGY_STARTGAME_TURNS &&
       Info->game_strategy == st_STARTGAME)
        Info->game_strategy = st_MIDGAME;
    if(Game->my_hill_count == 0 &&
       Info->game_strategy == st_MIDGAME)
        Info->game_strategy = st_ENDGAME;

    prepare_next_turn(Info);
    need_reset = 1;
	diffuse_cost_map(Game,Info);
    bfs_cost_map(Game, Info);

    for (i = 0; i < Game->my_count; ++i) {
        struct my_ant * m = &Game->my_ants[i];
        int offset = INDEX_AT(m->row,m->col);

        // Find min neighbor in cost_map
        double max_score=-INFINITY;
        char dir = -1;
        char d = -1;
        int n;
        for(n=4;n>=0;n--) {
            int noffset;
            get_neighbor(Info, n, offset, &noffset,&d);
            int nl_r,nl_c;
            AT_INDEX(nl_r,nl_c,noffset);
            // Don't double move to a square, or to into water
            if(IS_WATER(Info->map[noffset]) || (Info->map[noffset] & MOVE_BIT)) continue;
            double score = calc_score(Info,offset,noffset,d);
#ifdef DEBUG
            LOG("  ?move (%d,%d)->(%d,%d) %lf [",
                m->row,m->col, nl_r,nl_c, score);
            print_scores(Info,offset,noffset,d,0);
            LOG("] %c\n", d );
            LOG("                                  [");
            print_scores(Info,offset,noffset,d,1);
            LOG("] %c\n", d );
#endif

            if(score>max_score) {
                max_score=score;
                dir = d;
            }
        }

        if (dir == -1 || dir == 'X') {
            Info->map[offset] |= MOVE_BIT; // if we don't move this ant mark it as staying
            LOG("Moved (%d,%d) STAY PUT %lf\n", m->row,m->col, max_score);
        }else {
            move(i, dir, Game, Info);
            LOG("Moved (%d,%d) %c %lf\n", m->row,m->col, dir, max_score);
        }
    }

	//render_map(Info);
#if PLOT_DUMP
    render_plots(Info);
#endif

    // Start preperation of next turn now, better to finsh now if we have time
    // otherwise will do start of next turn.
    prepare_next_turn(Info);
#ifdef TIMEOUT_PROTECTION
    u = ualarm(0,0);
    LOG(" ---- %dus remaining on timers\n", u);
turn_done:
#endif
	return;
}

