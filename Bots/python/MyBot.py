#!/usr/bin/env python
from ants import *
import random
import logging
import sys
from optparse import OptionParser

################################################################################
logLevel = logging.DEBUG
logging.basicConfig()
logger = logging.getLogger("MyBot")
logger.setLevel(logLevel)

hdlr = logging.FileHandler('/tmp/MyBot.log')
formatter = logging.Formatter('%(relativeCreated)d %(levelname)s %(message)s')
hdlr.setFormatter(formatter)
logger.addHandler(hdlr)
logger.propagate = False


ld = logger.debug
li = logger.info
lw = logger.warning
################################################################################

import signal
class TimeoutException(Exception):
    pass
def timeout_handler(signum, frame):
    raise TimeoutException()
def exit_handler(signum, frame):
    sys.exit("WHOOPS, die")

# :TODO:
#   a_star, reuse f_score table for repeated searches in a round? Goals are always same.


# define a class with a do_turn method
# the Ants.run method will parse and update bot input
# it will also run the do_turn method for us
class MyBot:
    def __init__(self):
        # define class level variables, will be remembered between turns
        self.velocities = dict()
        self.visited_map = set()
        self.known_bases = set()
        self.turn_count=0
        self.turn_time=0
        signal.signal(signal.SIGALRM, timeout_handler)
        signal.signal(signal.SIGTERM, exit_handler)
        pass

    # do_setup is run once at the start of the game
    # after the bot has received the game settings
    # the ants class is created and setup by the Ants.run method
    def do_setup(self, ants):
        # initialize data structures after learning the game settings
        self.rows= ants.rows
        self.cols= ants.cols
        self.NEAR = 10 # ants.viewradius2 #??
        pass

    def send_ant(self,ants,a,loc,destinations=[]):
        if a==loc: return a
        #ld("send_ant: %s -> %s", a,loc)
        directions = ants.direction(a, loc)
        if len(directions)!=1: ld("hmmm, unexpected")
        d=directions[0]
        if ants.unoccupied(loc) and not loc in destinations:
            destinations.append(loc)
            ants.issue_order((a, d))
            self.update_vel(a,loc,d)
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

    def good_stuff(self,ants):
        hills = self.known_bases
        food = ants.food()
        #CORNER CASE: Once we are out of bases, don't bother with food if there
        #             are hills left to attack.
        if hills and not ants.my_hills(): food = []
        return food + [x for x in hills] + [a for a in self.attacking_enemys]
    # do turn is run once per turn
    # the ants class has the game state and is updated by the Ants.run method
    # it also has several helper methods to use
    def do_turn(self, ants):
        self.turn_count+=1
        t1 = time.time()
        ld("--------------------%d-------------------- %s",self.turn_count, self.turn_time)
        try:
            timeout = float(ants.time_remaining() - 15)/1000
            a = signal.setitimer(signal.ITIMER_REAL, timeout)
            ld("timeout: %s",timeout)
            # loop through all my ants and try to give them orders
            # the ant_loc is an ant location tuple in (row, col) form
            destinations = []

            self.good_cache={}

            self.round_ants = ants.my_ants()
            def build_atacking_enemy_table():
                self.attacking_enemys = set()
                for h in ants.my_hills():
                    for a,_ in ants.enemy_ants():
                        if ants.distance(h,a)<self.NEAR:
                            self.attacking_enemys.add(a)

            def build_visibility_table():
                self.visible = defaultdict(lambda:0)
                for a in ants.my_ants():
                    for v in ants.visible_from(a):
                        self.visible[a]+=1

            def update_bases():
                dead_bases = []
                for h in self.known_bases:
                    if h not in ants.enemy_hills() and ants.visible(h):
                        dead_bases.append(h)
                #  remove dead bases
                for h in dead_bases:
                    self.known_bases.remove(h)
                #  add new bases
                for h in ants.enemy_hills():
                    self.known_bases.add(h[0])

            build_atacking_enemy_table()
            build_visibility_table()
            update_bases()

            # Decide which ants to do path finding on (lets say 3 closest to any object)
            self.ants_to_search = []
            for reward_loc in self.good_stuff(ants):
                ant_dist = []
                for ant_loc in ants.my_ants():
                    dist = ants.distance(ant_loc, reward_loc)
                    ant_dist.append((dist, ant_loc, reward_loc))
                ant_dist.sort()
                self.ants_to_search += [a for _,a,_ in ant_dist[0:4]]

            #ants_for_sale = [[( (d,self.objective_function(ants,a,d)), a ) for d in ants.neighbors(a)] for a in self.round_ants]

            for ant_loc in self.round_ants:
                self.visited_map.add(ant_loc)
                n = [a for a in ants.neighbors(ant_loc) if not a in ants.my_hills() and ants.unoccupied(a)]
                for score,new_loc in self.choose_move(ants,ant_loc,n):
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
            signal.setitimer(signal.ITIMER_REAL, 0)
        except TimeoutException:
            ld("You almost expired!!")
            pass
        t2 = time.time()
        self.turn_time=t2-t1

    # Define: objfuncs return HIGH value for BADness, LOW value for GOODness (can be negative)
    def good_objfunc(self,ants,ant_pos,pos):
        if not self.good_stuff(ants):
            return ants.MAXPATH
        ### HACK:
        if ant_pos:
            if ant_pos not in self.ants_to_search:
                #ld("good_obj: DIST`%s->%s %s", ant_pos, pos, self.ants_to_search)
                return ants.MAXPATH #return ants.distance(ant_pos,pos)
            if ant_pos not in self.good_cache:
                dists = sorted([(ants.distance(ant_pos,f),f) for f in self.good_stuff(ants)])
                gs = [f for _,f in dists]
                self.good_cache[ant_pos] = ants.path(ant_pos,gs)
            the_score,the_path = self.good_cache[ant_pos]
            #ld("good_obj: PATH %s->%s (%s:%s)", ant_pos, pos, the_path, the_score)
            if len(the_path)>1 and the_path[0]==ant_pos: the_path = the_path[1::] # Pop start position from path
            if pos == the_path[0]: return the_score
            return ants.MAXPATH # wouldnt choose it anyway (FIXME: causes problems when first choice cannot be taken)
        dists = sorted([(ants.distance(pos,f),f) for f in self.good_stuff(ants)])
        gs = [f for _,f in dists]
        the_score,the_path = ants.path(pos,gs)
        return the_score
    def momentum_objfunc(self,ants,ant_pos,pos):
        """Prefer same direction"""
        if not ant_pos: return ants.MAXPATH
        d = self.get_vel_dir(ant_pos)
        if d and ants.destination(ant_pos, d) == pos: return ants.MAXPATH -2
        return ants.MAXPATH
    def visibility_objfunc(self,ants,ant_pos,pos):
        # first completely unseen:
        if ants.check_unseen(ants.visible_from_per(pos)):
            return ants.MAXPATH - 4
        # TODO total visibility expansion?
        for xy in ants.visible_from_per(pos):
            if self.visible[xy] == 0: return ants.MAXPATH-1
        return ants.MAXPATH

    def explor_objfunc(self,ants,ant_pos,pos):
        """Prefer unseen areas"""
        if pos in self.visited_map: return self.momentum_objfunc(ants,ant_pos,pos)
        return ants.MAXPATH-1

    def choose_move(self,ants,ant_loc,nei):
        of = [self.good_objfunc, self.explor_objfunc, self.visibility_objfunc]
        mv = []
        for f in of:
            mv += [(f(ants,ant_loc,n),n) for n in nei]
        mv.sort()
        #ld("choose_move: %s->%s", ant_loc,mv)
        return mv

    def objective_function(self, ants, ant_pos, pos):
        #if not ants.passable(pos): return ants.MAXPATH*2
        #if pos in ants.my_hills(): return ants.MAXPATH*2
        of = [self.good_objfunc, self.explor_objfunc, self.visibility_objfunc]
        o = [f(ants,ant_pos,pos) for f in of]
        #o = [self.of_call(ants,ant_pos,pos,f) for f in of]
        ld("obj_func: %s->%s=%s",ant_pos,pos,o)
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
    except Exception as e:
        traceback.print_exc(file=sys.stderr)
