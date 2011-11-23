#include "ants.h"

//#define PLOT_DUMP 1
//#define DIFFUSE_BATTLE
//#ifdef TIMEOUT_PROTECTION
//#define DIFFUSE_VIS

#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <unistd.h>

#define LOG(format,args...) fprintf(stderr,format,##args)
//#define LOG(format,args...) 

// TODO:
// 1. defence ... if my_count > 8/hill: assign closest ant to hill as defence
// 2. remember seen stuff until see the cell and it is gone
// 3. slight preference for momentum?

static jmp_buf buf;

void timeout(int sig) {
    siglongjmp(buf, 1);
}

struct map_cell_t {
	double food;
	double hill;
	double unseen;
	double visibility;
	double friend;
	double battle;
};

#define MAX_ATTACKERS 50 /* FIXME: size of perimeter of attack radius */
struct attackers_t {
	int atkr_count;
	int atkrs[MAX_ATTACKERS];
};
struct attackers_t *attacked_by=NULL;
int *vis_tmp=NULL;

//#define ENEMY_ANT_SCORE 100000
//#define MY_ANT_SCORE -ENEMY_ANT_SCORE
#define FOOD_SCORE 1.0
#define HILL_SCORE 1.0
#define UNSEEN_SCORE 1.0

struct loc_t {
	int row, col;
};

int get_neighbor(struct game_info *Info, struct loc_t *cur_loc, int n, struct loc_t * new_loc,char *dir){
	int r=0;
	int c=0;
	char d=-1;
    switch(n){
		case 0://N
			r=-1;c=0;d='N';
			break;
		case 1://E
			r=0;c=1;d='E';
			break;
		case 2://S
			r=1;c=0;d='S';
			break;
		case 3://W
			r=0;c=-1;d='W';
			break;
		case 4://stay put
			r=0;c=0;d='X';
			break;
	}
	new_loc->row = (cur_loc->row+r+Info->rows)%Info->rows;
	new_loc->col = (cur_loc->col+c+Info->cols)%Info->cols;
	if(dir) *dir=d;
	//LOG("neighbor of (%d,%d): (%d,%d)\n", cur_loc->row, cur_loc->col, new_loc->row, new_loc->col);
	return 0;
}

#define INDEX_AT(r,c) (r*Info->cols+c)
#define AT_INDEX(r,c,offset) ({c=offset%Info->cols; r=(offset-c)/Info->cols;})

#define WRAP_R(r) ( (r+Info->rows)%Info->rows )
#define WRAP_C(c) ( (c+Info->cols)%Info->cols )

#define NORTH(r,c) INDEX_AT(WRAP_R(r-1),       c )
#define  EAST(r,c) INDEX_AT(       r   ,WRAP_C(c+1))
#define SOUTH(r,c) INDEX_AT(WRAP_R(r+1),       c )
#define  WEST(r,c) INDEX_AT(       r   ,WRAP_C(c-1))

static int need_reset = 1;
void prepare_next_turn(struct game_info *Info, struct map_cell_t * cost_map) {
    if(!need_reset) return;
	if( attacked_by == NULL )
		attacked_by = malloc(Info->rows*Info->cols*sizeof(struct attackers_t));
	if( vis_tmp == NULL )
		vis_tmp = malloc(Info->rows*Info->cols*sizeof(int));
    memset(attacked_by, 0, Info->rows*Info->cols*sizeof(struct attackers_t));
    memset(vis_tmp, 0, Info->rows*Info->cols*sizeof(int));
    int i,j;
    for(i=0;i<Info->rows;i++) {
        for(j=0;j<Info->cols;j++) {
            int offset = i*Info->cols + j;
            cost_map[offset].battle = 0.0;
            cost_map[offset].visibility=0.0;
        }
    }
    need_reset = 0;
}

double attack_this_enemy(struct basic_ant * e, struct game_info *Info) {
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

void di3ffuse_cost_map(struct game_state *Game, struct game_info *Info, struct map_cell_t * cost_map)
{

	int i,j;

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
				struct loc_t nl,l;
				l.row = m->row; l.col=m->col;
				int n;
				int once_per_en=1, once_per_my=1;
				for(n=0;n<5;n++) {
					get_neighbor(Info, &l, n, &nl,NULL);
					//int noffset = INDEX_AT(nl.row, nl.col);
					if( distance(e->row,e->col,nl.row,nl.col,Info) < next_attack ) {
						struct attackers_t * atk;
						if(once_per_en) { // enemy can attack this ant
                            atk = &attacked_by[eoffset];
							atk->atkrs[atk->atkr_count++] = moffset;
							once_per_en = 0;
						}
						if(once_per_my) { // enemy can attack this ant
                            atk = &attacked_by[moffset];
							atk->atkrs[atk->atkr_count++] = eoffset;
							once_per_my = 0;
						}
						// each neighbor position can attack this enemy
						//atk = &attacked_by[noffset];
						//atk->atkrs[atk->atkr_count++] = eoffset;
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
						cost_map[offset].battle+=fill_circle*fill_circle*battle_val/d;
					}
				}
			}
		}
	}

    {/*build visibility table*/
#if 0
        double fill_circle = sqrt(Info->viewradius_sq);
		int rad = 2*ceil(fill_circle);
        int mc;
		rad+=2;
        for(mc=0;mc<Game->my_count;mc++) {
            struct my_ant *m = &Game->my_ants[mc];
            int moffset = INDEX_AT(m->row,m->col);
			for(i=-rad;i<=rad;i++) {
				for(j=-rad;j<=rad;j++) {
					int r = WRAP_R(i+m->row);
					int c = WRAP_C(j+m->col);
					int offset = INDEX_AT(r,c);
                    double d =distance(m->row,m->col,r,c,Info);
					if(d < fill_circle ) {
						cost_map[offset].visibility -= 1.0 / Game->my_count;
					}
				}
			}
        }
#else
        double fill_circle = sqrt(Info->viewradius_sq);
		int rad = 2*ceil(fill_circle);
        int mc;
		rad+=2;
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
        /* For each possible move mark if increases visibility */
        for(mc=0;mc<Game->my_count;mc++) {
            struct my_ant *m = &Game->my_ants[mc];
            int moffset = INDEX_AT(m->row,m->col);
            int just_me=0;
            /* Tally how many cells are visibile by just me*/
            for(i=-rad;i<=rad;i++) {
                for(j=-rad;j<=rad;j++) {
                    int r = WRAP_R(i+m->row);
                    int c = WRAP_C(j+m->col);
                    int offset = INDEX_AT(r,c);
                    double d =distance(m->row,m->col,r,c,Info);
                    if(d < fill_circle && vis_tmp[offset]==1) {
                        just_me+=1;
                    }
                }
            }
            struct loc_t nl,l;
            l.row = m->row; l.col=m->col;
            int n;
            for(n=0;n<4;n++) {
                get_neighbor(Info, &l, n, &nl,NULL);
                int noffset = INDEX_AT(nl.row, nl.col);
                int just_new=0;
                /* Tally how many cells are visibile noone but will be visible with move to nl */
                for(i=-rad;i<=rad;i++) {
                    for(j=-rad;j<=rad;j++) {
                        int r = WRAP_R(i+nl.row);
                        int c = WRAP_C(j+nl.col);
                        int offset = INDEX_AT(r,c);
                        double d =distance(nl.row,nl.col,r,c,Info);
                        if(d < fill_circle && vis_tmp[offset]<=1) {
                            just_new+=1;
                        }
                    }
                }
                cost_map[noffset].visibility = just_me<just_new?1.0:0.0;
                //LOG("Vis(%d,%d): %d -> %d: %lf\n", nl.row,nl.col,just_me, just_new, cost_map[noffset].visibility);
            }
        }
#endif
    }

	inline double get_food_val(int offset) {
		if( IS_FOOD(Info->map[offset]) )
			return FOOD_SCORE;
		if( IS_WATER(Info->map[offset]) || 
			IS_MY_ANT(Info->map[offset]) )
			return 0.0;
		return cost_map[offset].food;
	}
	inline double get_hill_val(int offset) {
		if( IS_ENEMY_HILL(Info->map[offset]) )
			return HILL_SCORE;
		if( IS_WATER(Info->map[offset]) )
			return 0.0; 
        if ( IS_MY_ANT(Info->map[offset]) )
			return cost_map[offset].hill*0.5;  // I want to attack hills in numbers, diffuse partially through my ants
		return cost_map[offset].hill;
	}
	inline double get_unseen_val(int offset) {
		if( IS_UNSEEN(Info->map[offset]) )
			return UNSEEN_SCORE;
		if( IS_WATER(Info->map[offset]) || 
			IS_MY_ANT(Info->map[offset]) )
			return 0.0; 
		return cost_map[offset].unseen;
	}
	inline double get_friend_val(int offset) {
		if( IS_MY_ANT(Info->map[offset]) )
			return 1.0;
		if( IS_WATER(Info->map[offset]) )
			return 0.0; 
		return cost_map[offset].friend;
	}
	inline double get_visibility_val(int offset) {
		if( IS_WATER(Info->map[offset]) )
			return 0.0; 
		if( IS_MY_ANT(Info->map[offset]) )
			return 0.0;
		return cost_map[offset].visibility;
	}
	inline double get_battle_val(int offset) {
		return cost_map[offset].battle;
	}
	inline void diffuse_step(int i, int j){
		/* some game specific constants for diffusion */
		double dx = 1.0/Info->rows, dy = 1.0/Info->cols;
		double dx2 = dx*dx, dy2 = dy*dy;
		double a = 0.2;
		double dt = dx2*dy2/( 2*a*(dx2+dy2) );

		double uxx,uyy;
		int offset = INDEX_AT(i,j);
		int _n=NORTH(i,j),_e=EAST(i,j),_s=SOUTH(i,j),_w=WEST(i,j);

		/* FOOD */
		uxx = ( get_food_val(_n) - 2*get_food_val(offset) + get_food_val(_s) )/dx2;
		uyy = ( get_food_val(_w) - 2*get_food_val(offset) + get_food_val(_e) )/dy2;
		cost_map[offset].food = get_food_val(offset)+dt*a*(uxx+uyy);
		/* HILL */
		uxx = ( get_hill_val(_n) - 2*get_hill_val(offset) + get_hill_val(_s) )/dx2;
		uyy = ( get_hill_val(_w) - 2*get_hill_val(offset) + get_hill_val(_e) )/dy2;
		cost_map[offset].hill = get_hill_val(offset)+dt*a*(uxx+uyy);
		/* UNSEEN */
		uxx = ( get_unseen_val(_n) - 2*get_unseen_val(offset) + get_unseen_val(_s) )/dx2;
		uyy = ( get_unseen_val(_w) - 2*get_unseen_val(offset) + get_unseen_val(_e) )/dy2;
		cost_map[offset].unseen = get_unseen_val(offset)+dt*a*(uxx+uyy);
		/* VISIBILITY */
#ifdef DIFFUSE_VIS
		uxx = ( get_visibility_val(_n) - 2*get_visibility_val(offset) + get_visibility_val(_s) )/dx2;
		uyy = ( get_visibility_val(_w) - 2*get_visibility_val(offset) + get_visibility_val(_e) )/dy2;
		cost_map[offset].visibility = get_visibility_val(offset)+dt*a*(uxx+uyy);
#endif
		/* BATTLE */
#ifdef DIFFUSE_BATTLE
		uxx = ( get_battle_val(_n) - 2*get_battle_val(offset) + get_battle_val(_s) )/dx2;
		uyy = ( get_battle_val(_w) - 2*get_battle_val(offset) + get_battle_val(_e) )/dy2;
		cost_map[offset].battle = get_battle_val(offset)+dt*a*(uxx+uyy);
#endif
		//LOG("diffused @offset %d(%d,%d) to %d\n", offset, i, j, cost_map[offset].food);
	}

	int full_pass;
	for(full_pass=0;full_pass<10;full_pass++) {
		/* Diffuse half pass 1 */
		for(i=0;i<Info->rows;i++) {
			for(j=0;j<Info->cols;j++) {
				diffuse_step(i,j);
			}
		}
		/* Diffuse half pass 2 */
		for(i=Info->rows-1;i>=0;i--) {
			for(j=Info->cols-1;j>=0;j--) {
				diffuse_step(i,j);
			}
		}
	}
	/* Restore all items to their non-defused maxes */
	for(i=0;i<Info->rows;i++)
		for(j=0;j<Info->cols;j++) {
			int offset = INDEX_AT(i,j);
			if(IS_ENEMY_HILL(Info->map[offset]))
					cost_map[offset].hill = 1.0;
		}
	/* 	Reset all cost_map[offset].enemy squares to 0.0
	 * 	For each square an enemy can attack in the next round:
	 * 		if cost_map[offset].enemy <= 0.0: cost_map[offset].enemy+=0.5
	 * 		cost_map[offset].enemy+=1.0
	 * 	For each square an __I__ can attack in the next round:
	 * 		if cost_map[offset].enemy > 0.0: cost_map[offset].enemy -= 1.0
	 *	if cost_map[offset].ememy > 0: DEATH
	 *	if cost_map[offset].ememy < 0: WIN FIGHT // assuming all moves correct
	 *	if cost_map[offset].enemy <> 0: NO FIGHT, NO CARE
	 */
	/* Special case for visibility */
	/*
	 *  For each ant:
	 * 		For each neighbor:
	 * 			if increases visibility: 1.0
	 * 			else: 0.0
	 */
}

inline double calc_score(struct map_cell_t * cost_map, int offset) {
    //const double weights[] = {1.0,2.0,0.5,0.125,5.0,0.001};
    return (   10.0    * cost_map[offset].food
            +  20.0    * cost_map[offset].hill
            +   0.25   * cost_map[offset].unseen
            +   0.0125 * cost_map[offset].visibility
            + 100.0    * cost_map[offset].battle
              //+ 1.0e-10* (double)rand()/(double)RAND_MAX
           ) / 130.2625;
}

double timevaldiff(struct timeval *starttime, struct timeval *finishtime)
{
  double msec;
  msec=(finishtime->tv_sec-starttime->tv_sec)*1000;
  msec+=(finishtime->tv_usec-starttime->tv_usec)/1000.0;
  return msec;
}
struct map_cell_t *cost_map;
void do_turn(struct game_state *Game, struct game_info *Info) {
	static int turn_count = 0;
    struct sigaction act;
    act.sa_handler = timeout;
    sigaction (SIGALRM, & act, 0);

    int i;

	if(!cost_map) cost_map = calloc(1,sizeof(struct map_cell_t)*Info->rows*Info->cols);
	turn_count++;
	static struct timeval t_start;
	struct timeval t_end;
	gettimeofday(&t_end,NULL);
	LOG("--------------------------------------%d--------------------------------------%lf\n",turn_count, timevaldiff(&t_end,&t_start));
	t_start=t_end;
#ifdef TIMEOUT_PROTECTION
    if (setjmp(buf) != 0) {
        goto turn_done;
    }
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
	{ /* Render cost map(s) */
		int i,j;
		for(i=0;i<Info->rows;i++) {
			fprintf(stderr,"plt fod %03d: ", i);
			for(j=0;j<Info->cols;j++) {
				fprintf(stderr,"%lf ", cost_map[i*Info->cols+j].food);
			}
			fprintf(stderr,"\n");
			fprintf(stderr,"plt hil %03d: ", i);
			for(j=0;j<Info->cols;j++) {
				fprintf(stderr,"%lf ", cost_map[i*Info->cols+j].hill);
			}
			fprintf(stderr,"\n");
			fprintf(stderr,"plt uns %03d: ", i);
			for(j=0;j<Info->cols;j++) {
				fprintf(stderr,"%lf ", cost_map[i*Info->cols+j].unseen);
			}
			fprintf(stderr,"\n");
			fprintf(stderr,"plt scr %03d: ", i);
			for(j=0;j<Info->cols;j++) {
				fprintf(stderr,"%lf ", calc_score(cost_map,i*Info->cols+j));
			}
			fprintf(stderr,"\n");
			//fprintf(stderr,"plt ant %03d: ", i);
			//for(j=0;j<Info->cols;j++) {
			//	fprintf(stderr,"%lf ", cost_map[i*Info->cols+j].friend);
			//}
			//fprintf(stderr,"\n");
			fprintf(stderr,"plt bat %03d: ", i);
			for(j=0;j<Info->cols;j++) {
				//fprintf(stderr,"%lf ", cost_map[i*Info->cols+j].battle_outcome);
				fprintf(stderr,"%lf ", cost_map[i*Info->cols+j].battle);
				//fprintf(stderr,"%d.0 ", attacked_by[i*Info->cols+j].atkr_count);
			}
			fprintf(stderr,"\n");
			fprintf(stderr,"plt vis %03d: ", i);
			for(j=0;j<Info->cols;j++) {
				fprintf(stderr,"%lf ", cost_map[i*Info->cols+j].visibility);
				//fprintf(stderr,"%d.0 ", vis_tmp[i*Info->cols+j]);
			}
			fprintf(stderr,"\n");
		}
	}
#endif

    for (i = 0; i < Game->my_count; ++i) {
		struct my_ant * m = &Game->my_ants[i];
        int offset = INDEX_AT(m->row,m->col);

		// Find min neighbor in cost_map
		double max_score=-INFINITY;
        char dir = -1;
        char d = -1;
		struct loc_t nl,l;
		int n;
		l.row = Game->my_ants[i].row; l.col=Game->my_ants[i].col;
		for(n=4;n>=0;n--) {
			get_neighbor(Info, &l, n, &nl,&d);
			int no = INDEX_AT(nl.row, nl.col);
			if(IS_WATER(Info->map[no]) || Info->map[no] == MOVE_OFFSET) continue; // Don't double move to a square, or to into water
			double score = calc_score(cost_map,no);
            LOG("  ?move (%d,%d)->(%d,%d) %lf [f%lf,h%lf,u%lf,v%lf,b%lf] %c\n", 
                l.row,l.col, nl.row,nl.col, score
                , 10.0   * cost_map[no].food
                , 20.0   * cost_map[no].hill
                , 0.25   * cost_map[no].unseen
                , 0.0125 * cost_map[no].visibility
                , 100.0  * cost_map[no].battle
                , d );

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
