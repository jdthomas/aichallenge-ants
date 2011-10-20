#!/usr/bin/env python
from ants import *
import random
import logging
import sys
from optparse import OptionParser

logLevel = logging.DEBUG
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
        pass
    
    # do_setup is run once at the start of the game
    # after the bot has received the game settings
    # the ants class is created and setup by the Ants.run method
    def do_setup(self, ants):
        # initialize data structures after learning the game settings
        self.MAXPATH = sqrt(ants.rows**2 + ants.cols**2)
        self.NEAR = 10 # ants.viewradius2 #??
        pass

    def send_ant(self,ants,a,loc,destinations=[]):
        directions = ants.direction(a, loc)
        random.shuffle(directions)
        for direction in directions:
            new_loc = ants.destination(a, direction)
            if ants.unoccupied(new_loc) and not new_loc in destinations:
                destinations.append(new_loc)
                ants.issue_order((a, direction))
                #self.round_ants.remove(a)
                #ld("a: %s -> %s", a, new_loc)
                return a
        return None

    def update_vel(self,ant_loc,new_loc,direction):
        vl = 1
        if ant_loc in self.velocities:
            v = self.velocities[ant_loc]
            self.velocities.pop(ant_loc)
            if direction == v[0]: vl += v[1]
        self.velocities[new_loc] = (direction,vl)

    def get_vel_dirs(self,ant_loc):
        if ant_loc in self.velocities and self.velocities[ant_loc][1]>1:
            return [self.velocities[ant_loc][0]] #*self.velocities[ant_loc][1]
        return []

    def near_home(self,ants,pos):
        for h in ants.my_hills():
            if ants.distance(pos,h) < self.NEAR: return True
        return False
    
    # do turn is run once per turn
    # the ants class has the game state and is updated by the Ants.run method
    # it also has several helper methods to use
    def do_turn(self, ants):
        ld("------------------------------")
        # loop through all my ants and try to give them orders
        # the ant_loc is an ant location tuple in (row, col) form
        destinations = []

        # 0. if near base attack ants if away don't
        #self.near_home = dict()
        #for a in ants.my_ants():
        #    for h in ants.my_hills():
        #        if ants.distance(a,h) < 10:
        #            self.near_home[a] = True
        #        else:
        #            self.near_home[a] = False
        ##ld("NH: %s",self.near_home)

        # 1. Miso hungry, attack food first with closest ant
        self.round_ants = ants.my_ants()
        #for f in ants.food():
        #    # pop closest ant from all round_ants, assign it to food
        #    min_ant = None
        #    min_dist = 999999999999 #sys.maxint FIXME
        #    for a in self.round_ants:
        #        d = manhattanDistance(f,a)
        #        if d < min_dist:
        #            min_ant = a
        #            min_dist = d
        #    if min_ant:
        #        a = self.send_ant(ants,min_ant,f,destinations)

        # defend base?
        #if self.near_home:
        #    for ant_loc in self.round_ants:
        #        if self.near_home[ant_loc]:
        #            for ea,_ in ants.enemy_ants():
        #                #ld("ea: %s a: %s d: %d", ea,ant_loc,ants.distance(ant_loc,ea))
        #                if ants.distance(ant_loc,ea) < 10:
        #                    x = self.send_ant(ants,ant_loc,ea,destinations)
        #                    ld("***** attack *****")
        #                    break

        for ant_loc in self.round_ants:
            # try all directions in given order
            directions = ['n','e','s','w']
            random.shuffle(directions)
            # Add current direction with 90%
            if random.random() < 0.9:
                directions = self.get_vel_dirs(ant_loc) + directions
            #ld('posible dirs:%s',directions)
            pd = [(d,self.objective_function(ants,d)) for d in map(lambda x: ants.destination(ant_loc,x), directions)]
            pd = [d  for d in pd if ants.unoccupied(d[0])]
            #new_loc,score = min(pd,key=lambda x: x[1])
            new_loc,score = max(pd,key=lambda x: x[1])
            #ld("new_loc: %s score:%s", new_loc, score)
            x = self.send_ant(ants,ant_loc,new_loc,destinations)
            if not x: ld("WTF")

            # check if we still have time left to calculate more orders
            if ants.time_remaining() < 10:
                ld("OH FUCK, OUT OF TIME!!!")
                break
        if self.round_ants: 
            ld("forgot to move: %s", self.round_ants)

    # Define: objfuncs return HIGH value for goodness, low value for badness (can be negative)
    def enemy_objfunc(self, ants, pos):
        """ objective function for cells based on nearness to enemy"""
        if not ants.enemy_ants(): return 0
        nh = self.near_home(ants,pos)
        dist = min([x for x in map(lambda a: ants.distance(pos,a[0]), ants.enemy_ants())])
        if nh and dist < 10: return self.MAXPATH      # defend home
        if not nh and dist < 3: return 0-self.MAXPATH # prefer life
        return 0                                      # don't care
        #num=1.0
        #if self.near_home(ants,pos): num=-4.0
        #v = sum([num/x for x in map(lambda a: ants.distance(pos,a[0]), ants.enemy_ants())])
        #return v
    def food_objfunc(self,ants,pos):
        """ object function for food. Closest ant should go for the food"""
        for f in ants.food():
            d = manhattanDistance(pos,f)
            # we can see it, assume we are closest, and go to it
            if d < ants.viewradius2: return self.MAXPATH - d
        return 0
    def hill_objfunc(self,ants,pos):
        if not ants.enemy_hills(): return 0 # no current objective
        m=0
        for h,_ in ants.enemy_hills():
            m = min(manhattanDistance(pos,h),m)
        return self.MAXPATH - m
    def objective_function(self, ants, pos):
        e = self.enemy_objfunc(ants,pos)       
        f = self.food_objfunc(ants,pos) 
        h = self.hill_objfunc(ants,pos) 
        # TODO: need randomness to our search, everyone goes for the one food item now ;)
        ld("pos: %s enmy obj: %s food obj: %s hill obj: %s", pos,e,f,h)
        o = [ x for x in [e,f,h] if x != 0] or [0]
        ld("o=%s",o)
        return o
            
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
