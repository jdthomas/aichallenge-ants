#!/usr/bin/env python

from math import log
from pprint import pprint
import scipy as sp
import matplotlib
#matplotlib.use('GTKAgg') # Change this as desired.
#import gobject
#from pylab import *
from matplotlib.pylab import *
import matplotlib.animation as animation

vmin=9999
vmax=0
data = {}

def load_data():
    global vmin,vmax,data
    f=open('/tmp/MyBot_c.log')
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
            a = map(float,d[3::])
        #if i not in ['bat','scr']: continue ## HACK
        if i not in ['vis','fod','hil','scr']: continue ## HACK
        if i != -1:
            if i not in tmp_data: tmp_data[i] = []
            #a = map(lambda x: log(x+0.1), a)
            tmp_data[i].append(a)
            vmin=min([vmin]+tmp_data[i][-1])
            vmax=max([vmax]+tmp_data[i][-1])
    print "VM/VM", vmin, vmax

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
    #plt.suptitle('%04d'%m)
    manager.canvas.draw()
    filename='/tmp/HeatMap/diffusion_%04d.png'%m
    fig.savefig(filename)
    render_frame(m,data,images,subplots)
    something = data.keys()[0]
    if m < len(data[something]):
        m+=1
        return True
    return False



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
    for render_num,x in enumerate(data):
        #s = fig.add_subplot(1,(1+len(data))/1,render_num+1, title=x)
        s = fig.add_subplot(r,c,render_num+1, title=x)
        subplots.append(s)
        i = s.imshow( data[x][1], cmap=cm.hot, interpolation='nearest', origin='upper')
        i.set_norm(norm)
        if render_num>0: images[0].callbacksSM.connect('changed', ImageFollower(i))
        images.append(i)
        fig.colorbar( i, orientation='horizantal' )

    manager = get_current_fig_manager()
    # once idle, call updatefig until it returns false.
    #gobject.idle_add(updatefig)
    ani = animation.FuncAnimation(fig, updatefig)
    show()
    print '(cd /tmp/HeatMap/ ;ffmpeg -qscale 5 -r 3 -b 9600 -i diffusion_%04d.png movie.mp4)'

m=1
fig = plt.figure(1)
subplots = []
images = []
manager = None

if __name__ == "__main__":
    load_data()
    do_rendering()