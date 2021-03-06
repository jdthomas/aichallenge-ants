#!/usr/bin/env python
import sys
import traceback
import random
import time
from collections import defaultdict
from math import sqrt
from numpy import *

MY_ANT = 0
ANTS = 0
DEAD = -1
LAND = -2
FOOD = -3
WATER = -4
UNSEEN = -5
HILL = -6

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
        self.cur_turn=0
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
        self.MAXPATH = 9999999
        self.MAXDIST = 9999999
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
        self.MAXDIST = (self.rows + self.cols)/2
        self.MAXPATH = (self.rows * self.cols)/2
        self.A_STAR_TIE_BREAKER = 1.0/self.MAXPATH
        #ld("A_STAR_TIE_BREAKER: %s", self.A_STAR_TIE_BREAKER)
        self.map = [[UNSEEN for col in range(self.cols)]
                    for row in range(self.rows)]
        # Generate these offsets now, no since in wasting turn time on it
        self.generate_attack_offsets()
        self.generate_vision_offsets()
        self.generate_vision_offsets_per()

    def update(self, data):
        'parse engine input and update the game state'
        # start timer
        self.turn_start_time = time.clock()
        
        # reset vision
        self.vision = None

        # reset paths
        self.path_cache = {}
        
        # clear hill, ant and food data
        for row, col in self.ant_list.keys():
            self.map[row][col] = LAND
        self.ant_list = {}
        for row, col in self.food_list:
            self.map[row][col] = LAND
        self.food_list = []
        for row, col in self.dead_list.keys():
            self.map[row][col] = LAND
        self.dead_list = defaultdict(list)
        for (row, col), owner in self.hill_list.items():
            self.map[row][col] = LAND
        self.hill_list = {}

        #ld("data:\n%s", data)
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
                    elif tokens[0] == 'l':
                        self.map[row][col] = LAND
                    else:
                        owner = int(tokens[3])
                        if tokens[0] == 'a':
                            self.map[row][col] = owner
                            self.ant_list[(row, col)] = owner
                        elif tokens[0] == 'd':
                            # food could spawn on a spot where an ant just died
                            # don't overwrite the space unless it is land
                            if self.map[row][col] == LAND or self.map[row][col] == UNSEEN:
                                self.map[row][col] = DEAD
                            # but always add to the dead list
                            self.dead_list[(row, col)].append(owner)
                        elif tokens[0] == 'h':
                            owner = int(tokens[3])
                            self.hill_list[(row, col)] = owner
        # mark all visible UNSEEN cells as LAND
        #def update_unseen():
        #    for r in range(self.rows):
        #        for c in range(self.cols):
        #            if self.map[r][c] == UNSEEN and self.visible((r,c)):
        #                self.map[r][c] = LAND
        def update_unseen():
            for a in self.my_ants():
                for (r,c) in self.visible_from_per(a):
                    if self.map[r][c] == UNSEEN: self.map[r][c] = LAND
        update_unseen()
        #ld("\n%s\n",self.render_text_map())

        def symmetery():
            import math
            import numpy as NP
            import scipy as SP
            import pylab as PL
            from scipy import fftpack, signal
            A = NP.zeros((self.rows,self.cols))
            for r in range(self.rows):
                for c in range(self.cols):
                    x=-1.0 #LAND, all else
                    if self.map[r][c] == UNSEEN: x=0.0
                    if self.map[r][c] == WATER: x=1.0
                    A[r][c] = x
            PL.clf()
            PL.imshow(A,cmap=PL.cm.jet)
            PL.savefig("/tmp/HeatMap/FFTMap_%s.png"%self.cur_turn)

            fmap = fftpack.fft2(A)
            FM2 = [[math.log(abs(x)) for x in y] for y in fmap]
            PL.imshow(FM2,cmap=PL.cm.jet)
            PL.savefig("/tmp/HeatMap/FFTMap_fft_%s.png"%self.cur_turn)
            self.cur_turn+=1

            laplacian = NP.array([[0,1,0],[1,-4,1],[0,1,0]],NP.float32)
            deriv2 = signal.convolve2d(fmap,laplacian,mode='same',boundary='symm')
            #derfilt = NP.array([1.0,-2,1.0],NP.float32)
            #ck = signal.cspline2d(FM2,8.0)
            #deriv = signal.sepfir2d(ck, derfilt, [1]) + \
            #signal.sepfir2d(ck, [1], derfilt)
            #for r in range(self.rows):
            #    ld("A: %s",''.join(map(str,A[r])))
            #for r in range(self.rows):
            #    ld("F: %s",''.join(map(str,fmap[r])))
            #for r in range(self.rows):
            #    ld("D: %s",''.join(map(str,deriv2[r])))
            #PL.clf()
            #PL.imshow(deriv,cmap=PL.cm.jet)
            #PL.savefig("/tmp/HeatMap/FFTMap_der_%s.png"%self.cur_turn)
            #PL.clf()
        #symmetery()

                        
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

    def all_ants(self):
        return [(loc,owner) for loc, owner in self.ant_list.items()]
    def my_ants(self):
        'return a list of all my ants'
        return [loc for loc, owner in self.ant_list.items()
                    if owner == MY_ANT]

    def enemy_ants(self):
        'return a list of all visible enemy ants'
        return [(loc, owner) for loc, owner in self.ant_list.items()
                    if owner != MY_ANT]
    
    def my_hills(self):
        return [loc for loc, owner in self.hill_list.items()
                    if owner == MY_ANT]

    def enemy_hills(self):
        return [(loc, owner) for loc, owner in self.hill_list.items()
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
        return self.map[row][col] in (LAND, DEAD, UNSEEN)

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
    def bfs_build_full_map(self,starts):
        """ Build a full map of distances from starts to each square """
        t1 = time.time()
        new_map = zeros((self.rows,self.cols),int32)
        #new_map = [[0]*self.cols for x in range(self.rows)]
        search_counter=0
        Q = [(s,0) for s in starts]
        seen = set(starts)
        #ld("bfs_all: %sx%s %s", self.rows, self.cols, seen)
        while Q:
            search_counter+=1
            v,vs=Q.pop(0)
            #ld("bfs: popped %s %s @%s %s", v,vs, search_counter, len(seen))
            seen.add(v)
            r,c = v
            new_map[r][c] = vs
            for w in self.neighbors(v):
                if w not in seen:
                    Q.append((w,vs+1))
                    seen.add(w) # Add to seen now so we don't re-add it.
                    #ld("bfs: seen %s %s", w,vs+1)
        t2 = time.time()
        ld("bfs_all done in %s\n%s",t2-t1,new_map)
        return new_map
    def bfs(self,start,goals):
        #ld("bfs: %s->%s",start,goals)
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
            #ld("bfs: popped %s %s", v,vs)
            #if self.time_remaining() < 30: ld("bfs: TIMEOUT, OH FUCK!!! %s",search_counter); return (self.MAXPATH,start)
            #if search_counter > 2**12:  ld("bfs: Search too far") ; return (self.MAXPATH,start)
            if(v in goals):
                pth = self.__reconstruct_path(came_from,v)
                #ld("bfs: steps=%d v=%s, score=%d(%d), %s",search_counter,v,vs,len(pth),pth)
                #ld("bfs: steps=%d v=%s, score=%d",search_counter,v,vs)
                return (vs,pth)
            for w in self.neighbors(v):
                if w not in seen:
                    Q.append((w,vs+1))
                    came_from[w]=v
                    if(w in goals):
                        pth = self.__reconstruct_path(came_from,w)
                        #ld("bfs: steps=%d v=%s, score=%d(%d), %s",search_counter,v,vs,len(pth),pth)
                        #HACK
                        return (vs+1,pth) # really! fuck it
        #ld("bfs: NO PATH FOUND")
        return (self.MAXPATH,[start])

    def reset_a_star(self):
        self.closedset = set()
        self.openset = set()
        self.g_score = {}
        self.f_score = {}
        self.h_score = {}
    def a_star(self, start, goals):
        #ld("a_star: %s->%s",start,goals)
        if start in goals: return (0,[start])
        search_counter = 0
        came_from = {}

        ### I think I can reuse some of this too ... ###
        self.closedset = set()
        self.openset = set()
        self.g_score = {}
        self.f_score = {}

        self.closedset.discard(start)
        self.openset.add(start)
        self.g_score[start] = 0
        self.h_score[start] = min([self.distance(start,goal) for goal in goals])
        self.f_score[start] = self.g_score[start]+self.h_score[start]
        def best_guess():
            #blah = sorted([(self.f_score[a],a) for a in self.openset])
            mv,m = min([(self.f_score[a],a) for a in self.openset])
            #ld("best_guess: %s:%s:%s",m,mv,blah)
            return m
        def hueristic(y,goals):
            if y in self.h_score: return self.h_score[y]
            return min([self.distance(y,goal) for goal in goals]) * (1.0+self.A_STAR_TIE_BREAKER)

        while self.openset:
            search_counter+=1
            x = best_guess()
            x_score = self.f_score[x]
            if x in goals: 
                pth = self.__reconstruct_path(came_from,x)
                #ld("a_star: steps=%d score=%d(%d) path: %s",
                #        search_counter,x_score, len(pth),pth )
                # :TODO: update h_score with more accurate score?
                return (x_score,pth)
            #if self.time_remaining() < 30 or search_counter > 2**11:
            #    pth = self.__reconstruct_path(came_from,x)
            #    ld("a_star:Search too far: %s@%s (%s)",x_score,pth,search_counter) 
            #    return (x_score,pth)
            self.openset.remove(x)
            self.closedset.add(x)
            for y in self.neighbors(x):
                if y in self.closedset: continue
                tenative_g_score = self.g_score[x] + 1
                tenative_is_better = False
                if y not in self.openset:
                    self.openset.add(y)
                    tenative_is_better = True
                elif tenative_g_score < self.g_score[y]:
                    tenative_is_better = True
                if tenative_is_better:
                    came_from[y] = x
                    self.g_score[y] = tenative_g_score
                    self.h_score[y] = hueristic(y,goals)
                    self.f_score[y] = self.g_score[y] + self.h_score[y]
        return (self.MAXPATH,[start])

    def path(self, start, goals):
        # do a path search in the map here
        return self.a_star(start,goals)
        #return self.bfs(start,goals)

    def distance(self, loc1, loc2):
        'calculate the closest distance between to locations'
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

    def generate_circle_per(self,rad_2):
        circle = set()
        mx = int(sqrt(rad_2))
        inrad_2 = (mx-1)**2
        ld("circle_per mx: %s, r: %s r2: %s",mx,rad_2,inrad_2)
        for d_row in range(-mx,mx+1):
            for d_col in range(-mx,mx+1):
                d = d_row**2 + d_col**2
                #if inrad_2 < d <= rad_2:
                #    x,y=d_row,d_col
                #    circle.add((x%self.rows-self.rows,y%self.cols-self.cols))
                if d <= rad_2:
                    x,y=d_row,d_col
                    circle.add((x%self.rows-self.rows,y%self.cols-self.cols))
                    x,y=0-d_row,d_col
                    circle.add((x%self.rows-self.rows,y%self.cols-self.cols))
                    x,y=d_row,0-d_col
                    circle.add((x%self.rows-self.rows,y%self.cols-self.cols))
                    x,y=0-d_row,0-d_col
                    circle.add((x%self.rows-self.rows,y%self.cols-self.cols))

                    y,x=d_row,d_col
                    circle.add((x%self.rows-self.rows,y%self.cols-self.cols))
                    y,x=0-d_row,d_col
                    circle.add((x%self.rows-self.rows,y%self.cols-self.cols))
                    y,x=d_row,0-d_col
                    circle.add((x%self.rows-self.rows,y%self.cols-self.cols))
                    y,x=0-d_row,0-d_col
                    circle.add((x%self.rows-self.rows,y%self.cols-self.cols))
                    break
        return sorted([a for a in circle])
    def generate_circle_fill(self,rad_2):
        circle = []
        mx = int(sqrt(rad_2))
        ld("circle mx: %s, r: %s",mx,rad_2)
        for d_row in range(-mx,mx+1):
            for d_col in range(-mx,mx+1):
                d = d_row**2 + d_col**2
                if d <= rad_2:
                    circle.append((
                        d_row%self.rows-self.rows,
                        d_col%self.cols-self.cols
                    ))
        return circle
    def generate_vision_offsets_per(self):
        if not hasattr(self, 'vision_offsets_per_2'):
            self.vision_offsets_per_2 = self.generate_circle_per(self.viewradius2)
            #ld("Visible offests peremiter: %s",self.vision_offsets_per_2)
            #import pylab
            #pylab.clf()
            #pylab.scatter(*zip(*self.vision_offsets_per_2))
            #pylab.savefig("/tmp/HeatMap/VisiblePer.png")
    def generate_vision_offsets(self):
        if not hasattr(self, 'vision_offsets_2'):
            self.vision_offsets_2 = self.generate_circle_fill(self.viewradius2)
            #ld("Visible offests: %s",self.vision_offsets_2)
            #import pylab
            #pylab.clf()
            #pylab.scatter(*zip(*self.vision_offsets_2))
            #pylab.savefig("/tmp/HeatMap/Visible.png")
    def generate_attack_offsets(self):
        if not hasattr(self, 'attack_offsets_2'):
            self.attack_offsets_2 = self.generate_circle_fill(self.attackradius2)
            #ld("Attackable offests: %s",self.attack_offsets_2)
            #import pylab
            #pylab.clf()
            #pylab.scatter(*zip(*self.attack_offsets_2))
            #pylab.savefig("/tmp/HeatMap/attack.png")
    def attackable_from(self, loc):
        ' determine which squares are attackable from loc '
        self.generate_attack_offsets()
        a_row, a_col = loc
        this_attack = []
        for v_row, v_col in self.attack_offsets_2:
            this_attack.append( ((a_row+v_row)%self.rows,(a_col+v_col)%self.cols) )
        return this_attack
    def genereate_vision(self):
        if self.vision == None:
            self.generate_vision_offsets()
            # set all spaces as not visible
            # loop through ants and set all squares around ant as visible
            self.vision = [[False]*self.cols for row in range(self.rows)]
            for ant in self.my_ants():
                a_row, a_col = ant
                for v_row, v_col in self.vision_offsets_2:
                    self.vision[a_row+v_row][a_col+v_col] = True

    def visible(self, loc):
        ' determine which squares are visible to the given player '
        self.genereate_vision()
        row, col = loc
        return self.vision[row][col]

    def visible_from_per(self, loc):
        self.generate_vision_offsets_per()
        return self.__visible_from(loc,self.vision_offsets_per_2)
    def visible_from(self, loc):
        ' determine which squares are visible to the given player '
        self.generate_vision_offsets()
        return self.__visible_from(loc,self.vision_offsets_2)
    def __visible_from(self,loc,offsets):
        # set all spaces as not visible
        # loop through ants and set all squares around ant as visible
        a_row, a_col = loc
        this_vision = []
        for v_row, v_col in offsets:
            this_vision.append( ((a_row+v_row)%self.rows,(a_col+v_col)%self.cols) )
        return this_vision
    
    def render_text_map(self):
        'return a pretty string representing the map'
        tmp = ''
        for row in self.map:
            tmp += '+ %s\n' % ''.join([MAP_RENDER[col] for col in row])
        return tmp

    def check_unseen(self,cells):
        return UNSEEN in [self.map[r][c] for (r,c) in cells]

    # static methods are not tied to a class and don't have self passed in
    # this is a python decorator
    @staticmethod
    def run(bot,debug=False):
        'parse input, update game state and call the bot classes do_turn method'
        if debug:
            hdlr = logging.FileHandler('/tmp/MyBot.log')
            formatter = logging.Formatter('%(relativeCreated)d %(levelname)s %(message)s')
            hdlr.setFormatter(formatter)
            logger.addHandler(hdlr)
            logger.propagate = False

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
                break
