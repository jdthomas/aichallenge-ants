ps ax | egrep tcpclient | cut -d' ' -f2|xargs kill
