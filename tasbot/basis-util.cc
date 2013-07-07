
#include "basis-util.h"

vector<uint8> BasisUtil::LoadOrComputeBasis(const vector<uint8> &inputs,
					      int frame,
					      const string &basisfile) {
  if (Util::ExistsFile(basisfile)) {
    fprintf(stderr, "Loading basis file %s.\n", basisfile.c_str());
    return Util::ReadFileBytes(basisfile);
  }

  fprintf(stderr, "Computing basis file %s.\n", basisfile.c_str());
  vector<uint8> start;
  Emulator::Save(&start);
  for (int i = 0; i < frame && i < inputs.size(); i++) {
    Emulator::Step(inputs[i]);
  }
  vector<uint8> basis;
  Emulator::GetBasis(&basis);
  if (!Util::WriteFileBytes(basisfile, basis)) {
    fprintf(stderr, "Couldn't write to %s\n", basisfile.c_str());
    abort();
  }
  fprintf(stderr, "Written.\n");
  
  // Rewind.
  Emulator::Load(&start);

  return basis;
}
