
import random
import logging
import sys
import numpy as NP
import pylab

logLevel = logging.DEBUG
logging.basicConfig()
logger = logging.getLogger("DebugDisplay")
logger.setLevel(logLevel)

ld = logger.debug
li = logger.info
lw = logger.warning
from threading import Thread
import Queue

class _Antimation(Thread):
    def __init__ (self):
        Thread.__init__(self)
        #pylab.ion() # animation on
        #pylab.axis([0, 1, 0, 1])
        self.fnum=0
        self.q = Queue.Queue()
    def run(self):
        while(True):
            DebugInfo = self.q.get()
            ld("GOT DI: %s", DebugInfo)
            self.drawHeatMap(DebugInfo)
            #self.drawHeatMapAll(DebugInfo)
            self.fnum+=1

    def drawHeatMapAll(self,di):
        A=di['all_objective']
        pylab.subplot(111)
        #cb = pylab.colorbar()
        #cb.set_label('mean value')

        pylab.imshow( A, cmap=pylab.cm.jet,  interpolation='nearest', vmin=0, vmax=self.MAXPATH*2 )
        pylab.savefig("/tmp/HeatMapAll/pylab_%s.png"%self.fnum)
        #pylab.draw()
    def drawHeatMap(self,di):
        round_scores = di['round_scores']
        rows=di['rows']
        cols=di['cols']
        x = [ a[0] for a in round_scores ]
        y = [ a[1] for a in round_scores ]
        z = [ a[2] for a in round_scores ]

        A = NP.zeros((rows,cols))
        for s in round_scores:
            A[s[0]][s[1]] = float(s[2]- min(z))/max(z) 

        ld("x: %s",x)
        ld("y: %s",y)
        ld("z: %s",z)

        pylab.subplot(111)
        #cb = pylab.colorbar()
        #cb.set_label('mean value')
        #gridsize=[self.rows,self.cols]
        #pylab.hexbin(x, y, C=z, gridsize=gridsize, cmap=CM.jet, bins=None)
        #pylab.imshow( A, cmap=CM.jet,  interpolation='nearest', vmin=min(z), vmax=max(z) )
        pylab.imshow( A, cmap=pylab.cm.jet,  interpolation='nearest')
        pylab.savefig("/tmp/HeatMap/pylab_%s.png"%self.fnum)
        #pylab.draw()

class Antimation():
    def __init__(self):
        self._self = _Antimation()
        self._self.start()
    def put(self,di):
        self._self.q.put(di)

