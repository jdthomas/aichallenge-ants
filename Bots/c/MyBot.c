#include "ants.h"

//#define PLOT_DUMP 1
//#define DIFFUSE_BATTLE
//#ifdef TIMEOUT_PROTECTION
//#define DIFFUSE_VIS

#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <unistd.h>

#define LOG(format,args...) fprintf(stderr,format,##args)
//#define LOG(format,args...) 

// TODO:
// 1. defence ... if my_count > 8/hill: assign closest ant to hill as defence
// 2. remember seen stuff until see the cell and it is gone
// 3. slight preference for momentum?
// 4. once dead, attack bases, ignore food.
// 5. sort my ants by degrees of freedom and move most constrained ones first?

static jmp_buf buf;

void timeout(int sig) {
    siglongjmp(buf, 1);
}

enum {
    cm_FOOD,
    cm_HILL,
    cm_UNSEEN,
    cm_VIS,
    cm_BATTLE,
    cm_TOTAL
};

#define MAX_ATTACKERS 50 /* FIXME: size of perimeter of attack radius */
struct attackers_t {
	int atkr_count;
	int atkrs[MAX_ATTACKERS];
};

const double weights[cm_TOTAL] = {
    [cm_FOOD]   =  1.0,
    [cm_HILL]   =  2.0,
    [cm_UNSEEN] =  0.000125,
    [cm_VIS]    =  0.000125,
    [cm_BATTLE] =  5.0,
    //[cm_RAND]   = 1.0e-10
};
#if 0
const double weights[cm_TOTAL] = {
    [cm_FOOD]   =  10.0,
    [cm_HILL]   =  20.0,
    [cm_UNSEEN] =   0.25,
    [cm_VIS]    =   0.0125,
    [cm_BATTLE] = 100.0,
    //[cm_RAND]   = 1.0e-10
};
#endif
double * cost_map[cm_TOTAL];
struct attackers_t *attacked_by=NULL;
int *vis_tmp=NULL;
static int need_reset = 1;

double timevaldiff(struct timeval *starttime, struct timeval *finishtime)
{
  double msec;
  msec=(finishtime->tv_sec-starttime->tv_sec)*1000;
  msec+=(finishtime->tv_usec-starttime->tv_usec)/1000.0;
  return msec;
}
#if 1 // math.h has this
// returns the absolute value of a number; used in distance function

int abs(int x) {
    if (x >= 0)
        return x;
    return -x;
}
#endif


// returns the distance between two items on the grid accounting for map wrapping

float distance(int row1, int col1, int row2, int col2, struct game_info *Info) {
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

	return sqrt(pow(dr, 2) + pow(dc, 2));
}

// sends a move to the tournament engine and keeps track of ants new location

void move(int index, char dir, struct game_state* Game, struct game_info* Info) {
    fprintf(stdout, "O %i %i %c\n", Game->my_ants[index].row, Game->my_ants[index].col, dir);
	int r = Game->my_ants[index].row;
	int c = Game->my_ants[index].col;

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

    fprintf(stderr, "O %i %i %c -> %i %i\n", r,c,dir,Game->my_ants[index].row, Game->my_ants[index].col);
	Info->map[Game->my_ants[index].row*Info->cols+Game->my_ants[index].col] = MOVE_OFFSET;
}

// just a function that returns the string on a given line for i/o
// you don't need to worry about this

char *get_line(char *text) {
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

int main(int argc, char *argv[]) {
    int action = -1;

    struct game_info Info;
    struct game_state Game;
    Info.map = 0;

    Game.my_ants = 0;
    Game.enemy_ants = 0;
    Game.food = 0;
    Game.dead_ants = 0;

	freopen("/tmp/MyBot_c.log","wa+",stderr);

    while (42) {
        int initial_buffer = 100000;

        char *data = malloc(initial_buffer);
        memset(data, 0, initial_buffer);

        *data = '\n';

        char *ins_data = data + 1;

        int i = 0;

        while (1 > 0) {
            ++i;

            if (i > initial_buffer) {
                initial_buffer *= 2;
                data = realloc(data, initial_buffer);
                memset(ins_data, 0, initial_buffer/2);
            }

            *ins_data = getchar();

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


void prepare_next_turn(struct game_info *Info, double ** cost_map)
{
    static int done_allocations=0;
    int i;
    if(!need_reset) return;
    if(!done_allocations) {
        for(i=0;i<cm_TOTAL;i++)
            if(!cost_map[i])
                cost_map[i] = calloc(1,sizeof(double)*Info->rows*Info->cols);
        if(!attacked_by)
            attacked_by = malloc(Info->rows*Info->cols*sizeof(struct attackers_t));
        if(!vis_tmp)
            vis_tmp = malloc(Info->rows*Info->cols*sizeof(int));
        done_allocations=1;
    }
    memset(attacked_by, 0, Info->rows*Info->cols*sizeof(struct attackers_t));
    memset(vis_tmp, 0, Info->rows*Info->cols*sizeof(int));
    memset(cost_map[cm_BATTLE],0,Info->rows*Info->cols*sizeof(double));
    memset(cost_map[cm_VIS],0,Info->rows*Info->cols*sizeof(double));
    need_reset = 0;
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

double attack_this_enemy(struct basic_ant * e, struct game_info *Info)
{
	int eoffset = INDEX_AT(e->row,e->col);
	int i;
	int win =0, loss=0;
	//if( !attacked_by[eoffset].atkr_count ) return 0.0; // This guy is not near anyone
	for(i=0;i<attacked_by[eoffset].atkr_count;i++) { // For each ant this enemy can attack...
		int moffset = attacked_by[eoffset].atkrs[i];
		if( attacked_by[eoffset].atkr_count >
			attacked_by[moffset].atkr_count )
			win++;
		if( attacked_by[eoffset].atkr_count <
			attacked_by[moffset].atkr_count )
			loss++;
#if 0
		//// Dump
		{
			int er,ec,ev, mr,mc,mv;
			AT_INDEX(er,ec,eoffset); ev=attacked_by[eoffset].atkr_count;
			AT_INDEX(mr,mc,moffset); mv=attacked_by[moffset].atkr_count;
			LOG("(%d,%d)[%d] attacks (%d,%d)[%d] w=%d l=%d\n", er,ec,ev, mr,mc,mv, win,loss );
		}
#endif
	}
	if( win ) return 1.0;
	return -1.0;
}

/* */
inline double get_food_val(int offset, double **cost_map, struct game_info *Info)
{
    if( IS_FOOD(Info->map[offset]) )
        return 1.0;
    if( IS_WATER(Info->map[offset]) || 
        IS_MY_ANT(Info->map[offset]) )
        return 0.0;
    return cost_map[cm_FOOD][offset];
}

inline double get_hill_val(int offset, double **cost_map, struct game_info *Info)
{
    if( IS_ENEMY_HILL(Info->map[offset]) )
        return 1.0;
    if( IS_WATER(Info->map[offset]) )
        return 0.0; 
    if ( IS_MY_ANT(Info->map[offset]) )
        return cost_map[cm_HILL][offset]*0.5;  // I want to attack hills in numbers, diffuse partially through my ants
    return cost_map[cm_FOOD][offset];
}

inline double get_unseen_val(int offset, double **cost_map, struct game_info *Info)
{
    if( IS_UNSEEN(Info->map[offset]) )
        return 1.0;
    if( IS_WATER(Info->map[offset]) || 
        IS_MY_ANT(Info->map[offset]) )
        return 0.0; 
    return cost_map[cm_UNSEEN][offset];
}

inline double get_visibility_val(int offset, double **cost_map, struct game_info *Info)
{
    if( IS_WATER(Info->map[offset]) )
        return 0.0; 
    if( IS_MY_ANT(Info->map[offset]) )
        return 0.0;
    return cost_map[cm_VIS][offset];
}

inline double get_battle_val(int offset, double **cost_map, struct game_info *Info)
{
    return cost_map[cm_BATTLE][offset];
}

inline void diffuse_step(int i, int j, struct game_info *Info, double **cost_map)
{
    /* some game specific constants for diffusion */
    double dx = 1.0/Info->rows, dy = 1.0/Info->cols;
    double dx2 = dx*dx, dy2 = dy*dy;
    double a = 0.2;
    double dt = dx2*dy2/( 2*a*(dx2+dy2) );

    double uxx,uyy;
    int offset = INDEX_AT(i,j);
    int _n=NORTH(i,j),_e=EAST(i,j),_s=SOUTH(i,j),_w=WEST(i,j);

    /* FOOD */
    uxx = ( get_food_val(_n,cost_map,Info) - 2*get_food_val(offset,cost_map,Info) + get_food_val(_s,cost_map,Info) )/dx2;
    uyy = ( get_food_val(_w,cost_map,Info) - 2*get_food_val(offset,cost_map,Info) + get_food_val(_e,cost_map,Info) )/dy2;
    cost_map[cm_FOOD][offset] = get_food_val(offset,cost_map,Info)+dt*a*(uxx+uyy);
    /* HILL */
    uxx = ( get_hill_val(_n,cost_map,Info) - 2*get_hill_val(offset,cost_map,Info) + get_hill_val(_s,cost_map,Info) )/dx2;
    uyy = ( get_hill_val(_w,cost_map,Info) - 2*get_hill_val(offset,cost_map,Info) + get_hill_val(_e,cost_map,Info) )/dy2;
    cost_map[cm_HILL][offset] = get_hill_val(offset,cost_map,Info)+dt*a*(uxx+uyy);
    /* UNSEEN */
    uxx = ( get_unseen_val(_n,cost_map,Info) - 2*get_unseen_val(offset,cost_map,Info) + get_unseen_val(_s,cost_map,Info) )/dx2;
    uyy = ( get_unseen_val(_w,cost_map,Info) - 2*get_unseen_val(offset,cost_map,Info) + get_unseen_val(_e,cost_map,Info) )/dy2;
    cost_map[cm_UNSEEN][offset] = get_unseen_val(offset,cost_map,Info)+dt*a*(uxx+uyy);
    /* VISIBILITY */
#ifdef DIFFUSE_VIS
    uxx = ( get_visibility_val(_n,cost_map,Info) - 2*get_visibility_val(offset,cost_map,Info) + get_visibility_val(_s,cost_map,Info) )/dx2;
    uyy = ( get_visibility_val(_w,cost_map,Info) - 2*get_visibility_val(offset,cost_map,Info) + get_visibility_val(_e,cost_map,Info) )/dy2;
    cost_map[cm_VIS][offset] = get_visibility_val(offset,cost_map,Info)+dt*a*(uxx+uyy);
#endif
    /* BATTLE */
#ifdef DIFFUSE_BATTLE
    uxx = ( get_battle_val(_n,cost_map,Info) - 2*get_battle_val(offset,cost_map,Info) + get_battle_val(_s,cost_map,Info) )/dx2;
    uyy = ( get_battle_val(_w,cost_map,Info) - 2*get_battle_val(offset,cost_map,Info) + get_battle_val(_e,cost_map,Info) )/dy2;
    cost_map[cm_BATTLE][offset] = get_battle_val(offset,cost_map,Info)+dt*a*(uxx+uyy);
#endif
}

/* */
void diffuse_cost_map(struct game_state *Game, struct game_info *Info, double** cost_map)
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
                    if( distance(e->row,e->col,nl_r,nl_c,Info) < next_attack ) {
                        struct attackers_t * atk;
                        atk = &attacked_by[eoffset];
                        atk->atkrs[atk->atkr_count++] = moffset;
                        atk = &attacked_by[moffset];
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
                    double d =distance(e->row,e->col,r,c,Info);
                    if(d < fill_circle ) {
                        cost_map[cm_BATTLE][offset]+=fill_circle*fill_circle*battle_val/d;
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
        /* Tally visibility squares in vis_tmp */
        for(mc=0;mc<Game->my_count;mc++) {
            struct my_ant *m = &Game->my_ants[mc];
            //int moffset = INDEX_AT(m->row,m->col);
            for(i=-rad;i<=rad;i++) {
                for(j=-rad;j<=rad;j++) {
                    int r = WRAP_R(i+m->row);
                    int c = WRAP_C(j+m->col);
                    int offset = INDEX_AT(r,c);
                    double d =distance(m->row,m->col,r,c,Info);
                    if(d < fill_circle ) {
                        vis_tmp[offset]+=1;
                    }
                }
            }
        }

        struct timeval t_mid;
        gettimeofday(&t_mid,NULL);
        LOG("Vis calc mid: %lf\n",timevaldiff(&t_start,&t_mid));

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
                    double d =distance(m->row,m->col,r,c,Info);
                    if(d < fill_circle && vis_tmp[offset]==1) {
                        cur_vis+=1;
                    }
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
                        double d =distance(nl_r,nl_c,r,c,Info);
                        if(d < fill_circle && vis_tmp[offset]<=1) {
                            next_vis+=1;
                        }
                    }
                }
                cost_map[cm_VIS][noffset] = cur_vis<next_vis?1.0:0.0;
                //LOG("Vis(%d,%d): %d -> %d: %lf\n", nl.row,nl.col,cur_vis, next_vis, cost_map[cm_VIS][noffset]);
            }
        }
    }
	gettimeofday(&t_end,NULL);
    LOG("Vis calc: %lf\n",timevaldiff(&t_start,&t_end));

    int full_pass;
    for(full_pass=0;full_pass<10;full_pass++) {
        /* Diffuse half pass 1 */
        for(i=0;i<Info->rows;i++)
            for(j=0;j<Info->cols;j++)
                diffuse_step(i,j,Info,cost_map);
        /* Diffuse half pass 2 */
        for(i=Info->rows-1;i>=0;i--)
            for(j=Info->cols-1;j>=0;j--)
                diffuse_step(i,j,Info,cost_map);
    }
    /* Restore all items to their non-defused maxes */
    for(i=0;i<Info->rows;i++)
        for(j=0;j<Info->cols;j++) {
            int offset = INDEX_AT(i,j);
            if(IS_ENEMY_HILL(Info->map[offset]))
                cost_map[cm_HILL][offset] = 1.0;
            if(IS_MY_HILL(Info->map[offset]))
                cost_map[cm_HILL][offset] = -1.0;
        }
}

inline double calc_score(double ** cost_map, int offset)
{
    double score = 0.0;
    double weight_total=0.0;
    int i;
    for(i=0;i<cm_TOTAL;i++) {
        //if(im_dead() && i==cm_FOOD) continue;
        weight_total += weights[i];
        score += weights[i] * cost_map[i][offset];
    }
    return score / weight_total;
}

void print_scores(double ** cost_map,int offset)
{
    double weight_total=0.0;
    int i;
    for(i=0;i<cm_TOTAL;i++)
        weight_total += weights[i];
    for(i=0;i<cm_TOTAL;i++)
        LOG("%f,",weights[i] * cost_map[i][offset]/weight_total);
}

void render_plots(struct game_info *Info,double **cost_map)
{
#if PLOT_DUMP
    int i,j;
    for(i=0;i<Info->rows;i++) {
        fprintf(stderr,"plt fod %03d: ", i);
        for(j=0;j<Info->cols;j++)
            fprintf(stderr,"%lf ", cost_map[cm_FOOD][INDEX_AT(i,j)]);
        fprintf(stderr,"\n");
        fprintf(stderr,"plt hil %03d: ", i);
        for(j=0;j<Info->cols;j++)
            fprintf(stderr,"%lf ", cost_map[cm_HILL][INDEX_AT(i,j)]);
        fprintf(stderr,"\n");
        fprintf(stderr,"plt uns %03d: ", i);
        for(j=0;j<Info->cols;j++)
            fprintf(stderr,"%lf ", cost_map[cm_UNSEEN][INDEX_AT(i,j)]);
        fprintf(stderr,"\n");
        fprintf(stderr,"plt scr %03d: ", i);
        for(j=0;j<Info->cols;j++)
            fprintf(stderr,"%lf ", calc_score(cost_map,INDEX_AT(i,j)));
        fprintf(stderr,"\n");
        fprintf(stderr,"plt bat %03d: ", i);
        for(j=0;j<Info->cols;j++)
            fprintf(stderr,"%lf ", cost_map[cm_BATTLE][INDEX_AT(i,j)]);
        fprintf(stderr,"\n");
        fprintf(stderr,"plt vis %03d: ", i);
        for(j=0;j<Info->cols;j++)
            fprintf(stderr,"%lf ", cost_map[cm_VIS][INDEX_AT(i,j)]);
        fprintf(stderr,"\n");
    }
#endif
}

void do_turn(struct game_state *Game, struct game_info *Info)
{
	static int turn_count = 0;
    struct sigaction act;
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
    ualarm(Info->turntime-20,0);
#endif
	// :TODO: dynamic memory allocations for turn need to be carefully handled
	// so we do not leak in case of timeout. Timeout should be based on how
	// much time we have left now, not the total time .. eg. we should pass
	// down a DONE time from VERY start of turn.

    prepare_next_turn(Info, cost_map);
    need_reset = 1;
	diffuse_cost_map(Game,Info,cost_map);

	//render_map(Info);
#if PLOT_DUMP
    render_plots(Info,cost_map);
#endif

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
            if(IS_WATER(Info->map[noffset]) || Info->map[noffset] == MOVE_OFFSET) continue;
            double score = calc_score(cost_map,noffset);
            LOG("  ?move (%d,%d)->(%d,%d) %lf [",
                m->row,m->col, nl_r,nl_c, score);
            print_scores(cost_map,noffset);
            LOG("] %c\n", d );

            if(score>max_score) {
                max_score=score;
                dir = d;
            }
        }

        if (dir == -1 || dir == 'X') {
            Info->map[offset] = MOVE_OFFSET; // if we don't move this ant mark it as staying
            LOG("Moved (%d,%d) STAY PUT %lf\n", m->row,m->col, max_score);
        }else {
            move(i, dir, Game, Info);
            LOG("Moved (%d,%d) %c %lf\n", m->row,m->col, dir, max_score);
        }
    }

    // Start preperation of next turn now, better to finsh now if we have time
    // otherwise will do start of next turn.
    prepare_next_turn(Info, cost_map);
#ifdef TIMEOUT_PROTECTION
    ualarm(0,0);
turn_done:
#endif
	return;
}

