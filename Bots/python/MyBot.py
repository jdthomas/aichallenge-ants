#!/usr/bin/env python
from ants import *
import random
import logging
import sys
from optparse import OptionParser

logLevel = logging.DEBUG
logging.basicConfig()
logger = logging.getLogger("ConsoleLog")
logger.setLevel(logLevel)

ld = logger.debug
li = logger.info
lw = logger.warning

## JT IDEA 1:
# Goals: 1. Find food, 2. Attack enemy base
# Based off of PSO?
#
# Ants follow other ants if they are or are following a "leader" 
# Once enemy base is located, leader ants use a* to go to it.


def manhattanDistance( xy1, xy2 ):
  "Returns the Manhattan distance between points xy1 and xy2"
  return abs( xy1[0] - xy2[0] ) + abs( xy1[1] - xy2[1] )


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
        self.fnum=0
        pass
    
    # do_setup is run once at the start of the game
    # after the bot has received the game settings
    # the ants class is created and setup by the Ants.run method
    def do_setup(self, ants):
        # initialize data structures after learning the game settings
        self.MAXPATH = ants.rows + ants.cols + 1
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

    def neighbors(self, ants, group):
        pass

    def drawHeatMapAll(self,ants):
        from matplotlib import pyplot as PLT
        from matplotlib import cm as CM
        from matplotlib import mlab as ML
        import numpy as NP

        A = NP.zeros((self.rows,self.cols))
        mi,ma = 9999999,0
        for x in range(self.rows):
            for y in range(self.cols):
                A[x][y] = self.objective_function(ants,None,(x,y))
                ma = max(ma,A[x][y])
                mi = min(mi,A[x][y])
        gridsize=[self.rows,self.cols]
        PLT.subplot(111)
        # if "bins=None", then color of each hexagon corresponds directly to its count
        # "C" is optional--it maps values to x, y coordinates; if C is None (default) then 
        # the result is a pure 2D histogram 
        #PLT.hexbin(x, y, C=z, gridsize=gridsize, cmap=CM.jet, bins=None)
        PLT.imshow( A, cmap=CM.jet,  interpolation='nearest', vmin=mi, vmax=ma )
        #PLT.imshow( A, cmap=CM.jet,  interpolation='nearest')
        #PLT.axis([min(x), max(x), min(y), max(y)])
        if 0 == self.fnum:
            cb = PLT.colorbar()
            cb.set_label('mean value')
        self.fnum+=1
        PLT.savefig("/tmp/plt_%s.png"%self.fnum)
    def drawHeatMap(self,ants):
        return
        from matplotlib import pyplot as PLT
        from matplotlib import cm as CM
        from matplotlib import mlab as ML
        import numpy as NP

        x = [ a[0] for a in self.round_scores ]
        y = [ a[1] for a in self.round_scores ]
        z = [ a[2] for a in self.round_scores ]

        A = NP.zeros((self.rows,self.cols))
        for s in self.round_scores:
            A[s[0]][s[1]] = float(s[2]- min(z))/max(z) 

        ld("x: %s",x)
        ld("y: %s",y)
        ld("z: %s",z)

        gridsize=[self.rows,self.cols]
        PLT.subplot(111)
        # if "bins=None", then color of each hexagon corresponds directly to its count
        # "C" is optional--it maps values to x, y coordinates; if C is None (default) then 
        # the result is a pure 2D histogram 
        #PLT.hexbin(x, y, C=z, gridsize=gridsize, cmap=CM.jet, bins=None)
        #PLT.imshow( A, cmap=CM.jet,  interpolation='nearest', vmin=min(z), vmax=max(z) )
        PLT.imshow( A, cmap=CM.jet,  interpolation='nearest')
        #PLT.axis([x.min(), x.max(), y.min(), y.max()])
        if 0 == self.fnum:
            cb = PLT.colorbar()
            cb.set_label('mean value')
        self.fnum+=1
        PLT.savefig("/tmp/plt_%s.png"%self.fnum)
    
    # do turn is run once per turn
    # the ants class has the game state and is updated by the Ants.run method
    # it also has several helper methods to use
    def do_turn(self, ants):
        ld("------------------------------")
        # loop through all my ants and try to give them orders
        # the ant_loc is an ant location tuple in (row, col) form
        destinations = []

        self.round_ants = ants.my_ants()
        self.round_scores = []

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

        for ant_loc in self.round_ants:
            self.visited_map.add(ant_loc)
            # try all directions in given order
            directions = ['n','e','s','w']
            #random.shuffle(directions)
            pd = [(d,self.objective_function(ants,ant_loc,d)) for d in map(lambda x: ants.destination(ant_loc,x), directions) if ants.passable(d)]
            pd.append( (ant_loc,self.objective_function(ants,ant_loc,ant_loc)) ) # stay put?
            pd.sort(key=lambda x: x[1])
            #new_loc,score = min(pd,key=lambda x: x[1])
            #ld("new_loc: %s score:%s", new_loc, score)
            for new_loc,score in pd:
                x = self.send_ant(ants,ant_loc,new_loc,destinations)
                if x:
                    ld("moved: %s->%s '%s' for %s", ant_loc,new_loc,ants.direction(ant_loc, new_loc), score)
                    break
                else: ld("didn't move %s->%s", ant_loc,new_loc)

            # check if we still have time left to calculate more orders
            if ants.time_remaining() < 10:
                ld("OH FUCK, OUT OF TIME!!!")
                break
        ### Draw my heat map
        self.drawHeatMapAll(ants)

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
        return min([self.MAXPATH] + [manhattanDistance(pos,f) for f in ants.food()])
    def hill_objfunc(self,ants,ant_pos,pos):
        #hills = ants.enemy_hills()
        hills = self.known_bases
        return min([self.MAXPATH] + [manhattanDistance(pos,h) for h in hills])
    def friend_objfunc(self,ants,pos):
        """AVOID running into each other"""
        pass
    def momentum_objfunc(self,ants,ant_pos,pos):
        """Prefer same direction"""
        d = self.get_vel_dir(ant_pos)
        if d and ants.destination(ant_pos, d) == pos: return self.MAXPATH -1
        return self.MAXPATH
    def explor_objfunc(self,ants,ant_pos,pos):
        """Prefer unseen areas"""
        #if pos in self.seen_map: return self.MAXPATH
        #ld(self.visited_map)
        #if pos in self.visited_map: return self.MAXPATH
        if pos in self.visited_map: return self.momentum_objfunc(ants,ant_pos,pos)
        return self.MAXPATH-2

    # :TODO: Travel in groups of 3+
    # :TODO: Keep as much screen visible as possible
    # :TODO: Assign ants types: HUNGRY, FIGHTER, BOMBER, DEFENCE .. proportion something like 40,10,40,10
    def objective_function(self, ants, ant_pos, pos):
        of = [self.enemy_objfunc,self.food_objfunc,self.hill_objfunc,self.explor_objfunc]
        o = [f(ants,ant_pos,pos) for f in of]
        self.round_scores.append((pos[0],pos[1],min(o)))
        #ld("o=%s",o)
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
