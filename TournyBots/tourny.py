
import random
num_bots = 100
weight_min=0
weight_max=100
#SERVER="209.62.17.40" #http://ash.webfactional.com/
#SERVER="ants.fluxid.pl"
SERVER="localhost"
PORT=2081
NETGAME=10
    

start_cmd = open("tourny_start.sh","w")

def print_bot(bot_id,w):
    MYBOT_Name="auntsinpants%d"%bot_id
    MYBOT_File="./Bots/RunMyBot_%d.sh"%bot_id
    my_bot = open(MYBOT_File,"w")
    my_bot.write('${HOME}/some_code/aichallenge.org/Bots/c/MyBot --debug --weights %f %f %f %f %f %f'% (w[0], w[1], w[2], w[3], w[4], w[5]))
    #close(my_bot)
    start_cmd.write('python ./tcpclient.py %s 2081 "sh %s" %s jdt %d &\n' %(SERVER,MYBOT_File,MYBOT_Name,NETGAME))

print_bot(0,[10.0, 20.0, 0.25, 0.0125, 100.0, 20.0])
print_bot(1,[1.0, 4.0, 0.000125, 0.0000125, 5.0, 5.0])

for bot_id in range(2,num_bots+2):
    w = [random.uniform(weight_min,weight_max) for x in range(6)]
    print_bot(bot_id,w)
#close(start_cmd)
