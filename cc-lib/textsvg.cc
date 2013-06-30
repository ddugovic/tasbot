
#include "textsvg.h"

#include <string>
#include <stdio.h>

using namespace std;

string TextSVG::Header(double width, double height) {
  char out[512];
  sprintf(out,
	  "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
	  "<!-- Generator: weighted-objectives.cc -->\n"
	  "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" "
	  "\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\" [\n"
	  "<!ENTITY ns_flows \"http://ns.adobe.com/Flows/1.0/\">\n"
	  "]>\n"
	  "<svg version=\"1.1\"\n"
	  " xmlns=\"http://www.w3.org/2000/svg\""
	  " xmlns:xlink=\"http://www.w3.org/1999/xlink\""
	  " xmlns:a=\"http://ns.adobe.com/AdobeSVGViewerExtensions/3.0/\""
	  " x=\"0px\" y=\"0px\" width=\"%fpx\" height=\"%fpx\""
	  " xml:space=\"preserve\">\n",
	  width, height);
  return (string)out;
}

string TextSVG::Footer() {
  return "</svg>\n";
}
