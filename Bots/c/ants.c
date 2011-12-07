#include "ants.h"

// initializes the game_info structure on the very first turn
// function is not called after the game has started

void _init_ants(char *data, struct game_info *game_info) {
    char *replace_data = data;

    while (*replace_data != '\0') {
        if (*replace_data == '\n')
            *replace_data = '\0';
        ++replace_data;
    }

    while (42) {
        char *value = data;

        while (*++value != ' ');
        ++value;

        int num_value = atoi(value);

	    switch (*data) {
            case 'l':
                game_info->loadtime = num_value;
                break;

            case 't':
                if (*(data + 4) == 't')
                    game_info->turntime = num_value;
                else
                    game_info->turns = num_value;
                break;

            case 'r':
                game_info->rows = num_value;
                break;

            case 'c':
                game_info->cols = num_value;
                break;

            case 'v':
                game_info->viewradius_sq = num_value;
                break;

            case 'a':
                game_info->attackradius_sq = num_value;
                break;

            case 's':
                if (*(data + 1) == 'p')
                    game_info->spawnradius_sq = num_value;
                else {
                    game_info->seed = num_value;
                    srand(game_info->seed);
                }
                break;
        }

        data = value;

        while (*++data != '\0');
        ++data;

        if (strcmp(data, "ready") == 0)
            break;
    }
}

// updates game data with locations of ants and food
// only the ids of your ants are preserved
void shuffle_ants(int count, struct my_ant * my_ants) {
    int i,j;
    for(i=0;i<count;i++)
    {
        struct my_ant a = my_ants[i];
        j = rand() % (count-i) + i;
        my_ants[i] = my_ants[j];
        my_ants[j] = a;
    }
}

void _init_game(struct game_info *game_info, struct game_state *game_state) {
    int map_len = game_info->rows*game_info->cols;

    int my_count = 0;
    int my_hill_count = 0;
    int enemy_hill_count = 0;
    int enemy_count = 0;
    int food_count = 0;
    int dead_count = 0;
    int i, j;

    for (i = 0; i < map_len; ++i) {
        uint8_t current = game_info->map[i];

        if (IS_BACKGROUND(current))
            continue;
        if (IS_FOOD(current))
            ++food_count;
        if (IS_MY_HILL(current))
            ++my_hill_count;
        if (IS_ENEMY_HILL(current))
            ++enemy_hill_count;
        if (IS_DEAD(current))
            ++dead_count;
        if(IS_ANT(current)) {
            if (IS_MY_ANT(current))
                ++my_count;
            else
                ++enemy_count;
        }
    }
    LOG("food: %d, my: %d, dead: %d, enemy: %d, enemy_hill: %d, my_hills: %d\n",
        food_count, my_count, dead_count, enemy_count, enemy_hill_count, my_hill_count);

    struct my_ant *my_old = 0;
    int my_old_count = game_state->my_count;

    game_state->my_count = my_count;
    game_state->my_hill_count = my_hill_count;
    game_state->enemy_hill_count = enemy_hill_count;
    game_state->enemy_count = enemy_count;
    game_state->food_count = food_count;
    game_state->dead_count = dead_count;

    if (game_state->my_ants)
        my_old = game_state->my_ants;

    if (game_state->my_hills)
        free(game_state->my_hills);
    if (game_state->enemy_hills)
        free(game_state->enemy_hills);
    if (game_state->enemy_ants)
        free(game_state->enemy_ants);
    if (game_state->food)
        free(game_state->food);
    if (game_state->dead_ants)
        free(game_state->dead_ants);

    game_state->my_ants = malloc(my_count*sizeof(struct my_ant));

    if (my_hill_count> 0)
        game_state->my_hills = malloc(my_hill_count*sizeof(struct my_ant));
    else
        game_state->my_hills = 0;

    if (enemy_hill_count> 0)
        game_state->enemy_hills = malloc(enemy_hill_count*sizeof(struct basic_ant));
    else
        game_state->enemy_hills = 0;

    if (enemy_count > 0)
        game_state->enemy_ants = malloc(enemy_count*sizeof(struct basic_ant));
    else
        game_state->enemy_ants = 0;

    if (dead_count > 0)
        game_state->dead_ants = malloc(dead_count*sizeof(struct basic_ant));
    else
        game_state->dead_ants = 0;

    game_state->food = malloc(food_count*sizeof(struct food));

    for (i = 0; i < game_info->rows; ++i) {
        for (j = 0; j < game_info->cols; ++j) {
            uint8_t current = game_info->map[game_info->cols*i + j];
			if (IS_BACKGROUND(current))
                 continue;

            if (IS_FOOD(current)) {
                --food_count;

                game_state->food[food_count].row = i;
                game_state->food[food_count].col = j;
            }
            if (IS_DEAD(current)) {
                --dead_count;

                game_state->dead_ants[dead_count].row = i;
                game_state->dead_ants[dead_count].col = j;
                game_state->dead_ants[dead_count].player = current;
            }
            if (IS_ENEMY_HILL(current)) {
                --enemy_hill_count;

                game_state->enemy_hills[enemy_hill_count].row = i;
                game_state->enemy_hills[enemy_hill_count].col = j;
                game_state->enemy_hills[enemy_hill_count].player = current;
            }
            if (IS_MY_HILL(current)) {
                --my_hill_count;

                game_state->my_hills[my_hill_count].row = i;
                game_state->my_hills[my_hill_count].col = j;
            }
            if (IS_ANT(current)) {
                if (IS_MY_ANT(current)) {
                    --my_count;

                    int keep_id = -1;
                    int k = 0;

                    if (my_old != 0) {
                        for (; k < my_old_count; ++k) {
                            if (my_old[k].row == i && my_old[k].col == j) {
                                keep_id = my_old[k].id;
                                break;
                            }
                        }
                    }

                    game_state->my_ants[my_count].row = i;
                    game_state->my_ants[my_count].col = j;

                    if (keep_id == -1)
                        game_state->my_ants[my_count].id = ++game_state->my_ant_index;
                    else
                        game_state->my_ants[my_count].id = keep_id;
                } else {
                    --enemy_count;

                    game_state->enemy_ants[enemy_count].row = i;
                    game_state->enemy_ants[enemy_count].col = j;
                    game_state->enemy_ants[enemy_count].player = current;
                }
            }
        }
    }

    // Randomize the order of ants in effort to improve move selection.
    // Alternatives are sorting by something (degrees of freedom, ...)
    shuffle_ants(game_state->my_count,game_state->my_ants);

    if (my_old != 0)
        free(my_old);
}

struct knowledge_t {
    int offset;
    uint8_t value;
};
void _init_map(char *data, struct game_info *Info)
{
    static struct knowledge_t *new_knol = NULL;
    int new_knol_count = 0;
    if(!new_knol)
        new_knol = malloc(sizeof(struct knowledge_t)*Info->rows*Info->cols);

   if (Info->map == 0) {
        Info->map = malloc(Info->rows*Info->cols);
        memset(Info->map, MAP_UNSEEN, Info->rows*Info->cols);
    }

    int map_len = Info->rows*Info->cols;
    int i = 0;

    while (*data != 0) {
        char *tmp_data = data;
        int arg = 0;

        while (*tmp_data != '\n') {
            if (*tmp_data == ' ') {
                *tmp_data = '\0';
                ++arg;
            }

            ++tmp_data;
        }

        char *tmp_ptr = tmp_data;
        tmp_data = data;

        tmp_data += 2;
        int jump = strlen(tmp_data) + 1;

        int row = atoi(tmp_data);
        int col = atoi(tmp_data + jump);
        char var3 = -1;

        if (arg > 2) {
            jump += strlen(tmp_data + jump) + 1;
			var3 = strtol(tmp_data+jump,NULL,10);
			//assert(var3<=MAX_PLAYERS);
        }

        new_knol[new_knol_count].offset=row*Info->cols + col;
        new_knol[new_knol_count].value=255;

        switch (*data) {
            case 'w':
                new_knol[new_knol_count].value = MAP_WATER;
                break;
            case 'a':
                new_knol[new_knol_count].value = var3 | ANT_BIT;
                break;
            case 'h':
                new_knol[new_knol_count].value = var3 | HILL_BIT;
                break;
            case 'd':
                new_knol[new_knol_count].value = var3 | DEAD_BIT;
                break;
            case 'f':
                new_knol[new_knol_count].value = MAP_FOOD;
                break;
            case 'l':
                new_knol[new_knol_count].value = MAP_LAND;
                break;
        }
        if(new_knol[new_knol_count].value!=255)
            new_knol_count++;
        //else LOG("PROBLEM, unrecognized item, %c\n", *data); // Catches a 'g' probably from 'go'?

        data = tmp_ptr + 1;
    }

    /* Build new visibility table */
    if(!Info->vis_tmp)
        Info->vis_tmp = malloc(Info->rows*Info->cols*sizeof(int));
    memset(Info->vis_tmp, 0, Info->rows*Info->cols*sizeof(int));
    int rad = ceil(sqrt(Info->viewradius_sq)) + 2;
    int x,y;
    for(i=0;i<new_knol_count;i++)
        if(IS_MY_ANT(new_knol[i].value)) {
            int mr,mc;
            AT_INDEX(mr,mc,new_knol[i].offset);
            for(x=-rad;x<=rad;x++)
                for(y=-rad;y<=rad;y++) {
                    int r = WRAP_R(x+mr);
                    int c = WRAP_C(y+mc);
                    int offset = INDEX_AT(r,c);
                    if(edist_sq(mr,mc,r,c,Info) < Info->viewradius_sq)
                        Info->vis_tmp[offset]+=1;
                }
        }

	// reset old knowledge -- my ants and visible stuff
    for (i=0; i < map_len; ++i) {
        // Clear all my ants,
        if ( IS_MY_ANT(Info->map[i]) && IS_HILL(Info->map[i]) )
            Info->map[i] &= ~ANT_BIT;
        else if ( IS_MY_ANT(Info->map[i] ) )
            Info->map[i] = MAP_LAND;
        // Clear any move bit
        if(Info->map[i]&MOVE_BIT) Info->map[i] &= ~MOVE_BIT;
        // If it is visible and it is an objct, we will reset from the
        // new_knol in a second. Or if it is visible and unseen, it can now be
        // called land.
        if ( Info->vis_tmp[i] &&
             (IS_OBJECT(Info->map[i])||IS_UNSEEN(Info->map[i])) )
            Info->map[i] = MAP_LAND;
    }
    // Add new knowledge
    for(i=0;i<new_knol_count;i++) {
        uint8_t cur=Info->map[new_knol[i].offset], new=new_knol[i].value;
        // Ants and hills can share a square if they are same owner.
        if( GET_OWNER(cur) == GET_OWNER(new) &&
            ((IS_ANT(cur) && IS_HILL(new)) || (IS_ANT(new) && IS_HILL(cur))) )
            Info->map[new_knol[i].offset]|=new_knol[i].value;
        else
            Info->map[new_knol[i].offset]=new_knol[i].value;
    }
}


void render_map(struct game_info *Info) {
	int i,j;
	for(j=0;j<Info->cols+5;j++) {
		fprintf(stderr,"=");
	}
	fprintf(stderr,"\n");
	for(i=0;i<Info->rows;i++) {
		fprintf(stderr,"%03d: ", i);
		for(j=0;j<Info->cols;j++) {
			char render = '_';
				 if(IS_LAND(Info->map[i*Info->cols+j]))  render = '.';
			else if(IS_WATER(Info->map[i*Info->cols+j])) render = '%';
			else if(IS_UNSEEN(Info->map[i*Info->cols+j]))render = '?';
			else if(IS_FOOD(Info->map[i*Info->cols+j]))  render = '*';
			else if(IS_ANT(Info->map[i*Info->cols+j]))   render = '0'+GET_OWNER(Info->map[i*Info->cols+j]);
			else if(IS_HILL(Info->map[i*Info->cols+j]))  render = 'A'+GET_OWNER(Info->map[i*Info->cols+j]);
			else if(IS_DEAD(Info->map[i*Info->cols+j]))  render = 'a'+GET_OWNER(Info->map[i*Info->cols+j]);
			fprintf(stderr,"%c", render);
			//fprintf(stderr,"%3d ", Info->map[i*Info->cols+j]);
		}
		fprintf(stderr,"\n");
	}
}

