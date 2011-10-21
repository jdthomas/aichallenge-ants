#!/usr/bin/env python
from ants import *
import random
import logging
import sys
from optparse import OptionParser

logLevel = logging.DEBUG
logging.basicConfig()
logger = logging.getLogger("MyBot")
logger.setLevel(logLevel)

ld = logger.debug
li = logger.info
lw = logger.warning

Antimation = None
try:
    from __MyAntimation import Antimation
except ImportError:
    pass


# define a class with a do_turn method
# the Ants.run method will parse and update bot input
# it will also run the do_turn method for us
class MyBot:
    def __init__(self):
        # define class level variables, will be remembered between turns
        self.velocities = dict()
        self.seen_map = set()
        self.visited_map = set()
        self.known_bases = set()
        self.Debug = None
        if Antimation:
            ld("Got your debug code...")
            self.Debug = Antimation()
            self.DebugInfo={}
        pass
    
    # do_setup is run once at the start of the game
    # after the bot has received the game settings
    # the ants class is created and setup by the Ants.run method
    def do_setup(self, ants):
        # initialize data structures after learning the game settings
        self.MAXPATH = (ants.rows + ants.cols)*2
        self.rows= ants.rows
        self.cols= ants.cols
        self.NEAR = 10 # ants.viewradius2 #??
        pass

    def send_ant(self,ants,a,loc,destinations=[]):
        directions = ants.direction(a, loc)
        random.shuffle(directions)
        for direction in directions:
            new_loc = ants.destination(a, direction)
            if ants.passable(new_loc) and not new_loc in destinations:
                destinations.append(new_loc)
                ants.issue_order((a, direction))
                self.update_vel(a,new_loc,direction)
                #ld("a: %s -> %s", a, new_loc)
                return a
        return None

    def near_home(self,ants,pos):
        for h in ants.my_hills():
            if ants.distance(pos,h) < self.NEAR: return True
        return False

    def find_close_food(self,ants,pos):
        ant_dist = []
        for food_loc in ants.food():
            for ant_loc in ants.my_ants():
                dist = ants.distance(ant_loc, food_loc)
                ant_dist.append((dist, ant_loc, food_loc))
        ant_dist.sort()
        return ant_dist[0:3]

    # do turn is run once per turn
    # the ants class has the game state and is updated by the Ants.run method
    # it also has several helper methods to use
    def do_turn(self, ants):
        #ld("------------------------------")
        # loop through all my ants and try to give them orders
        # the ant_loc is an ant location tuple in (row, col) form
        destinations = []

        self.round_ants = ants.my_ants()
        if self.Debug:
            self.DebugInfo={}
            self.DebugInfo["round_scores"]= []
            self.DebugInfo["rows"]=self.rows
            self.DebugInfo["cols"]=self.cols

        # Update known bases
        #  remove dead bases
        dead_bases = []
        for h in self.known_bases:
            if h not in ants.enemy_hills() and ants.visible(h):
                dead_bases.append(h) 
        for h in dead_bases:
            self.known_bases.remove(h)
        #  add new bases
        for h in ants.enemy_hills():
            self.known_bases.add(h[0])

        # Add all seen squares to the seen_map
        for r in range(ants.rows):
            for c in range(ants.cols):
                if ants.visible((r,c)): self.seen_map.add((r,c))
        #ld(self.seen_map)

        self.close_food = {}
        for ant_loc in self.round_ants:
            #self.close_food[ant_loc] = self.find_close_food(ants,ant_loc)
            self.visited_map.add(ant_loc)
            # try all directions in given order
            directions = ['n','e','s','w']
            #random.shuffle(directions)
            pd = [(d,self.objective_function(ants,ant_loc,d)) for d in ants.neighbors(ant_loc)]
            #pd = [(d,self.objective_function(ants,ant_loc,d)) for d in map(lambda x: ants.destination(ant_loc,x), directions) if ants.passable(d)]
            pd.append( (ant_loc,self.objective_function(ants,ant_loc,ant_loc)) ) # stay put?
            pd.sort(key=lambda x: x[1])
            #new_loc,score = min(pd,key=lambda x: x[1])
            #ld("new_loc: %s score:%s", new_loc, score)
            for new_loc,score in pd:
                x = self.send_ant(ants,ant_loc,new_loc,destinations)
                if x:
#                    ld("moved: %s->%s '%s' for %s", ant_loc,new_loc,ants.direction(ant_loc, new_loc), score)
                    break
#                else: ld("didn't move %s->%s", ant_loc,new_loc)

            # check if we still have time left to calculate more orders
            if ants.time_remaining() < 10:
#                ld("OH FUCK, OUT OF TIME!!!")
                break
        ### Draw my heat map
        if self.Debug:
            #A = [[0 for col in range(self.cols)] for row in range(self.rows)] #A = NP.zeros((self.rows,self.cols))
            #mi,ma = 9999999,0
            #for x in range(self.rows):
            #    for y in range(self.cols):
            #        A[x][y] = 1 #self.objective_function(ants,None,(x,y))
            #        #ld("loop: %d %d", x,y)
            #        #A[x][y] = self.objective_function(ants,None,(x,y))
            #        ma = max(ma,A[x][y])
            #        mi = min(mi,A[x][y])
            #self.DebugInfo["all_objective"] = A
            self.Debug.put(self.DebugInfo)

    # Define: objfuncs return HIGH value for BADness, LOW value for GOODness (can be negative)
    def enemy_objfunc(self, ants, ant_pos, pos):
        """ Attack enemy close to home, but avoid when exploring.
        :TODO: If we can attack with TWO+ at once then we don't loose an ant,
               take this into account."""
        return self.MAXPATH                           # don't care
        if not ants.enemy_ants(): return self.MAXPATH
        nh = self.near_home(ants,pos)
        dist = min([x for x in map(lambda a: ants.distance(pos,a[0]), ants.enemy_ants())])
        if nh and dist < 10: return 0-self.MAXPATH    # defend home
        if not nh and dist < 3: return 2*self.MAXPATH # prefer life
        return self.MAXPATH                           # don't care
    def food_objfunc(self,ants,ant_pos,pos):
        """ object function for food. Closest ant should go for the food"""
        dists = sorted([(ants.distance(pos,f),f) for f in ants.food()])[0:3]
        return min([self.MAXPATH] + [ants.path(pos,f) for _,f in dists])
        #return min([self.MAXPATH] + [ants.path(pos,f) for f in ants.food()])
    def hill_objfunc(self,ants,ant_pos,pos):
        #hills = ants.enemy_hills()
        hills = self.known_bases
        dists = sorted([(ants.distance(pos,h),h) for h in hills])[0:3]
        return min([self.MAXPATH] + [ants.path(pos,h) for _,h in dists])/2
        #return min([self.MAXPATH] + [ants.path(pos,h) for h in hills])
        ## :FIXME: divide by two, will walk twice as far to find hill as find food
    def friend_objfunc(self,ants,pos):
        """AVOID running into each other"""
        pass
    def momentum_objfunc(self,ants,ant_pos,pos):
        """Prefer same direction"""
        if not ant_pos: return self.MAXPATH
        d = self.get_vel_dir(ant_pos)
        if d and ants.destination(ant_pos, d) == pos: return self.MAXPATH -1
        return self.MAXPATH
    def visability_objfunc(self,ants,ant_pos,pos):
        if not ant_pos: return self.MAXPATH
        visible_others = set()
        my_ants = [a for a in ants.my_ants()]
        my_ants.remove(ant_pos)
        for a in my_ants:
            for v in ants.visible_from(a):
                visible_others.add(v)
        visible_now = set(visible_others)
        for v in ants.visible_from(ant_pos):
            visible_now.add(v)
        visible_next = set(visible_others)
        for v in ants.visible_from(pos):
            visible_next.add(v)
        if len(visible_next - visible_now) > len(visible_now-visible_next): 
            #ld("Vis increase")
            # :TODO: hmmm, how to map "more visibility" into my scoreing
            #        fucntion of "distance to something good"
            return self.MAXPATH/2
        return self.MAXPATH

    def explor_objfunc(self,ants,ant_pos,pos):
        """Prefer unseen areas"""
        #if pos in self.seen_map: return self.MAXPATH
        #ld(self.visited_map)
        #if pos in self.visited_map: return self.MAXPATH
        if pos in self.visited_map: return self.momentum_objfunc(ants,ant_pos,pos)
        return self.MAXPATH-2

    # :TODO: Travel in groups of 3+?
    # :TODO: Assign ants types: HUNGRY, FIGHTER, BOMBER, DEFENCE .. proportion something like 40,10,40,10
    def objective_function(self, ants, ant_pos, pos):
        if not ants.passable(pos): return self.MAXPATH*2
        if pos in ants.my_hills(): return self.MAXPATH*2
        of = [self.enemy_objfunc,self.food_objfunc,self.hill_objfunc,self.explor_objfunc,self.visability_objfunc]
        o = [f(ants,ant_pos,pos) for f in of]
        if self.Debug:
            self.DebugInfo["round_scores"].append((pos[0],pos[1],min(o)))
#        ld("o=%s",o)
        return min(o)

    def update_vel(self,ant_loc,new_loc,direction):
        vl = 1
        if ant_loc in self.velocities:
            v = self.velocities[ant_loc]
            self.velocities.pop(ant_loc)
            if direction == v[0]: vl += v[1]
        self.velocities[new_loc] = (direction,vl)

    def get_vel_dir(self,ant_loc):
        if ant_loc and ant_loc in self.velocities and self.velocities[ant_loc][1]>=1:
            return self.velocities[ant_loc][0]
        return None

            
if __name__ == '__main__':
    # psyco will speed up python a little, but is not needed
    try:
        import psyco
        psyco.full()
    except ImportError:
        pass
    
    try:
        # if run is passed a class with a do_turn method, it will do the work
        # this is not needed, in which case you will need to write your own
        # parsing function and your own game state class
        Ants.run(MyBot())
    except KeyboardInterrupt:
        print('ctrl-c, leaving ...')
