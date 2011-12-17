#!/usr/bin/env python

import random
import subprocess

num_bots = 100
weight_min=0.0
weight_max=1000.0
alpha_min=0.0
alpha_max=1.0
#SERVER="209.62.17.40" #http://ash.webfactional.com/
#SERVER="ants.fluxid.pl"
SERVER="localhost"
PORT=2081
NETGAME=2
    
start_cmd=None

def print_bot(bot_id,w,a):
    global start_cmd
    MYBOT_Name="auntsinpants%d"%bot_id
    MYBOT_File="./Bots/RunMyBot_%d.sh"%bot_id
    my_bot = open(MYBOT_File,"w")
    weight_args = '--weights %f %f %f %f %f %f %f'%(w[0], w[1], w[2], w[3], w[4], w[5], w[6])
    alpha_args = '--alpha %f %f %f %f %f %f'%(a[0], a[1], a[2], a[3], a[4], a[5])
    my_bot.write('${HOME}/some_code/aichallenge.org/Bots/c/MyBot --debug %s %s'%(weight_args,alpha_args))
    my_bot.close()
    start_cmd.write('python ./tcpclient.py %s 2081 "sh %s" %s jdt %d &\n' %(SERVER,MYBOT_File,MYBOT_Name,NETGAME))

def generate_botscripts():
    global start_cmd
    start_cmd = open("tourny_start.sh","w")
    print_bot(0,[10.0, 20.0, 0.25, 0.0125, 100.0, 20.0, 0.0000001], [0.2]*6)
    print_bot(1,[1.0, 4.0, 0.000125, 0.0000125, 5.0, 5.0, 0.0000001], [0.2]*6)
    for bot_id in range(2,num_bots+2):
        w = [random.uniform(weight_min,weight_max) for x in range(7)]
        a = [random.uniform(alpha_min,alpha_max) for x in range(6)]
        print_bot(bot_id,w,a)
    start_cmd.close()

def run_bot(bot_id):
    MYBOT_Name="auntsinpants%d"%bot_id
    MYBOT_File="./Bots/RunMyBot_%d.sh"%bot_id
    bot_command = 'python ./tcpclient.py %s 2081 "sh %s" %s jdt %d > /dev/null\n' %(SERVER,MYBOT_File,MYBOT_Name,NETGAME)
    bot = subprocess.Popen(
        bot_command,
        close_fds=True,
        shell=True,
    )
    return bot

generate_botscripts()

all_bot_ids = range(num_bots+2)
count = 0
while(True):
    count+=1
    random.shuffle(all_bot_ids)
    some_bot_ids = all_bot_ids[0:10]
    running_bots = [run_bot(x) for x in some_bot_ids]
    for bot in running_bots:
        bot.wait()
    #if count > 1: break;

