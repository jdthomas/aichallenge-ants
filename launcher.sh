#!/usr/bin/env bash
# :TODO: support games of other sizes
# Paramaters of this script
USE_RAND=1
TURNS=500
TIMEOUT=800
TCP_USER=auntsinpants
NETGAME=
# 
TOOLS_DIR=_official/ants/
MAP="${TOOLS_DIR}/maps/cell_maze/cell_maze_p04_01.map"

function choose_random_map {
    MAPS_LIST_FILE=/tmp/$$.maps.txt
    find _official/ants/maps/ -name "*.map" | grep "p04" > $MAPS_LIST_FILE
    LINES=$(cat $MAPS_LIST_FILE | wc -l)
    LINE=$(( RANDOM % LINES + 1 ))
    head -$LINE $MAPS_LIST_FILE | tail -1
    rm $MAPS_LIST_FILE
}

M=
if [ "x$USE_RAND" != "x" ] ; then
    M=$(choose_random_map)
fi
if [ "x$M" != "x" ]; then
    MAP=$M
    echo "MAP UPDATED TO: $MAP"
fi

MB_ARGS="--debug"
MYBOT_C="$PWD/Bots/c/RunMyBot.sh"
#MYBOT_P="python -m cProfile -o /tmp/MyBotProf Bots/python/MyBot.py $MB_ARGS"
MYBOT_P="python Bots/python/MyBot.py $MB_ARGS"

MYBOT=$MYBOT_C

function get_rel_bots {
    if [ ! -d ReleasedBots ]; then
        ./MakeReleaseBots.sh ReleasedBots
    fi
    for B in $(ls ReleasedBots/Run* | sort -r -n | head -n $1); do
        echo "$PWD/$B"
    done
}

if [ "x$NETGAME" == "x" ] ; then
    if [ "x$USE_RAND" == "x" ] ; then
        PLAYER_SEED="--player_seed=7"
        ENGINE_SEED="--engine_seed=17"
    fi
    echo "Starting game ..."
    python ${TOOLS_DIR}/playgame.py \
        "$MYBOT_C" \
        $(get_rel_bots 3) \
        --map_file ${MAP} \
        --log_dir game_logs \
        --turns $TURNS \
        ${PLAYER_SEED} ${ENGINE_SEED} \
        --verbose -e -E \
        --end_wait=1.25 \
        --nolaunch --html="/tmp/replay.$$.html" \
        --turntime=$TIMEOUT ;
    echo "MAP=$MAP TIMEOUT=$TIMEOUT TURNS=$TURNS"
else
    #SERVER=209.62.17.40 #http://ash.webfactional.com/
    #SERVER=tcpants.com
    SERVER=ants.fluxid.pl
    PORT=2081
    TCP_PW_FILE=./.tcp_pw
    TCP_PASS=$(cat ${TCP_PW_FILE})
    if [ ! -e $TCP_PW_FILE -o "x$TCP_PASS" == "x" ]; then
        echo "Place your password for user ${TCP_USER} in ${TCP_PW_FILE}"
        exit 1
    fi
    python ants-tcp/clients/tcpclient.py $SERVER 2081 "${MYBOT}" ${TCP_USER} ${TCP_PASS} ${NETGAME}
fi
