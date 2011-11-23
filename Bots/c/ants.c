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
                else
                    game_info->seed = num_value;
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

void _init_game(struct game_info *game_info, struct game_state *game_state) {
    int map_len = game_info->rows*game_info->cols;

    int my_count = 0;
    int enemy_count = 0;
    int food_count = 0;
    int dead_count = 0;
    int i, j;

    for (i = 0; i < map_len; ++i) {
        uint8_t current = game_info->map[i];

        if (IS_BACKGROUND(current))
            continue;
        else if (IS_FOOD(current))
            ++food_count;
        else if (IS_MY_ANT(current))
            ++my_count;
        else if (IS_DEAD(current))
            ++dead_count;
        else if (IS_ANT(current))
            ++enemy_count;
    }

    struct my_ant *my_old = 0;
    int my_old_count = game_state->my_count;

    game_state->my_count = my_count;
    game_state->enemy_count = enemy_count;
    game_state->food_count = food_count;
    game_state->dead_count = dead_count;

    if (game_state->my_ants != 0)
        my_old = game_state->my_ants;

    if (game_state->enemy_ants != 0)
        free(game_state->enemy_ants);
    if (game_state->food != 0)
        free(game_state->food);
    if (game_state->dead_ants != 0)
        free(game_state->dead_ants);

    game_state->my_ants = malloc(my_count*sizeof(struct my_ant));

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
            else if (IS_MY_ANT(current)) {
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
            }
            else if (IS_DEAD(current)) {
                --dead_count;

                game_state->dead_ants[dead_count].row = i;
                game_state->dead_ants[dead_count].col = j;
                game_state->dead_ants[dead_count].player = current;
            }
			else if (IS_ANT(current)) {
                --enemy_count;

                game_state->enemy_ants[enemy_count].row = i;
                game_state->enemy_ants[enemy_count].col = j;
                game_state->enemy_ants[enemy_count].player = current;
            } 
        }
    }

	int k;
	for (i = 0; i < game_info->rows; ++i)
		for (j = 0; j < game_info->cols; ++j) {
			int offset = i*game_info->cols+j;
			if( IS_UNSEEN(game_info->map[offset]) )
				for(k=0;k<game_state->my_count;++k)
                    if( edist_sq(i, j, game_state->my_ants[k].row,
                                 game_state->my_ants[k].col, game_info) <=
                        game_info->viewradius_sq ) {
						game_info->map[offset] = LAND_OFFSET;
						break;
					}
		}

    if (my_old != 0)
        free(my_old);
}

void _init_map(char *data, struct game_info *game_info) {

   if (game_info->map == 0) {
        game_info->map = malloc(game_info->rows*game_info->cols);
        memset(game_info->map, UNSEEN_OFFSET, game_info->rows*game_info->cols);
    }

    int map_len = game_info->rows*game_info->cols;
    int i = 0;

	// reset old knowledge
    for (; i < map_len; ++i)
		if (IS_OBJECT(game_info->map[i]))
			game_info->map[i] = LAND_OFFSET;
	/* TODO if food/hill, only reset if visible .. otherwise assume it exists. */

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

        int offset = row*game_info->cols + col;

        switch (*data) {
            case 'w':
                game_info->map[offset] = WATER_OFFSET;
                break;
            case 'a':
                game_info->map[offset] = var3 + ANT_OFFSET;
                break;
            case 'h':
                game_info->map[offset] = var3 + HILL_OFFSET;
                break;
            case 'd':
                game_info->map[offset] = var3 + DEAD_OFFSET;
                break;
            case 'f':
                game_info->map[offset] = FOOD_OFFSET;
                break;
            case 'l':
                game_info->map[offset] = LAND_OFFSET;
                break;
        }

        data = tmp_ptr + 1;
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
			else if(IS_ANT(Info->map[i*Info->cols+j]))   render = '0'+Info->map[i*Info->cols+j]-ANT_OFFSET;
			else if(IS_HILL(Info->map[i*Info->cols+j]))  render = 'A'+Info->map[i*Info->cols+j]-HILL_OFFSET;
			else if(IS_DEAD(Info->map[i*Info->cols+j]))  render = 'a'+Info->map[i*Info->cols+j]-DEAD_OFFSET;
			fprintf(stderr,"%c", render);
			//fprintf(stderr,"%3d ", Info->map[i*Info->cols+j]);
		}
		fprintf(stderr,"\n");
	}
}

