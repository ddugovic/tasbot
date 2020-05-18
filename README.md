Experimental code for automating the play of NES games.
This directory contains some false starts and the playfun and learnfun
algorithms, which original author Tom Murphy VII describes here:
http://tom7.org/mario/

The intended platform is 64-bit GNU/Linux, although mingw-specific
code is left intact so compiling on Windows should only require
obtaining dependencies and editing the makefile.

Compiling with Google protobuf (optional), SDL (optional), and
SDL_net (optional) is known to work with the following packages:
protobuf-devel 2.3.0-7 or better
SDL-devel 1.2.10-9 or better
SDL_net-devel 1.2.7-1 or better

Compiling with zlib is known to work with the following package:
zlib-devel 1.2.3-29 or better

The fceu subdirectory is the fork of FCEUX. Tom deleted a bunch of
stuff from it, and made it compile cleanly under 64-bit mingw for
x64. It is licensed under the GPL (see fceu/COPYING), including his
modifications.

Despite depending on SDL (for networking) this is currently a
headless compile; no graphics or sound or input. It is possible to
edit the makefile to compile without SDL and networking. See Tom's
original project (at the link above) for Windows compile details.


For most of these programs you need to make modifications to the
script (e.g. playfun.sh) to set some constants, like what game and
what movie you want to learn:

tasbot   - A*-ish search for solutions to games. Needs a hand-written
           objective function. Very slow.

learnfun - learns an objective function of RAM values, as well as
           capturing input motifs that can be played by playfun.

playfun  - plays the game given the output of learnfun. More or less
           works for "easy" games like Super Mario Bros.


Learnfun and Playfun work well enough to play "easy" games without
any customization. The steps are:

- Use FCEUX to record a movie (FM2 format) of you playing the game.
  I usually record a few thousand frames and try to keep it simple.
  See this video for an example: http://youtu.be/OS75JLwJExk

- Modify playfun.sh to set the name of your movie. You can also set
  fastforward to skip any number of frames (copying them from your
  movie), like if you want to skip menus. This is obviously cheating
  since learnfun and playfun never learn the menus.

- Run learnfun to produce an .objectives and .motifs file
  based on your inputs. It also makes some optional SVG files.

- Run playfun to produce replayable .fm2 movie files.
  I recommend running playfun in MARIONET (client/server) mode which
  parallelizes computation to run faster. This means first starting
  a helper for each logical CPU, then starting a single master:

      ARGS="--game mario --movie mario.fm2 --fastforward 200"
      ./learnfun $ARGS
      ./playfun --helper 8000 $ARGS &
      ./playfun --helper 8001 $ARGS &
      ./playfun --helper 8002 $ARGS &
      ./playfun --helper 8003 $ARGS &
      sleep 1
      ./playfun --master 8000 8001 8002 8003 $ARGS

  These all output ANSI colors and escape sequences to draw progress
  bars, so you may want to run them in different console windows.
  Note that MARIONET is vulnerable to remote exploitation (it loads
  savestates, assuming they're valid). Don't run it on open networks.

- Playfun will run forever. Every 50 frames it writes an .fm2 file
  (*-playfun-*.fm2) which you can replay in FCEUX to watch it play!
  Note that playfun is slow; on a 2-core AMD Turion, it takes about
  5 minutes to generate 1 second (60 frames) of gameplay.

Read TODO for ideas on how to improve this program. To date I've
mainly focused on refactors to accomodate future changes and to
improve performance.
