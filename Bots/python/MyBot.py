#!/usr/bin/env python
from ants import *
import random
import logging
from pprint import pformat, pprint
import sys
from optparse import OptionParser

################################################################################
logLevel = logging.DEBUG
logging.basicConfig()
logger = logging.getLogger("MyBot")
logger.setLevel(logLevel)


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
#   [X] Fix permeter of circle generator
#   [ ] Movement, need to handle case where will move into currently ocupied
#       cell  (ant has moved for instance)
#   [ ] Attack:
#   [ ]     Expand "danger" code to attack code.
#   [ ]     min-max attack code?
#   [ ] Weight hill attack higher than food? Send multiple ants to hills?
#   [ ] Momentum should be scored 100/50/50/0 to weight continuing on diags?
#   [ ] Symmetry detection in map:
#   [ ]     Basic detection as I exlor
#   [ ]     Predicted hills added to "good stuff" list
#   [ ]     ?? Locations that would help with the symmetry detection should be
#           weighted??
#   [ ]     Do path finding in symmetry expaned map
#   [ ] Between turn logic:
#   [ ]     If attacking hill w/o enough force (want hill, will die) bring more
#           forces
#   [ ]     Speedup of "stuff" by not redoing some calculations??
#   [ ]     Cache found shortest paths if not tainted by UNSEEN? Gather
#           statistics on how many times I re-search a path, is worthwhile?
#   [ ] Visibile_from can be MUCH faster by just checking if r**2+y**2<vr2
#   [ ] Remember invisible food, remove when visible but not there (like done
#       with hills)
#   [ ] What about Fluxid's gradient for exploring, distance to unseen squares.
#       How does he build this efficiently? BFS? Yes, BFS from all objectives,
#       update every n rounds.
#   [ ] Change ant assignments, give paths to ants, don't re-search these each
#       round. Check that the goal still exists and perform enemy
#       attack/avoidance, then regain path.
#   [ ] Better utilization of 'extra' CPU time per turn.
#   [ ] Port to c? setjmp/longjmp for timeout?

def cfDist(l1,l2):
    """Crow flight distance two points"""
    r1,c1 = l1
    r2,c2 = l2
    cd = sqrt(min(r1-r2,r2-r1)**2 + min(c1-c2,c2-c1)**2)
    #ld("crow dist: %s->%s: %s", l1,l2,cd)
    return cd

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
        if len(directions)!=1: ld("hmmm, unexpected: %s",directions)
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
            for ant_loc in self.round_ants:
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
    def nearby_ants(self, ants, loc, max_dist, exclude=None):                           
        n_ants = []                                                                 
        for a in ants.all_ants():
            if a[1] != exclude and cfDist(loc,a[0]) < max_dist:
                n_ants.append(a[0])
        return n_ants    
    def do_attack_focus(self,ants):
        # maps ants to nearby enemies
        nearby_enemies = {}
        for ant,owner in ants.all_ants():
            nearby_enemies[ant] = self.nearby_ants(ants, ant, sqrt(ants.attackradius2)+2, owner)
        ld("   nearby: %s... %s", pformat(nearby_enemies),sqrt(ants.attackradius2))
        # determine which ants to kill
        ants_to_kill = []
        for ant in [a for a in ants.my_ants()]:
            weakness = len(nearby_enemies[ant])
            if weakness == 0: continue
            min_enemy_weakness = min(len(nearby_enemies[enemy]) for enemy in nearby_enemies[ant])
            if min_enemy_weakness <= weakness: ants_to_kill.append(ant)
        return ants_to_kill

    # do turn is run once per turn
    # the ants class has the game state and is updated by the Ants.run method
    # it also has several helper methods to use
    def do_turn(self, ants):
        self.turn_count+=1
        t1 = time.time()
        try:
            timeout = float(ants.time_remaining() - 100)/1000
            a = signal.setitimer(signal.ITIMER_REAL, timeout)
            ld("--------------------%d-------------------- %s,%s",self.turn_count, self.turn_time,timeout)
            # loop through all my ants and try to give them orders
            # the ant_loc is an ant location tuple in (row, col) form
            destinations = []
            self.good_cache = {}
            self.round_ants = ants.my_ants()
            self.defenders = []

            def build_atacking_enemy_table():
                self.attacking_enemys = set()
                for h in ants.my_hills():
                    for a,_ in ants.enemy_ants():
                        if ants.distance(h,a)<self.NEAR:
                            self.attacking_enemys.add(a)

            def build_danger_table():
                self.danger = defaultdict(lambda:0)
                for e,_ in ants.enemy_ants():
                    # TODO instead of iterating neighbors, increase radius by 1
                    for a in [e]+ants.neighbors(e): # we don't know their move
                    #a = e
                        for t in ants.attackable_from(a):
                            self.danger[t]+=1
                #ld("danger: %s %s", ants.enemy_ants(), self.danger)
                #import numpy as NP
                #import pylab
                #dan = [d for d in self.danger]
                #if dan:
                #    #ld("danger: %s %s", ants.enemy_ants(), dan)
                #    pylab.clf()
                #    pylab.scatter(*zip(*dan))
                #    pylab.xlim([0,self.rows]);pylab.ylim([0,self.cols])
                #    pylab.savefig("/tmp/HeatMap/danger_%04d.png"%self.turn_count)
            def attackable1():
                self.attackables = {}
                self.attackback = defaultdict(list)
                self.attackables_all = set()
                for a in self.round_ants:
                    self.attackables[a] = set()
                    for n in ants.neighbors(a) + [a]:
                        af = ants.attackable_from(n)
                        self.attackables[a] |= set(af)
                        for x in af: self.attackback[x].append(n)
                    self.attackables_all |= self.attackables[a]
            attackable1()
            attackable_enemys = self.attackables_all.intersection([e for e,_ in ants.enemy_ants()])
            ld("enemies I can get: %s", attackable_enemys)
            for e in attackable_enemys:
                az = self.attackback[e]
                for a in ants.my_ants():
                    pass
            ###         if there exists a move where len(az) < enemys atacking a:
            ###             save as tenative
            ###         else
            ###             clear tenative, abort
            ###     do_moves to tenative

            ld("Someone might die from: %s",self.do_attack_focus(ants))
            def build_visibility_table():
                self.visible = defaultdict(lambda:0)
                for a in self.round_ants:
                    for v in ants.visible_from(a):
                        self.visible[v]+=1

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

            def defense():
                """ Once we have 10 ants, assign defense. If we have more than
                    20 ants per hill, assign 2. """
                #t1 = time.time()
                if not ants.my_hills() or len(self.round_ants)<10: return
                num_defenders_per_hill = max(1,min(2,len(self.round_ants)/(20*len(ants.my_hills()))))
                for h in ants.my_hills():
                    ant_dist = []
                    for a in self.round_ants:
                        dist = ants.distance(a, h)
                        ant_dist.append((dist, a, h))
                    ant_dist.sort()
                    for df in range(num_defenders_per_hill):
                        self.defenders.append( (ant_dist.pop(0)[1],h) )
                #t2 = time.time()
                #ld("Defenders: %s, %s", num_defenders_per_hill, t2-t1)

            build_atacking_enemy_table()
            build_visibility_table()
            build_danger_table()
            update_bases()

            ants.bfs_build_full_map(self.good_stuff(ants))

            ants.reset_a_star() ## Reset a_star as our goals are different than future uses
            defense()
            ants.reset_a_star() ## Reset a_star as our goals are different than future uses

            # Decide which ants to do path finding on (lets say 3 closest to any object)
            self.ants_to_search = set()
            for reward_loc in self.good_stuff(ants):
                ant_dist = []
                for ant_loc in self.round_ants:
                    dist = ants.distance(ant_loc, reward_loc)
                    ant_dist.append((dist, ant_loc, reward_loc))
                ant_dist.sort()
                for _,a,_ in ant_dist[0:4]:
                    self.ants_to_search.add(a)

            #ants_for_sale = [[( (d,self.objective_function(ants,a,d)), a ) for d in ants.neighbors(a)] for a in self.round_ants]
            def assign_some_ants(ant_list):
                for ant_loc in ant_list:
                    self.visited_map.add(ant_loc)
                    #n = [a for a in ants.neighbors(ant_loc) if ants.unoccupied(a)]
                    n = [a for a in ants.neighbors(ant_loc)] + [ant_loc]
                    #ld("Nei: %s:%s", ant_loc,n)
                    for score,new_loc in self.choose_move(ants,ant_loc,n):
                        x = self.send_ant(ants,ant_loc,new_loc,destinations)
                        if x:
                            self.round_ants.remove(x) # don't allow assignment elsewhere
                            #ld("moved: %s->%s '%s' for %s", ant_loc,new_loc,ants.direction(ant_loc, new_loc), score)
                            break
                        #else: ld("didn't move %s->%s", ant_loc,new_loc)

            # Iterate the path finding ants first, they are more important if we time out
            #def assignments():
            #    assign_some_ants(self.ants_to_search)
            #assignments()

            self.ants_to_explore = [a for a in self.round_ants] # rest can explore

            def explorers():
                assign_some_ants(self.ants_to_explore)
            explorers()

            signal.setitimer(signal.ITIMER_REAL, 0)
        except TimeoutException:
            ld("You almost expired!!")
            pass
        t2 = time.time()
        self.turn_time=t2-t1

    # Define: objfuncs return HIGH value for BADness, LOW value for GOODness (can be negative)
    def good_objfunc(self,ants,ant_pos,pos):
        """ Assign an ant to each food item """
        def build_goto_table():
            for g in self.good_stuff(ants):
                score,path = ants.path(g,self.round_ants)
                path.reverse()
                a = path[0]
                if a not in self.good_cache or self.good_cache[a][0] > score:
                    self.good_cache[a] = (score,path)
        if not self.good_stuff(ants): return 0.0 # No good stuff, bail
        if not self.good_cache: build_goto_table() # First ant in a round w/ good stuff
        if ant_pos not in self.good_cache: return 0.0 # Not an ant we path found on
        score,path = self.good_cache[ant_pos]
        if len(path)>1 and path[0]==ant_pos: path = path[1::] # Pop current position from path
        #ld("good_obj: %s->%s %s, %s", ant_pos, pos, score, path)
        if pos == path[0]: return (ants.MAXPATH-float(score))/ants.MAXPATH
        return 0.0
    def good_objfunc2(self,ants,ant_pos,pos):
        if not self.good_stuff(ants): return 0.0
        ANT_SEARCH_HACK=False
        ### HACK:
        if ant_pos:
            if ANT_SEARCH_HACK: search_pos = ant_pos
            else: search_pos = pos
            #ld("%s %s %s", search_pos, self.ants_to_search, search_pos in self.ants_to_search)
            #if ant_pos not in self.ants_to_search: return 0.0
            if search_pos not in self.good_cache:
                gs = [f for f in self.good_stuff(ants)]
                self.good_cache[search_pos] = ants.path(search_pos,gs)
            the_score,the_path = self.good_cache[search_pos]
            #ld("good_obj: PATH %s->%s (%s:%s)", search_pos, pos, the_path, the_score)
            if len(the_path)>1 and the_path[0]==ant_pos: the_path = the_path[1::] # Pop current position from path
            if pos == the_path[0]: return float(the_score)/ants.MAXPATH
            return 0.0 # wouldn't choose it anyway (FIXME: causes problems when first choice cannot be taken)
        else:
            # Default to Manhattan distance
            dists = sorted([(ants.distance(pos,f),f) for f in self.good_stuff(ants)])
            gs = [f for _,f in dists]
            the_score,the_path = ants.path(pos,gs)
            return float(the_score)/ants.MAXPATH
    def my_hill_objfunc(self,ants,ant_pos,pos):
        if pos in ants.my_hills(): return 0.0 # Don't sit on hill
        for da,dh in self.defenders:
            if ant_pos == da:
                dist = ants.distance(pos,dh)
                ld("hill_obj: %s->%s: %s", pos,da,dist)
                return float(10-dist)/10 # Distnace metric here should be OK, these ants will always be recently spawned
        return 0.5
    def visibility_objfunc(self,ants,ant_pos,pos):
        # first completely unseen:
        if ants.check_unseen(ants.visible_from_per(pos)):
            return 1.0
        # total visibility expansion
        cur_vis  = sum([1 for xy in ants.visible_from_per(ant_pos) if self.visible[xy] == 1])
        next_vis = sum([1 for xy in ants.visible_from_per(pos) if self.visible[xy] == 0])
        if next_vis>cur_vis: return 1.0
        return 0.0
    def momentum_objfunc(self,ants,ant_pos,pos):
        """Prefer same direction"""
        backwards = {'n':'s','e':'w','s':'n','w':'e'}
        if not ant_pos: return 0.0
        da = self.get_vel_dir(ant_pos)
        dp = ants.direction(ant_pos, pos)
        if not dp or not da: return 0.0
        dp = dp[0]
        if da == dp: return 1.0
        if da == backwards[dp]: return 0.0
        return 0.5
    def explor_objfunc(self,ants,ant_pos,pos):
        """Prefer unvisited areas"""
        ### TODO: Should be gradient of distance to unseen?
        if pos in self.visited_map: return 0.0
        return 1.0
    def enemy_objfunc(self,ants,ant_pos,pos):
        """ avoid dieing -- step function 1.0 if safe or near home (defense) 0.0 if dangerous """
        if self.near_home(ants,pos) or not self.danger[pos]:
            return 1.0
        #ld("Danger at %s",pos)
        return 0.0

    def choose_move(self,ants,ant_loc,nei):
        of = [self.objective_function]
        mv = []
        for f in of:
            mv += [(f(ants,ant_loc,n),n) for n in nei]
        mv.sort()
        mv.reverse() # MAX best
        #ld("choose_move: %s->%s", ant_loc,mv)
        return mv

    def objective_function(self, ants, ant_pos, pos):
        of = [  (self.good_objfunc       , 250) ,
                (self.visibility_objfunc , 100) ,
                (self.my_hill_objfunc    , 100) ,
                (self.momentum_objfunc   , 10)  ,
                (self.enemy_objfunc      , 500) ,
              ]
        tw = sum([w for _,w in of])
        o = [(f(ants,ant_pos,pos))*w for f,w in of]
        def tie_breaker(): return random.random()*1/10000.0
        s = sum(o) + tie_breaker()
        m = max(o)
        #ld("objectives: %s->%s=%s => %s(%s)/%s",ant_pos,pos,o,s,m,tw)
        return s

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
    parser = OptionParser()
    parser.add_option("-d", "--debug", dest='debug', action="store_true", default=False, help="Enable file logging")

    (opts, args) = parser.parse_args()

    if opts.debug:
        hdlr = logging.FileHandler('/tmp/MyBot.log')
        formatter = logging.Formatter('%(relativeCreated)d %(levelname)s %(message)s')
        hdlr.setFormatter(formatter)
        logger.addHandler(hdlr)
        logger.propagate = False

    try:
        import psyco
        psyco.full()
    except ImportError:
        pass

    try:
        # if run is passed a class with a do_turn method, it will do the work
        # this is not needed, in which case you will need to write your own
        # parsing function and your own game state class
        Ants.run(MyBot(),debug=opts.debug)
    except KeyboardInterrupt:
        print('ctrl-c, leaving ...')
    except Exception as e:
        traceback.print_exc(file=sys.stderr)
