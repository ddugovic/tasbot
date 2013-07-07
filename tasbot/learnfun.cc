/* This program attempts to learn an objective function for a
   particular game by watching movies of people playing it. The
   objective function can then be used by playfun to play the game.
 */

#include "learnfun.h"

static void SaveMemory(vector< vector<uint8> > *memories) {
  memories->resize(memories->size() + 1);
  vector<uint8> *v = &memories->back();
  for (int i = 0; i < 0x800; i++) {
    v->push_back((uint8)RAM[i]);
  }
}

static vector< vector<int> > *objectives = NULL;
static void PrintAndSave(const vector<int> &ordering) {
  printf("%zu=[ ", objectives->size());
  for (int i = 0; i < ordering.size(); i++) {
    printf("%d ", ordering[i]);
  }
  printf("]\n");
  CHECK(objectives);
  objectives->push_back(ordering);
}

// With e.g. an divisor of 3, generate slices covering
// the first third, middle third, and last third.
static void GenerateNthSlices(int divisor, int num, 
			      const vector< vector<uint8> > &memories,
			      Objective *obj) {
  const size_t onenth = memories.size() / divisor;
  for (int slicenum = 0; slicenum < divisor; slicenum++) {
    vector<int> look;
    size_t low = slicenum * onenth;
    for (int i = 0; i < onenth; i++) {
      look.push_back(low + i);
    }
    printf("For slice %zu-%zu:\n", low, low + onenth - 1);
    for (int i = 0; i < num; i++) {
      obj->EnumerateFull(look, PrintAndSave, 1, slicenum * 0xBEAD + i);
    }
  }
}

static void GenerateOccasional(int stride, int offsets, int num,
			       const vector< vector<uint8> > &memories,
			       Objective *obj) {
  int span = stride / offsets;
  int start = rand() % span;
  for (int off = 0; off < offsets; off++) {
    vector<int> look;
    // Consider starting at various places throughout the first stride?
    for (int frame = start; frame < memories.size(); frame += stride) {
      look.push_back(frame);
    }
    printf("For occasional @%d (every %d):\n", off, stride);
    for (int i = 0; i < num; i++) {
      obj->EnumerateFull(look, PrintAndSave, 1, off * 0xF00D + i);
    }
    start += span;
  }
}

static void MakeObjectives(const string &game, const vector< vector<uint8> > &memories) {
  printf("Now generating objectives.\n");
  objectives = new vector< vector<int> >;
  Objective obj(memories);

  // Going to generate a bunch of objective functions.
  // Some things will never violate the objective, like
  // [world number, stage number] or [score]. So generate
  // a handful of whole-game objectives.

  // TODO: In Mario, all 50 appear to be effectively the same
  // when graphed. Are they all equivalent, and should we be
  // accounting for that e.g. in weighting or deduplication?
  for (int i = 0; i < 50; i++)
    obj.EnumerateFullAll(PrintAndSave, 1, i);

  // XXX Not sure how I feel about these, based on the
  // graphics. They are VERY noisy.

  // Next, generate objectives for each slice of the game:
  // each half, third, fourth, etc.
  for (int divisor=2; divisor<=10; divisor++)
    GenerateNthSlices(divisor, 3, memories, &obj);

  // And for each 1/50th.
  GenerateNthSlices(50, 2, memories, &obj);

  // And for each 1/100th.
  GenerateNthSlices(100, 1, memories, &obj);

  // Now, for individual frames spread throughout the whole movie.
  // This one looks great.
  GenerateOccasional(100, 10, 20, memories, &obj);

  GenerateOccasional(250, 10, 10, memories, &obj);

  // This one looks okay; noisy at times.
  GenerateOccasional(1000, 10, 5, memories, &obj);

  // Weight them. Currently this is just removing duplicates.
  printf("There are %zu objectives\n", objectives->size());
  WeightedObjectives weighted(*objectives);
  printf("And %zu example memories\n", memories.size());
  weighted.WeightByExamples(memories);
  printf("And %zu unique objectives\n", weighted.Size());

  weighted.SaveToFile((game+ ".objectives").c_str());

  weighted.SaveSVG(memories, (game+ ".svg").c_str());
}

int main(int argc, char *argv[]) {
  Config config(argc, argv);
  Emulator::Initialize(config);
  vector<uint8> movie = SimpleFM2::ReadInputs(config.movie.c_str());
  CHECK(!movie.empty());

  vector< vector<uint8> > memories;
  memories.reserve(movie.size() + 1);
  vector<uint8> inputs;

  // The very beginning of most games start with RAM initialization,
  // which we really should ignore for building an objective function.
  // So skip until there's a button press in the movie.
  size_t start = 0;
  printf("Skipping frames without argument.\n");
  while (start < movie.size() && movie[start] == 0) {
    Emulator::Step(movie[start]);
    start++;
  }
  while (start < config.fastforward && start < movie.size()) {
    Emulator::Step(movie[start]);
    start++;
  }
  CHECK(start < movie.size());

  printf("Skipped %ld frames until first keypress/ffwd.\n"
	 "Playing %ld frames...\n", start, movie.size() - start);

  SaveMemory(&memories);

  {
    vector<uint8> save;
    Emulator::Save(&save);
    printf("Save states are %ld bytes.\n", save.size());
  }

  uint64 time_start = time(NULL);
  for (int i = start; i < movie.size(); i++) {
    if (i % 1000 == 0) {
      printf("  [% 5.1f%%] %6d/%ld\n", 
	     ((100.0 * i) / movie.size()), i, movie.size());
      // exit(0);
    }
    Emulator::Step(movie[i]);
    inputs.push_back(movie[i]);
    SaveMemory(&memories);
  }
  uint64 time_end = time(NULL);

  printf("Recorded %zu memories in %llu sec.\n", 
         memories.size(),
         time_end - time_start);

  MakeObjectives(config.game, memories);
  Motifs motifs;
  motifs.AddInputs(inputs, config.fastforward);
  motifs.SaveToFile((config.game+ ".motifs").c_str());

  Emulator::Shutdown();

  // exit the infrastructure
  FCEUI_Kill();
  return 0;
}
