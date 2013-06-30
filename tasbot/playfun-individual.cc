  // 10,000 is about enough to run out the clock, fyi
  static const int NUMFRAMES = 22000;
  for (int framenum = 0; framenum < NUMFRAMES; framenum++) {

    // Save our current state so we can try many different branches.
    Emulator::Save(&current_state);    
    GetMemory(&current_memory);

    static const uint8 buttons[] = { 0, INPUT_A, INPUT_B, INPUT_A | INPUT_B };
    static const uint8 dirs[] = { 0, INPUT_R, INPUT_U, INPUT_L, INPUT_D,
				  INPUT_R | INPUT_U, INPUT_L | INPUT_U,
				  INPUT_R | INPUT_D, INPUT_L | INPUT_D };

    vector<uint8> inputs;
    for (int b = 0; b < sizeof(buttons); b++) {
      for (int d = 0; d < sizeof(dirs); d++) {
	uint8 input = buttons[b] | dirs[d];
	inputs.push_back(input);
      }
    }
    
    // To break ties.
    Shuffle(&inputs);

    double best_score = -1;
    uint8 best_input = 0;
    for (int i = 0; i < inputs.size(); i++) {
      // (Don't restore for first one; it's already there)
      if (i != 0) Emulator::Load(&current_state);
      Emulator::Step(inputs[i]);

      vector<uint8> new_memory;
      GetMemory(&new_memory);
      double score = objectives->GetNumLess(current_memory, new_memory);
      if (score > best_score) {
	best_score = score;
	best_input = inputs[i];
      }
    }

    printf("Best was %f: %s\n", 
	   best_score,
	   SimpleFM2::InputToString(best_input).c_str());

    // PERF maybe could save best state?
    Emulator::Load(&current_state);
    Emulator::Step(best_input);
    movie.push_back(best_input);
  }
