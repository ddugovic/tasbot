#include "util.h"

string SVGTickmarks(double width, double maxx, double span,
		    double tickheight, double tickfont) {
  printf("SVGTickmarks %f %f %f\n", width, maxx, span);
  string out;
  bool longone = true;
  for (double x = 0.0; x < maxx; x += span) {
    double xf = x / maxx;
    out += 
      StringPrintf("  <polyline fill=\"none\" opacity=\"0.5\""
		   " stroke=\"#000000\""
		   " stroke-width=\"1\" points=\"%.2f,0.00 %.2f,%.2f\" />\n",
		   width * xf, width * xf, 
		   longone ? tickheight * 2 : tickheight);
    if (longone)
      out += StringPrintf("<text x=\"%.2f\" y=\"%.2f\" font-size=\"%.2f\">"
			  "<tspan fill=\"#000000\">%d</tspan>"
			  "</text>\n",
			  width * xf + 3.0, 2.0 * tickheight + 2.0,
			  tickfont, (int)x);
    longone = !longone;
  }
  return out;
}

string DrawDots(const double WIDTH, const double HEIGHT,
		const string &color, double xf,
		const vector<double> &values, double minval, double maxval, 
		int chosen_idx) {
  vector<double> sorted = values;
  std::sort(sorted.begin(), sorted.end());
  string out;
  int size = values.size();
  for (int i = 0; i < values.size(); i++) {
    int sorted_idx = 
      lower_bound(sorted.begin(), sorted.end(), values[i]) - sorted.begin();
    double opacity;
    if (sorted_idx < 0.1 * size || sorted_idx > 0.9 * size) {
      opacity = 0.2;
    } else if (sorted_idx < 0.2 * size || sorted_idx > 0.8 * size) {
      opacity = 0.4;
    } else if (sorted_idx < 0.3 * size || sorted_idx > 0.7 * size) {
      opacity = 0.6;
    } else if (sorted_idx < 0.4 * size || sorted_idx > 0.6 * size) {
      opacity = 0.8;
    } else {
      opacity = 1.0;
    }
    double yf = (values[i] - minval) / (maxval - minval);
    out += StringPrintf("<circle cx=\"%.1f\" cy=\"%.1f\" r=\"%d\" "
			"opacity=\"%.1f\" "
			"fill=\"%s\" />",
			WIDTH * xf,
			HEIGHT * yf,
			(i == chosen_idx) ? 10 : 4,
			opacity,
			color.c_str());
  }
  return out += "\n";
}

InPlaceTerminal::InPlaceTerminal(int lines) 
  : lines(lines), last_was_output(false) {
  CHECK(lines > 0);
}

void InPlaceTerminal::Output(const string &s) {
  if (last_was_output) {
    for (int i = 0; i < lines; i++) {
      fprintf(stderr,
	      // Cursor to beginning of previous line
	      "\x1B[F"
	      // Clear line
	      "\x1B[2K"
	      );
    }
  }

  // Maybe cleaner to pad this to the length of the line
  // with spaces than to clear above.
  fprintf(stderr, "%s", s.c_str());

  last_was_output = true;
}

void InPlaceTerminal::Advance() {
  last_was_output = false;
}
