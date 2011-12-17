#!/usr/bin/env python

import sys
from math import log
from pprint import pprint
import matplotlib
got_animation = False
try:
    import matplotlib.animation as animation
    got_animation = True
except ImportError:
    pass
if not got_animation:
    matplotlib.use('GTKAgg') # Change this as desired.
    import gobject
from matplotlib.pylab import *
from pylab import *
import scipy as sp

print "Got matplotlib's animation?", got_animation

show_colorbar = False
vmin=9999
vmax=0
data = {}

def load_data(log_file):
    global vmin,vmax,data
    f=open(log_file)
    turn = 0
    tmp_data = {}
    for line in f.readlines():
        if line.startswith('-------'):
            turn+=1
            print "got turn: ", turn
            for x in tmp_data:
                if x not in data: data[x] = []
                data[x].append(tmp_data[x])
            tmp_data = {}
        i=-1
        a=[]
        if line.startswith('plt '):
            d = line.split()
            i = d[1]
            #"plt name xxx: ..."
            try:
                a = map(float,d[3::])
            except ValueError:
                print d
                sys.exit(1)
        #if i not in ['score', 'defense', 'food', 'hill', 'unseen', 'battle', 'vis', 'bfs']: continue
        #if i not in ['bfs', 'score']: continue
        if i != -1:
            if i not in tmp_data: tmp_data[i] = []
            #a = map(lambda x: log(x+0.1), a)
            tmp_data[i].append(a)
            vmin=min([vmin]+tmp_data[i][-1])
            vmax=max([vmax]+tmp_data[i][-1])
    print "VM/VM", vmin, vmax
    print "read %d turns" % turn

class ImageFollower:
    'update image in response to changes in clim or cmap on another image'
    def __init__(self, follower):
        self.follower = follower
    def __call__(self, leader):
        self.follower.set_cmap(leader.get_cmap())
        self.follower.set_clim(leader.get_clim())

def render_frame(m, data, images, subplots):
    for render_num,x in enumerate(data):
        images[render_num].set_array(data[x][m])
        subplots[render_num].set_title('%s - %d'%(x,m))


def updatefig(*args):
    global m, data, images, subplots, fig, manager
    print "Computing and rendering u for m =", m
    something = data.keys()[0]
    if m >= len(data[something]): return False
    #plt.suptitle('%04d'%m)
    manager.canvas.draw()
    filename='/tmp/HeatMap/diffusion_%04d.png'%m
    fig.savefig(filename)
    render_frame(m,data,images,subplots)
    m+=1
    return True



def do_rendering():
    global vmin,vmax,data, m, images, subplots, fig, manager
    norm = matplotlib.colors.Normalize(vmin=-1, vmax=1)
    #norm = matplotlib.colors.Normalize(vmin=0, vmax=1)
    #norm = matplotlib.colors.Normalize(vmin=vmin, vmax=vmax)
    #norm = matplotlib.colors.LogNorm(vmin=vmin+0.0000001, vmax=vmax+0.0000001)
    def num_rows(total):
        if total < 4: return 1
        return 2
    def num_cols(total,rows):
        return total/rows
    r = num_rows(len(data))
    c = num_cols(len(data),r)
    something = data.keys()[0]
    rr=len(data[something])
    rc=len(data[something][0])
    if rr < rc: t=r ; r=c ; c=t

    for render_num,x in enumerate(data):
        #s = fig.add_subplot(1,(1+len(data))/1,render_num+1, title=x)
        s = fig.add_subplot(r,c,render_num+1, title=x)
        subplots.append(s)
        i = s.imshow( data[x][1], cmap=cm.hot, interpolation='nearest', origin='upper')
        i.set_norm(norm)
        if render_num>0: images[0].callbacksSM.connect('changed', ImageFollower(i))
        images.append(i)
        if(show_colorbar): fig.colorbar( i, orientation='horizantal' )

    manager = get_current_fig_manager()
    # once idle, call updatefig until it returns false.
    #gobject.idle_add(updatefig)
    if got_animation:
        ani = animation.FuncAnimation(fig, updatefig)
    else:
        # once idle, call updatefig until it returns false.
        gobject.idle_add(updatefig)
    show()
    print '(cd /tmp/HeatMap/ ;ffmpeg -qscale 5 -r 3 -b 9600 -i diffusion_%04d.png movie.mp4)'

m=1
fig = plt.figure(1)
subplots = []
images = []
manager = None

if __name__ == "__main__":
    #log_id = sys.argv[1]
    #load_data('/tmp/MyBot_c.%d.log'%log_id)
    load_data(sys.argv[1])
    do_rendering()
