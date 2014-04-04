# Using modulo as an high-level IRC bot

This set of scripts uses the `irc` backend, which abstracts IRC 
protocol-specific messages, and prints user messages (PRIVMSG) on its standard 
output.

From this directory, run:

    modulo plugins/ backend -n modbot -c '#modulo'
