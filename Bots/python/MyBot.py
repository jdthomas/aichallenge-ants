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
        pass

    def send_ant(self,ants,a,loc,destinations=[]):
        directions = ants.direction(a, loc)
        random.shuffle(directions)
        for direction in directions:
            new_loc = ants.destination(a, direction)
            if ants.unoccupied(new_loc) and not new_loc in destinations:
                destinations.append(new_loc)
                ants.issue_order((a, direction))
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
    
    # do turn is run once per turn
    # the ants class has the game state and is updated by the Ants.run method
    # it also has several helper methods to use
    def do_turn(self, ants):
        # loop through all my ants and try to give them orders
        # the ant_loc is an ant location tuple in (row, col) form
        destinations = []

        # 0. if near base attack ants if away don't
        near_home = dict()
        for a in ants.my_ants():
            for h in ants.my_hills():
                if ants.distance(a,h) < 10:
                    near_home[a] = True
                else:
                    near_home[a] = False
        #ld("NH: %s",near_home)

        # 1. Miso hungry, attack food first with closest ant
        round_ants = ants.my_ants()
        for f in ants.food():
            # pop closest ant from all round_ants, assign it to food
            min_ant = None
            min_dist = 999999999999 #sys.maxint FIXME
            for a in round_ants:
                d = manhattanDistance(f,a)
                if d < min_dist:
                    min_ant = a
                    min_dist = d
            if min_ant:
                a = self.send_ant(ants,min_ant,f,destinations)
                if a:
                    round_ants.remove(min_ant)
                #directions = ants.direction(min_ant, f)
                #random.shuffle(directions)
                #for direction in directions:
                #    new_loc = ants.destination(min_ant, direction)
                #    if ants.unoccupied(new_loc) and not new_loc in destinations:
                #        destinations.append(new_loc)
                #        ants.issue_order((min_ant, direction))
                #        round_ants.remove(min_ant)
                #        break
                ld("a: %s -> %s", min_ant, f)

        # defend base?
        if near_home:
            for ant_loc in round_ants:
                if near_home[ant_loc]:
                    for ea,_ in ants.enemy_ants():
                        ld("ea: %s a: %s d: %d", ea,ant_loc,ants.distance(ant_loc,ea))
                        if ants.distance(ant_loc,ea) < 10:
                            x = self.send_ant(ants,ant_loc,ea,destinations)
                            if x:
                                round_ants.remove(ant_loc)
                            ld("***** attack *****")
                            #break

        for ant_loc in round_ants:
            # try all directions in given order
            directions = ['n','e','s','w']
            random.shuffle(directions)
            # Add current direction with 90%
            if random.random() < 0.9:
                directions = self.get_vel_dirs(ant_loc) + directions
            ld('posible dirs:%s',directions)
            for direction in directions:
                # the destination method will wrap around the map properly
                # and give us a new (row, col) tuple
                new_loc = ants.destination(ant_loc, direction)
                # passable returns true if the location is land
                if (ants.passable(new_loc)) and not new_loc in destinations:
                    # an order is the location of a current ant and a direction
                    ants.issue_order((ant_loc, direction))
                    destinations.append(new_loc)
                    self.update_vel(ant_loc,new_loc,direction)
                    # stop now, don't give 1 ant multiple orders
                    break
            # check if we still have time left to calculate more orders
            if ants.time_remaining() < 10:
                break

    def objective_function(self, ants, pos):
        if not ants.enemy_hills(): return 0
        m=0
        for h,_ in ants.enemy_hills():
            m = min(manhattanDistance(pos,h),m)
        return m
            
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
