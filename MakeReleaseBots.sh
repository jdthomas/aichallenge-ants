#!/bin/bash

REL_BOT_DIR=released_bots
if [ "x$1" != "x" ] ; then
    REL_BOT_DIR=$1
fi
REL_BOT_DIR_F=${PWD}/${REL_BOT_DIR}

TAG_LIST_FILE=/tmp/tag_list.$$.txt
git tag | grep -- '-py$\|-c$' | sort -n -r > $TAG_LIST_FILE
MASTER_GIT=$PWD
function gen_c_start_script {
    echo "#!/bin/sh"
    echo "$PWD/c/MyBot"
}
function gen_py_start_script {
    echo "#!/bin/sh"
    echo "python $PWD/python/MyBot.py"
}
function gen_start_scripts() {
    T=$1
    TS=$(echo $T | sed -e 's/release//' -e 's/[_-]//g')
    SCR=${REL_BOT_DIR_F}/Run$TS
    if [ "x$(echo $T | grep -- '-c$')" != "x" ]; then
        (cd c/ && make) ;
        gen_c_start_script > "$SCR" ;
    elif [ "x$(echo $T | grep -- '-py$')" != "x" ]; then
        gen_py_start_script > "$SCR" ;
    else
        echo "UNRECOGNIZED BOT TYPE"
    fi
    chmod +x "$SCR"
}
CLONE_DIR=$REL_BOT_DIR/clone
rm -rf $CLONE_DIR
mkdir -p $CLONE_DIR
(cd $CLONE_DIR ;
git clone $MASTER_GIT . >/dev/null 2>&1 ; )

for T in $(cat $TAG_LIST_FILE); do
    BOT_DIR=${REL_BOT_DIR}/$T
    mkdir -p ${BOT_DIR}
    (cd ${CLONE_DIR} ;
    git checkout $T >/dev/null  2>&1 ; )
    cp -rf ${CLONE_DIR}/Bots/* ${BOT_DIR}
    (cd ${BOT_DIR} ;
    gen_start_scripts $T ; )
done

rm $TAG_LIST_FILE

