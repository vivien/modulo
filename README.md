# Modulo, a bot framework that smokes pipes!

_This is a work in progress._

## What is it?

*Modulo* is a framework used to build a language-agnostic bot, that means 
a source (IRC, Jabber, a socket, ...) and some plugins, in whatever language.

It consists of a *core*, dispatching I/Os between a *back end* and several 
*plugins*, for text-processing.

The back end and plugins are standard *filters*.

A filter is a standalone program, taking input from stdin, writing its output 
to stdout and eventually errors to stderr.

The core redirects the back end stdout to every plugins stdin.

Any plugins stdout is redirected to the back end stdin.

All stderr (back end and plugins) are redirected to the core's stderr.

## How does it work?

The core is purely input-driven. It uses realtime, queueable signals to wait 
for text from the back end or plugins. Thus, inputs have priorities, as follow 
(higher priority first):

  * Back end error
  * Plugin error
  * Plugin output
  * Back end output

That means that modulo tries to prioritize plugins responsivity.

When a message from the back end or a plugin is ready, the core receives 
a signal from the kernel, reads the message and forwards it where appropriate.

## How to use it?

The [**manpage**](http://vivien.github.io/modulo) is a good place to describe 
how to use Modulo.
