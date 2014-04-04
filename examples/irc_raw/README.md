# Using modulo as an low-level IRC bot

Use `netcat` as a backend to open a socket to an IRC server and execute scripts 
corresponding to the received IRC command. (e.g. PRIVMSG, QUIT, etc.)

From this directory, run:

    modulo . /usr/bin/nc irc.freenode.net 6667
