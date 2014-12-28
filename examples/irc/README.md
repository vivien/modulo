# Using modulo as an high-level IRC bot

This set of scripts contains a back end which abstracts IRC 
protocol-specific messages, and prints user messages (PRIVMSG) on its 
standard output. The standard output of plugins are forwarded as is as 
a user message. 

See the Dockerfile to check dependencies and how to run this IRC bot.

This container requires the "modulo" container. It can be built and run 
with:

    # docker build -t modbot .
    # docker run -i -t modbot
