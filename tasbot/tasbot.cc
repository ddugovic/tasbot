/* Searches for solutions to Karate Kid. */

#include "tasbot.h"

/* Represents a node in the state graph.

   == Quotienting ==

   This can actually be a quotient of the "real" state graph, if we
   can identify parts of the state that don't matter (i.e., if some
   location in memory stores the cursor in the music, but that cursor
   is never branched on except to change the music, then we might
   ignore it for the purpose of state equality). This is risky
   business because the search algorithm assumes these states are
   interchangeable.

   For Karate Kid I am using the contents of RAM with certain ranges
   blocked out. For example, address 0x036E always contains the input
   byte for that frame. We need to ignore this in equality tests or
   else it will appear that every frame has 2^buttons distinct
   successors just because of the byte in memory that we control. In
   reality, many buttons are ignored on many frames (for example,
   jumping when in the air or punching while punching). In fact, this
   could automate the otherwise external knowledge that the Select
   button does nothing in this game.

   (P.S. I think that's the right thing to do from an efficiency
    standpoint, but these states might converge anyway because the
    states with equivalent inputs (in game logic) should also have
    equivalent (or equal) successors. But we may still increase
    efficiency by a constant factor of 2^inputs, which is obviously
    worth the minor complexity.)

   == Replay ==

   Compressed save states are about 1.8kb after doing some tricks
   (though I think this can be reduced further by ignoring some
   output-only parts; see emulator.cc). We could only store about
   35 million of them, max. Therefore, they are optional in
   most nodes. If the savestate is not present, we back up
   the graph to the most recent predecessor that has a savestate,
   then replay the inputs in order to load this node. This trades
   off time for space.


*/
struct Node : public Heapable {
  // If NULL, then this is the root state. It MUST have
  // saved state.
  Node *prev;
  // Optional new-ly allocated savestate, owned by this node.
  // It can be cleared at any time, unless prev is NULL.
  vector<uint8> *savestate;

  // The input issued to get from the previous state
  // to this state. Meaningless if this is the root node.
  uint8 input;

  // Best known distance to the root. If location (inherited
  // from Heapable) is -1, then this is the final distance.
  uint16 distance;

  // We have to be able to compute a priority for each node; this will
  // be some function of the distance and the heuristic. It usually
  // won't be possible to come up with an admissible heuristic because
  // there are so many unpredictable things about how the game proceeds.
  uint32 heuristic;
};

// Get the hashcode for the current state. This is based just on RAM.
// XXX should include registers too, right?
static uint64 GetHashCode() {
#if 0
  uint64 a = 0xDEADBEEFCAFEBABEULL,
    b = 0xDULL,
    c = 0xFFFF000073731919ULL;
  for (int i = 0; i < 0x800; i++) {
    do { uint64 t = a; a = b; b = c; c = t; } while(0);
    switch (i) {
    // Bytes that are ignored...
    case 0x036E: // Contains the input byte for that frame. Ignore.

      break;
    default:
      a += (RAM[i] * 31337);
      b ^= 0xB00BFACEFULL;
      c *= 0x1234567ULL;
      c = (c >> 17) | (c << (64 - 17));
      a ^= c;
    }
  }
  return a + b + c;
#endif

#if 0
  md5_context md;
  md5_starts(&md);
  md5_update(&md, RAM, 0x036E);
  md5_update(&md, RAM + 0x036F, 0x800 - 0x36F);
  uint8 digest[16];
  md5_finish(&md, digest);

  uint64 res = 0;
  for (int i = 0; i < 8; i++) {
    res <<= 8;
    res |= 255 & digest[i];
  }
  return res;
#endif

  vector<uint8> ss;
  Emulator::GetBasis(&ss);
  md5_context md;
  md5_starts(&md);
  md5_update(&md, &ss[0], ss.size());
  uint8 digest[16];
  md5_finish(&md, digest);

  uint64 res = 0;
  for (int i = 0; i < 8; i++) {
    res <<= 8;
    res |= 255 & digest[i];
  }
  return res;
}

// Magic "game location" address.
// See addresses.txt
static uint32 GetLoc() {
  return (RAM[0x0080] << 24) |
    (RAM[0x0081] << 16) |
    (RAM[0x0082] << 8) |
    (RAM[0x0083]);
}

// Next byte up is used after you hit a guy, sometimes?
inline static bool KarateBattle(uint32 loc) {
  return (loc & 0xFFFFFF) == 0x10000;
}

// Determine if this state should be avoided
// completely. For example, if we die or enter
// some optional bonus stage.
static bool IsBad() {

  // Number of lives
  // n.b. this would allow me to get an extra
  // life and then die. Maybe this should be
  // IsBadTransition and compare to the
  // last state?
  if (RAM[0x07D6] < 3) {
    return true;
  }

  uint32 loc = GetLoc();

  if (KarateBattle(loc)) {
    // Don't allow taking damage during
    // tournament.
    if (RAM[0x0085] < 0xF) {
      return true;
    }
  }

  return false;
}

// Return true if we've won.
// For now this is just killing the
// dude in the first battle.
static bool IsWon() {
  // Magic "game location" address.
  uint32 loc = GetLoc();

  if (KarateBattle(loc)) {
    return RAM[0x008B] == 0;
  }

  fprintf(stderr, "Exited karate battle? %u\n", loc);
  abort();
  return false;
}

// This must return an unsigned number 0-0xFFFFFFFF
// where larger numbers are closer to the end of
// the game.
static uint32 GetHeuristic() {
  // Magic "game location" address.
  uint32 loc = GetLoc();

  // Plan for later: First nybble is the phase
  // (battle 1-3, level 3-9, etc.), remainder
  // is some scaled value for that phase.

  if (KarateBattle(loc)) {
    // XXX need something to tell us which battle
    // number this is.

    // Start screen and battles.

    // More damage is better.
    uint8 damage = 0xFF - (uint8)RAM[0x008B];
    // XXX need to fix this when facing left.
    uint8 xcoord = (uint8)RAM[0x0502];
    // XXX y coordinate?
    // fprintf(stderr, "damage %u xcoord %u\n", damage, xcoord);
    return (damage << 16) | xcoord;
  } else {
    fprintf(stderr, "Exited karate battle? %u\n", loc);
    abort();
    // return 0xFFFFFFFF;
    // abort();
  }

  return 0;
}

// Initialized at startup.
static vector<uint8> *basis = NULL;

static Node *MakeNode(Node *prev, uint8 input) {
  Node *n = new Node;
  n->prev = prev;
  // PERF Shouldn't need to initialize this?
  n->location = -1;

  n->distance = (prev == NULL) ? 0 : (prev->distance + 1);
  // Note, ignored if prev == NULL.
  n->input = input;

  // XXX decide whether to make savestate, like by
  // tracing backwards or computing parity of distance
  // or whatever.
  n->savestate = new vector<uint8>;
  Emulator::SaveEx(n->savestate, basis);

  n->heuristic = GetHeuristic();
  return n;
}

static uint64 Priority(const Node *n) {
  return (uint64)0xFFFFFFFFULL - (uint64)n->heuristic;
  // return n->distance + (0xFFFFFFFF - n->heuristic);
  // return ((uint64)n->distance << 32) | (0xFFFFFFFF - n->heuristic);
}

typedef Heap<uint64, Node> GameHeap;
typedef unordered_map<uint64, Node *> GameHash;

// Load the emulator state for this node.
static void LoadNode(const Node *n) {
  if (n == NULL) {
    fprintf(stderr, "Invariant violation in LoadNode\n");
    abort();
  }

  if (n->savestate == NULL) {
    // Recurse and replay inputs on the way back.
    LoadNode(n->prev);
    Emulator::Step(n->input);
  } else {
    Emulator::LoadEx(n->savestate, basis);
  }
}

static void WriteMovie(const string &moviename, Config config,
                       const vector<uint8> &start_inputs, Node *winstate) {
  // Copy base inputs.
  vector<uint8> inputs = start_inputs;

  vector<uint8> rev;
  while (winstate->prev != NULL) {
    rev.push_back(winstate->input);
    winstate = winstate->prev;
  }

  // Now reverse them onto the output.
  for (int i = rev.size() - 1; i >= 0; i--)
    inputs.push_back(rev[i]);

  SimpleFM2::WriteInputs(moviename + ".fm2", "karate.nes",
                         config,
                         inputs);
  fprintf(stderr, "Wrote movie!\n");
}

/**
 * The main loop for the SDL.
 */
int main(int argc, char *argv[]) {
  fprintf(stderr, "Nodes are %ld bytes\n", sizeof(Node));

  Config config;
  config.game = "karate.nes";
  config.movie = "karate.fm2";
  config.fastforward = 0;
  Emulator::Initialize(config);

  vector<uint8> start_inputs = SimpleFM2::ReadInputs(config.movie);
  basis = new vector<uint8>;
  *basis = BasisUtil::LoadOrComputeBasis(start_inputs, 140, "karate.basis");

  // Fast-forward to gameplay.
  // There are 98 frames before the initial battle begins.
  // But the opening whistle goes until about 130.
  start_inputs.resize(130);
  for (int i = 0; i < start_inputs.size(); i++) {
    Emulator::Step(start_inputs[i]);
  }

  fprintf(stderr, "Starting...\n");

  GameHash nodes;

  Node *start = MakeNode(NULL, 0x0);
  nodes[GetHashCode()] = start;

  fprintf(stderr, "Insert..\n");

  GameHeap queue;
  fprintf(stderr, "Created heap\n");
  uint64 p = Priority(start);
  fprintf(stderr, "priority %llx\n", p);
  queue.Insert(p, start);

  uint64 bad_nodes = 0;

  uint32 deepest = 0;
  uint64 wrotelastdeepest = 0;

  uint32 heuristicest = 0;
  uint64 wrotelastheuristic = 0;

  uint64 processed = 0;
  uint64 rediscovered = 0, rediscovered_obsolete = 0,
    rediscovered_same_or_worse = 0;

  fprintf(stderr, "Start queue.\n");
  while (!queue.Empty()) {
    Node *explore = queue.PopMinimumValue();
    CHECK(explore->location == -1);

    processed++;
    if (processed % 1000 == 0) {
      // XXX report deepest?
      fprintf(stderr, "%llu bad %llu queue %d dist %d (re %llu ob %llu sow %llu)\n",
              processed, bad_nodes, queue.Size(), explore->distance,
              rediscovered, rediscovered_obsolete,
              rediscovered_same_or_worse);
    }


    if (processed % 50000 == 0) {
      char name[512];
      sprintf(name, "prog%llu-%d", processed, explore->distance);
      WriteMovie(name, config, start_inputs, explore);
    }

    if (explore->distance > deepest) {
      deepest = explore->distance;
      fprintf(stderr, "New deepest: %d heu %x\n", deepest, explore->heuristic);
      if (deepest > 12 && (processed - wrotelastdeepest) < 100) {
        WriteMovie("deepest", config, start_inputs, explore);
        wrotelastdeepest = processed;
      }
    }

    if (explore->heuristic > heuristicest) {
      heuristicest = explore->heuristic;
      fprintf(stderr, "New best heuristic: %d steps heu %x\n",
              explore->distance,
              explore->heuristic);
      if (heuristicest > 0xfa007d /* && (processed - wrotelastheuristic) < 100 */) {
        WriteMovie("heuristicest", config, start_inputs, explore);
        wrotelastheuristic = processed;
      }
    }

    // Not the power set of button combinations. Holding left and right or
    // up and down at the same time in this game is useless (XXX check?).
    // Select does nothing (right?). Pausing could conceivably help for
    // luck manipulation, but we're not trying that here.
    static const uint8 buttons[] = { 0, INPUT_A, INPUT_B, INPUT_A | INPUT_B };
    static const uint8 dirs[] = { 0, INPUT_R, INPUT_U, INPUT_L, INPUT_D,
                                  INPUT_R | INPUT_U, INPUT_L | INPUT_U,
                                  INPUT_R | INPUT_D, INPUT_L | INPUT_D };
    vector<uint8> next;
    for (int b = 0; b < sizeof(buttons); b++) {
      for (int d = 0; d < sizeof(dirs); d++) {
        next.push_back(buttons[b] | dirs[d]);
      }
    }

    Shuffle(&next);

    for (int n = 0; n < next.size(); n++) {
      uint8 input = next[n];
      // Only way to try a new input is to load the explore node
      // and make a step.
      // PERF: Should probably have LoadNode return the save state
      // so that we don't have to keep replaying.
      LoadNode(explore);
      Emulator::Step(input);

      // Did we win?
      if (IsWon()) {
        Node *now = MakeNode(explore, input);
        WriteMovie("winning", config, start_inputs, now);
        return 0;
      } else if (IsBad()) {
        bad_nodes++;
      } else {
        uint64 h = GetHashCode();
        GameHash::const_iterator it = nodes.find(h);

        if (it == nodes.end()) {
          Node *now = MakeNode(explore, input);
          nodes[h] = now;
          queue.Insert(Priority(now), now);

        } else {
          uint16 distance = 1 + explore->distance;
          // Already found.
          rediscovered++;
          Node *now = it->second;
          if (now->location == -1) {
            rediscovered_obsolete++;
          } else if (now->distance < distance) {
            rediscovered_same_or_worse++;
          } else {
            now->distance = distance;
            now->prev = explore;
            now->input = input;
            queue.AdjustPriority(now, Priority(now));
          }
        }
      }
    }

  }

  Emulator::Shutdown();

  // exit the infrastructure
  FCEUI_Kill();
  return 0;
}
