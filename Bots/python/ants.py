#!/usr/bin/env python
import sys
import traceback
import random
import time
from collections import defaultdict
from math import sqrt

MY_ANT = 0
ANTS = 0
DEAD = -1
LAND = -2
FOOD = -3
WATER = -4

PLAYER_ANT = 'abcdefghij'
HILL_ANT = string = 'ABCDEFGHI'
PLAYER_HILL = string = '0123456789'
MAP_OBJECT = '?%*.!'
MAP_RENDER = PLAYER_ANT + HILL_ANT + PLAYER_HILL + MAP_OBJECT

AIM = {'n': (-1, 0),
       'e': (0, 1),
       's': (1, 0),
       'w': (0, -1)}
RIGHT = {'n': 'e',
         'e': 's',
         's': 'w',
         'w': 'n'}
LEFT = {'n': 'w',
        'e': 'n',
        's': 'e',
        'w': 's'}
BEHIND = {'n': 's',
          's': 'n',
          'e': 'w',
          'w': 'e'}

################################################################################
import logging
logLevel = logging.DEBUG
logging.basicConfig()
logger = logging.getLogger("Ants")
logger.setLevel(logLevel)

ld = logger.debug
li = logger.info
lw = logger.warning
################################################################################

class Ants():
    def __init__(self):
        self.cols = None
        self.rows = None
        self.map = None
        self.hill_list = {}
        self.ant_list = {}
        self.dead_list = defaultdict(list)
        self.food_list = []
        self.turntime = 0
        self.loadtime = 0
        self.turn_start_time = None
        self.vision = None
        self.viewradius2 = 0
        self.attackradius2 = 0
        self.spawnradius2 = 0
        self.turns = 0
        self.path_cache = {}

    def setup(self, data):
        'parse initial input and setup starting game state'
        for line in data.split('\n'):
            line = line.strip().lower()
            if len(line) > 0:
                tokens = line.split()
                key = tokens[0]
                if key == 'cols':
                    self.cols = int(tokens[1])
                elif key == 'rows':
                    self.rows = int(tokens[1])
                elif key == 'player_seed':
                    random.seed(int(tokens[1]))
                elif key == 'turntime':
                    self.turntime = int(tokens[1])
                elif key == 'loadtime':
                    self.loadtime = int(tokens[1])
                elif key == 'viewradius2':
                    self.viewradius2 = int(tokens[1])
                elif key == 'attackradius2':
                    self.attackradius2 = int(tokens[1])
                elif key == 'spawnradius2':
                    self.spawnradius2 = int(tokens[1])
                elif key == 'turns':
                    self.turns = int(tokens[1])
        self.map = [[LAND for col in range(self.cols)]
                    for row in range(self.rows)]

    def update(self, data):
        'parse engine input and update the game state'
        # start timer
        self.turn_start_time = time.clock()
        
        # reset vision
        self.vision = None

        # reset paths
        self.path_cache = {}
        
        # clear hill, ant and food data
        self.hill_list = {}
        for row, col in self.ant_list.keys():
            self.map[row][col] = LAND
        self.ant_list = {}
        for row, col in self.dead_list.keys():
            self.map[row][col] = LAND
        self.dead_list = defaultdict(list)
        for row, col in self.food_list:
            self.map[row][col] = LAND
        self.food_list = []
        
        # update map and create new ant and food lists
        for line in data.split('\n'):
            line = line.strip().lower()
            if len(line) > 0:
                tokens = line.split()
                if len(tokens) >= 3:
                    row = int(tokens[1])
                    col = int(tokens[2])
                    if tokens[0] == 'w':
                        self.map[row][col] = WATER
                    elif tokens[0] == 'f':
                        self.map[row][col] = FOOD
                        self.food_list.append((row, col))
                    else:
                        owner = int(tokens[3])
                        if tokens[0] == 'a':
                            self.map[row][col] = owner
                            self.ant_list[(row, col)] = owner
                        elif tokens[0] == 'd':
                            # food could spawn on a spot where an ant just died
                            # don't overwrite the space unless it is land
                            if self.map[row][col] == LAND:
                                self.map[row][col] = DEAD
                            # but always add to the dead list
                            self.dead_list[(row, col)].append(owner)
                        elif tokens[0] == 'h':
                            owner = int(tokens[3])
                            self.hill_list[(row, col)] = owner
                        
    def time_remaining(self):
        return self.turntime - int(1000 * (time.clock() - self.turn_start_time))
    
    def issue_order(self, order):
        'issue an order by writing the proper ant location and direction'
        (row, col), direction = order
        sys.stdout.write('o %s %s %s\n' % (row, col, direction))
        sys.stdout.flush()
        
    def finish_turn(self):
        'finish the turn by writing the go line'
        sys.stdout.write('go\n')
        sys.stdout.flush()
    
    def my_hills(self):
        return [loc for loc, owner in self.hill_list.items()
                    if owner == MY_ANT]

    def enemy_hills(self):
        return [(loc, owner) for loc, owner in self.hill_list.items()
                    if owner != MY_ANT]
        
    def my_ants(self):
        'return a list of all my ants'
        return [(row, col) for (row, col), owner in self.ant_list.items()
                    if owner == MY_ANT]

    def enemy_ants(self):
        'return a list of all visible enemy ants'
        return [((row, col), owner)
                    for (row, col), owner in self.ant_list.items()
                    if owner != MY_ANT]

    def food(self):
        'return a list of all food locations'
        return self.food_list[:]

    def passable(self, loc):
        'true if not water'
        row, col = loc
        return self.map[row][col] != WATER
    
    def unoccupied(self, loc):
        'true if no ants are at the location'
        row, col = loc
        return self.map[row][col] in (LAND, DEAD)

    def destination(self, loc, direction):
        'calculate a new location given the direction and wrap correctly'
        row, col = loc
        d_row, d_col = AIM[direction]
        return ((row + d_row) % self.rows, (col + d_col) % self.cols)        

    def __reconstruct_path(self,came_from, current_node):
        if current_node in came_from:
            p = self.__reconstruct_path(came_from, came_from[current_node])
            return p + [current_node]
        else:
            return [current_node]
    def neighbors(self, pos):
        return [x for x in [self.destination(pos,d) for d in AIM.keys()] if self.passable(x)]
    def bfs(self,start,goals):
        ld("bfs: %s->%s",start,goals)
        if start in goals: return 0
        came_from = {}
        search_counter=0
        Q = []
        Q.append( (start,0) )
        seen = set([start])
        while Q:
            search_counter+=1
            v,vs=Q.pop(0)
            seen.add(v)
            if self.time_remaining() < 20: ld("OH FUCK, OUT OF TIME!!!") ; return (9999999,start)
            #if search_counter > 2**13:  ld("Search too far") ; return self.distance(start,goals[0])
            if(v in goals):
                pth = self.__reconstruct_path(came_from,came_from[v])
                ld("bfs: steps=%d v=%s, score=%d(%d), %s",search_counter,v,vs,len(pth),pth)
                #ld("bfs: steps=%d v=%s, score=%d",search_counter,v,vs)
                return (vs,pth[0])
            for w in self.neighbors(v):
                if w not in seen:
                    Q.append((w,vs+1))
                    came_from[w]=v
                    if(w in goals):
                        pth = self.__reconstruct_path(came_from,came_from[w])
                        ld("bfs: steps=%d v=%s, score=%d(%d), %s",search_counter,v,vs,len(pth),pth)
                        return (vs+1,pth[0]) # fuck it
        ld("bfs: NO PATH FOUND")
        return (9999999,start)

    def a_star(self, start, goals):
        ld("a_star: %s->%s",start,goals)
        search_counter = 0
        closedset = set()
        openset = set()
        g_score = {}
        h_score = {}
        f_score = {}
        o_score = {} # cached copy of f_scores but only with the members of openset
        came_from = {}

        openset.add(start)
        g_score[start] = 0
        h_score[start] = min([self.distance(start,goal) for goal in goals])
        f_score[start] = g_score[start]+h_score[start]
        o_score[start] = g_score[start]+h_score[start]

        while openset:
            search_counter+=1
            x=min(o_score)
            x_score = f_score[x]
            if x in goal: 
                pth = self.__reconstruct_path(came_from,came_from[x])
                #ld("a_star: steps=%d score=%d(%d) path: %s",
                #        search_counter,x_score, len(pth),pth )
                return (x_score,pth[0])
            if self.time_remaining() < 20:
                ld("OH FUCK, OUT OF TIME!!!")
                pth = self.__reconstruct_path(came_from,came_from[x])
                return (x_score,pth[0])
            #if search_counter > 1000:  ld("Search too far") ; return x_score
            openset.remove(x)
            o_score.pop(x)
            closedset.add(x)
            for y in self.neighbors(x):
                if y in closedset: continue
                tenative_g_score = g_score[x] + 1
                tenative_is_better = False
                if y not in openset:
                    openset.add(y)
                    tenative_is_better = True
                elif tenative_g_score < g_score[y]:
                    tenative_is_better = True
                if tenative_is_better:
                    came_from[y] = x
                    g_score[y] = tenative_g_score
                    h_score[y] = min([self.distance(y,goal) for goal in goals])
                    f_score[y] = g_score[y] + h_score[y]
                    o_score[y] = f_score[y]
        return (999999999,start)

    def path(self, start, goals):
        # do a path search in the map here
        #return self.a_star(start,goals)[0]
        return self.bfs(start,goals)[0]

    def distance(self, loc1, loc2):
        'calculate the closest distance between to locations'
        #:TODO: do a path search in the map here
        row1, col1 = loc1
        row2, col2 = loc2
        d_col = min(abs(col1 - col2), self.cols - abs(col1 - col2))
        d_row = min(abs(row1 - row2), self.rows - abs(row1 - row2))
        return d_row + d_col


    def direction(self, loc1, loc2):
        'determine the 1 or 2 fastest (closest) directions to reach a location'
        row1, col1 = loc1
        row2, col2 = loc2
        height2 = self.rows//2
        width2 = self.cols//2
        d = []
        if row1 < row2:
            if row2 - row1 >= height2:
                d.append('n')
            if row2 - row1 <= height2:
                d.append('s')
        if row2 < row1:
            if row1 - row2 >= height2:
                d.append('s')
            if row1 - row2 <= height2:
                d.append('n')
        if col1 < col2:
            if col2 - col1 >= width2:
                d.append('w')
            if col2 - col1 <= width2:
                d.append('e')
        if col2 < col1:
            if col1 - col2 >= width2:
                d.append('e')
            if col1 - col2 <= width2:
                d.append('w')
        return d

    def visible(self, loc):
        ' determine which squares are visible to the given player '

        if self.vision == None:
            if not hasattr(self, 'vision_offsets_2'):
                # precalculate squares around an ant to set as visible
                self.vision_offsets_2 = []
                mx = int(sqrt(self.viewradius2))
                for d_row in range(-mx,mx+1):
                    for d_col in range(-mx,mx+1):
                        d = d_row**2 + d_col**2
                        if d <= self.viewradius2:
                            self.vision_offsets_2.append((
                                d_row%self.rows-self.rows,
                                d_col%self.cols-self.cols
                            ))
            # set all spaces as not visible
            # loop through ants and set all squares around ant as visible
            self.vision = [[False]*self.cols for row in range(self.rows)]
            for ant in self.my_ants():
                a_row, a_col = ant
                for v_row, v_col in self.vision_offsets_2:
                    self.vision[a_row+v_row][a_col+v_col] = True
        row, col = loc
        return self.vision[row][col]

    def visible_from(self, loc):
        ' determine which squares are visible to the given player '

        if not hasattr(self, 'vision_offsets_2'):
            # precalculate squares around an ant to set as visible
            self.vision_offsets_2 = []
            mx = int(sqrt(self.viewradius2))
            for d_row in range(-mx,mx+1):
                for d_col in range(-mx,mx+1):
                    d = d_row**2 + d_col**2
                    if d <= self.viewradius2:
                        self.vision_offsets_2.append((
                            d_row%self.rows-self.rows,
                            d_col%self.cols-self.cols
                        ))
        # set all spaces as not visible
        # loop through ants and set all squares around ant as visible
        a_row, a_col = loc
        this_vision = []
        for v_row, v_col in self.vision_offsets_2:
            this_vision.append( (a_row+v_row,a_col+v_col) )
        return this_vision
    
    def render_text_map(self):
        'return a pretty string representing the map'
        tmp = ''
        for row in self.map:
            tmp += '# %s\n' % ''.join([MAP_RENDER[col] for col in row])
        return tmp

    # static methods are not tied to a class and don't have self passed in
    # this is a python decorator
    @staticmethod
    def run(bot):
        'parse input, update game state and call the bot classes do_turn method'
        ants = Ants()
        map_data = ''
        while(True):
            try:
                current_line = sys.stdin.readline().rstrip('\r\n') # string new line char
                if current_line.lower() == 'ready':
                    ants.setup(map_data)
                    bot.do_setup(ants)
                    ants.finish_turn()
                    map_data = ''
                elif current_line.lower() == 'go':
                    ants.update(map_data)
                    # call the do_turn method of the class passed in
                    bot.do_turn(ants)
                    ants.finish_turn()
                    map_data = ''
                else:
                    map_data += current_line + '\n'
            except EOFError:
                break
            except KeyboardInterrupt:
                raise
            except:
                # don't raise error or return so that bot attempts to stay alive
                traceback.print_exc(file=sys.stderr)
                sys.stderr.flush()
