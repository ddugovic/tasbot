
#include <sys/stat.h>
#include <cstring>
#include <string.h>

#include "util.h"

#if defined(WIN32) || defined(__MINGW32__)
   /* chdir */
#  include <direct.h>
   /* getpid */
#  include <process.h>
   /* time */
#  include <time.h>
   /* rename */
#  include <io.h>
   /* setclipboard */
#  include <windows.h>

// Visual studio only.
#ifdef WIN32
# pragma warning(disable: 4996)
#endif

#else /* posix */
   /* chdir, unlink */
#  include <unistd.h>
   /* getpid */
#  include <sys/types.h>
   /* isalnum */
#  include <ctype.h>
#endif


string itos(int i) {
  char s[64];
  sprintf(s, "%d", i);
  return (string)s;
}

string dtos(double d) {
  char s[64];
  sprintf(s, "%.2f", d);
  return (string)s;
}

struct linereal : public line {
  int x0, y0, x1, y1;
  int dx, dy;
  int stepx, stepy;
  int frac;

  virtual ~linereal() {}
  
  linereal(int x0_, int y0_, int x1_, int y1_) : 
    x0(x0_), y0(y0_), x1(x1_), y1(y1_) {
    

    dy = y1 - y0;
    dx = x1 - x0;

    if (dy < 0) { 
      dy = -dy;
      stepy = -1; 
    } else {
      stepy = 1; 
    }

    if (dx < 0) {
      dx = -dx;
      stepx = -1;
    } else { 
      stepx = 1; 
    }

    dy <<= 1;
    dx <<= 1;

    if (dx > dy) {
      frac = dy - (dx >> 1);
    } else {
      frac = dx - (dy >> 1);
    }
  }

  bool next(int & cx, int & cy) {
    if (dx > dy) {
      if (x0 == x1) return false;
      else {
	if (frac >= 0) {
	  y0 += stepy;
	  frac -= dx;
	}
	x0 += stepx;
	frac += dy;
	cx = x0;
	cy = y0;
	return true;
      }
    } else {
      if (y0 == y1) return false;
      else {
	if (frac >= 0) {
	  x0 += stepx;
	  frac -= dy;
	}
	y0 += stepy;
	frac += dx;
	cx = x0;
	cy = y0;
	return true;
      }
    }
  }
  
  virtual void destroy() {
    delete this;
  }

};

line * line::create(int a, int b, int c, int d) {
  return new linereal(a, b, c, d);
}

bool Util::isdir(string f) {
  struct stat st;
  return (!stat(f.c_str(), &st)) && (st.st_mode & S_IFDIR);
}

bool Util::ExistsFile(string s) {
  struct stat st;

  return !stat(s.c_str(), &st);
}

bool Util::existsdir(string d) {
  return isdir(d); /* (ExistsFile(d) && isdir(d.c_str())); */
}

/* XXX what mode? */
bool Util::makedir(string d) {
# if defined(WIN32) || defined(__MINGW32__)
  return !mkdir(d.c_str());
# else /* posix */
  return !mkdir(d.c_str(), 0755);
# endif
}

string Util::ptos(void * p) {
  char s[64];
  sprintf(s, "%p", p);
  return (string)s;
}

string Util::ReadFile(const string &s) {
  if (Util::isdir(s)) return "";
  if (s == "") return "";

  FILE * f = fopen(s.c_str(), "rb");

  if (!f) return "";
  fseek(f, 0, SEEK_END);
  int size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char * ss = (char*)malloc(size);
  fread(ss, 1, size, f);

  fclose(f);

  string ret = string(ss, size);
  free(ss);

  return ret;
}

vector<string> Util::ReadFileToLines(const string &f) {
  string s = ReadFile(f);
  vector<string> v;
  string line;
  // PERF don't need to do so much copying.
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '\r')
      continue;
    else if (s[i] == '\n') {
      v.push_back(line);
      line.clear();
    } else {
      line += s[i];
    }
  }
  return v;
}

// PERF memcpy
vector<unsigned char> Util::ReadFileBytes(const string &f) {
  string s = ReadFile(f);
  vector<unsigned char> bytes;
  bytes.reserve(s.size());
  for (int i = 0; i < s.size(); i++) {
    bytes.push_back((unsigned char)s[i]);
  }
  return bytes;
}


static bool hasmagicf(FILE * f, const string & mag) {
  char * hdr = (char*)malloc(mag.length());
  if (!hdr) return false;

  /* we may not even be able to read sizeof(header) bytes! */
  if (mag.length() != fread(hdr, 1, mag.length(), f)) {
    free(hdr);
    return false;
  }

  for(unsigned int i = 0; i < mag.length(); i++) {
    if (hdr[i] != mag[i]) {
      free(hdr);
      return false;
    }
  }
  
  free(hdr);
  return true;
}

bool Util::hasmagic(string s, const string &mag) {
  FILE * f = fopen(s.c_str(), "rb");
  if (!f) return false;

  bool hm = hasmagicf(f, mag);
  
  fclose(f);
  return hm;
}

string Util::readfilemagic(string s, const string &mag) {
  if (isdir(s)) return "";
  if (s == "") return "";

  // printf("opened %s\n", s.c_str());

  /* PERF try this: and see! */
  // printf("Readfile '%s'\n", s.c_str());

  FILE * f = fopen(s.c_str(), "rb");

  if (!f) return "";


  if (!hasmagicf(f, mag)) {
    fclose(f);
    return "";
  }

  /* ok, now just read file */

  fseek(f, 0, SEEK_END);
  int size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char * ss = (char*)malloc(size);
  fread(ss, 1, size, f);

  fclose(f);

  string ret = string(ss, size);
  free(ss);

  return ret;
}

bool Util::WriteFile(const string &fn, const string &s) {

  FILE * f = fopen(fn.c_str(), "wb");
  if (!f) return false;

  /* XXX check failure */
  fwrite(s.c_str(), 1, s.length(), f);

  fclose(f);
  
  return true;
}

bool Util::WriteFileBytes(const string &fn, const vector<unsigned char> &bytes) {
  FILE * f = fopen(fn.c_str(), "wb");
  if (!f) return false;

  /* XXX check failure */
  fwrite(&bytes[0], 1, bytes.size(), f);

  fclose(f);
  
  return true;
}

string Util::sizes(int i) {
  string s = "    ";
  s[0] = 255&(i >> 24);
  s[1] = 255&(i >> 16);
  s[2] = 255&(i >> 8);
  s[3] = 255& i;
  return s;
}

/* XXX these have terrible names */

/* represent int i (as i mod (2^(b/8))) 
   using only b bytes */
string Util::shint(int b, int i) {
  return sizes(i).substr(4-b, b);
}

/* inverse of shint. does not check that
   there is enough room in s to read b bytes
   from idx ... */
int Util::shout(int b, string s, unsigned int & idx) {
  int r = 0;
  while(b--) {
    r = ((unsigned char)s[idx++]) + (r<<8);
  }
  return r;
}

unsigned int Util::hash(string s) {
  unsigned int h = 0x714FA5DD;
  for(unsigned int i = 0; i < s.length(); i ++) {
    h = (h << 11) | (h >> (32 - 11));
    h *= 3113;
    h ^= (unsigned char)s[i];
  }
  return h;
}

string Util::lcase(string in) {
  string out;
  for(unsigned int i = 0; i < in.length(); i++) {
    if (in[i] >= 'A' &&
	in[i] <= 'Z') out += in[i]|32;
    
    else out += in[i];
  }
  return out;
}

string Util::ucase(string in) {
  string out;
  for(unsigned int i = 0; i < in.length(); i++) {
    if (in[i] >= 'a' &&
	in[i] <= 'z') out += (in[i] & (~ 32));
    
    else out += in[i];
  }
  return out;
}

string Util::fileof(string s) {
  for(long long int i = s.length() - 1; i >= 0; i --) {
    if (s[i] == DIRSEPC) {
      return s.substr(i + 1, s.length () - (i + 1));
    }
  }
  return s;
}

string Util::pathof(string s) {
  if (s == "") return ".";
  for(long long int i = s.length () - 1; i >= 0; i --) {
    if (s[i] == DIRSEPC) {
      return s.substr(0, i);
    }
  }
  return ".";
}

/* XX can use endswith below */
string Util::ensureext(string f, string ext) {
  if (f.length () < ext.length())
    return f + ext;
  else {
    if (f.substr(f.length () - ext.length(),
		 ext.length()) != ext)
      return f + ext;
    else return f;
  }
}

bool Util::endswith (string big, string small_) {
  if (small_.length() > big.length()) return false;
  return big.substr(big.length() - small_.length (),
		    small_.length()) == small_;
}

bool Util::startswith (string big, string small_) {
  if (small_.length () > big.length()) return false;
  return big.substr(0, small_.length()) == small_;
}

int Util::changedir(string s) {
  return !chdir(s.c_str());
}

int Util::getpid() {
  return ::getpid();
}

int stoi(string s) {
  return atoi(s.c_str());
}

/* XXX race. should use creat
   with O_EXCL on unix, at least. */
FILE * Util::open_new(string fname) {
  if (!ExistsFile(fname))
    return fopen(fname.c_str(), "wb+");
  else return 0;
}

string Util::getline(string & chunk) {
  string ret;
  for(unsigned int i = 0; i < chunk.length(); i ++) {
    if (chunk[i] == '\r') continue;
    else if (chunk[i] == '\n') {
      chunk = chunk.substr(i + 1, chunk.length() - (i + 1));
      return ret;
    } else ret += chunk[i];
  }
  /* there doesn't need to be a final trailing newline. */
  chunk = "";
  return ret;
}

/* PERF */
string Util::fgetline(FILE * f) {
  string out;
  int c;
  while ( (c = fgetc(f)), ((c != EOF) && (c != '\n')) ) {
    /* ignore CR */
    if (c != '\r') {
      out += (char)c;
    }
  }
  return out;
}

/* PERF use substr instead of accumulating: this is used
   frequently in the net stuff */
/* return first token in line, removing it from 'line' */
string Util::chop(string & line) {
  for (unsigned int i = 0; i < line.length(); i ++) {
    if (line[i] != ' ') {
      string acc;
      for(unsigned int j = i; j < line.length (); j ++) {
	if (line[j] == ' ') {
	  line = line.substr(j, line.length() - j);
	  return acc;
	} else acc += line[j];
      }
      line = "";
      return acc;
    }
  }
  /* all whitespace */
  line = "";
  return "";
}

/* PERF same */
string Util::chopto(char c, string & line) {
  string acc;
  for (unsigned int i = 0; i < line.length(); i ++) {
    if (line[i] != c) {
      acc += line[i];
    } else {
      if (i < (line.length() - 1)) {
	line = line.substr(i + 1, line.length() - (i + 1));
	return acc;
      } else {
	line = "";
	return acc;
      }
    }
  }
  /* character didn't appear; treat as an invisible
     occurrence at the end */
  line = "";
  return acc;
}

string Util::losewhitel(const string & s) {
  for(unsigned int i = 0; i < s.length(); i ++) {
    switch(s[i]) {
    case ' ':
    case '\n':
    case '\r':
    case '\t':
      /* keep going ... */
      break;
    default:
      return s.substr(i, s.length () - i);
    }
  }
  /* all whitespace */
  return "";
}

string Util::tempfile(string suffix) {
  static int tries = 0;

  char * fname = new char[suffix.length() + 128];

  do {
    sprintf(fname, 
	    "%d_%d_%d%s", 
	    tries, getpid(), random(),
	    suffix.c_str());
    tries++;
  } while(ExistsFile(fname));

  string ret = fname;
  delete fname;
  return ret;
}

/* break up the strings into tokens. A token is either
   a single character (non-numeral) or a sequence of
   numerals that are interpreted as a number. We then
   do lexicographic comparison on this stream of tokens.
   assumes ascii. 

   l < r     -1
   l = r      0
   l > r      1

   XXX this treats

   abc 0123 def
   abc 123 def

   as equal strings. perhaps don't allow 0 to start a 
   number?
   
   n.b. it is easy to overflow here, so perhaps comparing
   as we go is better
*/
int Util::natural_compare(const string & l, const string & r) {

  for(int caseless = 0; caseless < 2; caseless ++) {
    
    unsigned int il = 0;
    unsigned int ir = 0;
    
    while(il < l.length() || ir < r.length()) {
      /* if out of tokens in either string, it comes first. */
      if (il >= l.length()) return -1;
      if (ir >= r.length()) return 1;
      
      int lc = (unsigned char)l[il];
      int rc = (unsigned char)r[ir];
      
      if (lc >= '0' && lc <= '9') {
	if (rc >= '0' && rc <= '9') {
	  /* compare ints */
	  int ll = 0;
	  int rr = 0;

	  while (il < l.length() && l[il] >= '0' && l[il] <= '9') {
	    ll *= 10;
	    ll += (l[il] - '0');
	    il ++;
	  }

	  while (ir < r.length() && r[ir] >= '0' && r[ir] <= '9') {
	    rr *= 10;
	    rr += (r[ir] - '0');
	    ir ++;
	  }

	  if (ll < rr) return -1;
	  if (ll > rr) return 1;
	  /* otherwise continue... */
	
	  il ++;
	  ir ++;
	} else {
	  /* treat numbers larger than any char. */
	  return 1;
	}
      } else {
	if (rc >= '0' && rc <= '9') {
	  return -1;
	} else {
	  /* compare chars */
	  if ((rc|32) >= 'a' && (rc|32) <= 'z' &&
	      (lc|32) >= 'a' && (rc|32) <= 'z' &&
	      !caseless) {
	  
	    /* letters are case-insensitive */
	    if ((lc|32) < (rc|32)) return -1;
	    if ((lc|32) > (rc|32)) return 1;
	  } else {
	    if (lc < rc) return -1;
	    if (lc > rc) return 1;
	  }
	
	  /* same so far. continue... */
	
	  il ++;
	  ir ++;
	}
      }
    
    }
    /* strings look equal when compared
       as case-insensitive. so try again
       sensitive */
  }

  /* strings are case-sensitive equal! */

  return 0;
}

/* same as above, but ignore "the" at beginning */
/* XXX also ignore symbols ie ... at the beginning */
int Util::library_compare(const string & l, const string & r) {

  /* XXX currently IGNOREs symbols, which could give incorrect
     results for strings that are equal other than their
     leading punctuation */
  unsigned int idxl = 0;
  unsigned int idxr = 0;
  while (idxl < l.length() && (!isalnum(l[idxl]))) idxl++;
  while (idxr < r.length() && (!isalnum(r[idxr]))) idxr++;

  bool thel = false;
  bool ther = false;
  if (l.length() >= (5 + idxl) &&
      (l[idxl + 0]|32) == 't' &&
      (l[idxl + 1]|32) == 'h' &&
      (l[idxl + 2]|32) == 'e' &&
      (l[idxl + 3])    == ' ') thel = true;

  if (r.length() >= (5 + idxr) &&
      (r[idxr + 0]|32) == 't' &&
      (r[idxr + 1]|32) == 'h' &&
      (r[idxr + 2]|32) == 'e' &&
      (r[idxr + 3])    == ' ') ther = true;

  if (thel != ther) {
    if (thel) idxl += 4;
    else idxr += 4;
  }

  return natural_compare (l.substr(idxl, l.length () - idxl),
			  r.substr(idxr, r.length () - idxr));
}

/* XXX impossible to specify a spec for just ^ */
bool Util::matchspec(string spec, char c) {
  if (!spec.length()) return false;
  else if (spec[0] == '^') 
  return !matchspec(spec.substr(1, spec.length() - 1), c);

  /* now loop looking for c in string, or ranges */
  for(unsigned int i = 0; i < spec.length(); i ++) {
    /* ok if starts range, since they are inclusive */
    if (spec[i] == c) return true;

    /* handle ranges */
    if (spec[i] == '-') {
      /* can't be first or last */
      if (i && i < (spec.length() - 1)) {
	if (spec[i - 1] <= c &&
	    spec[i + 1] >= c) return true;
	/* skip dash and next char */
	i ++;
      }
    }
  }
  return false; /* no match */
}


bool Util::library_matches(char k, const string & s) {
  /* skip symbolic */
  unsigned int idx = 0;
  while (idx < s.length() && (!isalnum(s[idx]))) idx++;
  
  /* skip 'the' */
  if (s.length() >= (idx + 5) &&
      (s[idx]|32) == 't' &&
      (s[idx + 1]|32) == 'h' &&
      (s[idx + 2]|32) == 'e' &&
      (s[idx + 3])    == ' ') return (s[idx + 4]|32) == (k|32);
  else return (s.length () > 0 && (s[idx]|32) == (k|32));
}

/* try a few methods to remove a file.
   An executable can't remove itself in
   Windows 98, though.
*/
bool Util::remove(string f) {
  if (!ExistsFile(f.c_str())) return true;
  else {
# ifdef WIN32
    /* We can do this by:
       rename tmp  delme1234.exe
       exec(delme1234.exe "-replace" "escape.exe")
          (now, the program has to have a flag -replace
	   that instructs it to replace escape.exe 
	   with itself, then exit)
       .. hopefully exec will unlock the original
       process's executable!! */

    /* try unlinking. if that fails,
       rename it away. */
    if (0 == unlink(f.c_str())) return true;

    string fname = tempfile(".deleteme");
    if (0 == rename(f.c_str(), fname.c_str())) return true;

# else /* posix */
    if (0 == unlink(f.c_str())) return true;
# endif
  } 
  return false;

}

bool Util::move(string src, string dst) {
# if defined(WIN32) || defined(__MINGW32__)
  if (0 == rename(src.c_str(), dst.c_str()))
    return true;
  else return false;

# else /* posix */
  /* XXX actually, posix has rename too. */
  if (0 == link(src.c_str(), dst.c_str())) {
    /* succeed regardless of whether we
       can remove the old link or not. */
    unlink(src.c_str());
    return true;
  } else {
    /* try copy and unlink... (link doesn't work on AFS?) */
    if (copy(src, dst)) {
      unlink(src.c_str());
      return true;
    } return false;
  }
# endif
}


bool Util::copy(string src, string dst) {
  FILE * s = fopen(src.c_str(), "rb");
  if (!s) return false;
  FILE * d = fopen(dst.c_str(), "wb+");
  if (!d) { fclose(s); return false; }

  char buf[256];
  int x = 0;
  do {
    /* XXX doesn't distinguish error from EOF, but... */
    x = (int)fread(buf, 1, 256, s);
    if (x) {
      if ((signed)fwrite(buf, 1, x, d) < x) {
	fclose(s);
	fclose(d);
	return false;
      }
    }
  } while (x == 256);

  fclose(s);
  fclose(d);
  return true;
}

string Util::dirplus(const string &dir_, const string &file) {
  if (dir_.empty()) return file;
  if (!file.empty() && file[0] == DIRSEPC) return file;
  string dir = dir_;
  if (dir[dir.size() - 1] != DIRSEPC)
    dir += DIRSEPC;
  return dir + file;
}

string Util::cdup(const string & dir) {
  /* XXX right second argument to rfind? I want to find the last / */
  size_t idx = dir.rfind(DIRSEP, dir.length () - 1);
  if (idx != (signed)string::npos) {
    if (idx) return dir.substr(0, idx);
    else return ".";
  } else return ".";
}

void Util::createpathfor(string f) {
  string s;
  for(unsigned int i = 0; i < f.length();  i++) {
    if (f[i] == DIRSEPC) {
      /* initial / will cause s == "" for first
	 appearance */
      if (s != "") makedir(s);
    }
    s += f[i];
  }
}

FILE * Util::fopenp(string f, string m) {
  createpathfor(f);
  return fopen(f.c_str(), m.c_str());
}

string Util::replace(string src, string findme, string rep) {
  long long int idx = src.length() - 1;

  if (findme.length () < 1) return src;

  /* idx represents the position in src which, for all chars greater
     than it, there begins no match of findme */
  while (idx >= 0 && idx != (signed)string::npos) {
    idx = src.rfind(findme, idx);
    if (idx != (signed)string::npos) {
      /* do replacement */
      src.replace(idx, findme.length(), rep);
      /* want to ensure termination even if rep contains findmes */
      idx -= findme.length();
    } else break;
  }
  return src;
}

/* implementation of imperative union-find is
   beautifully simple */
void onionfind::onion(int a, int b) {
  if (find(a) != find(b)) arr[find(a)] = b;
}

int onionfind::find(int a) {
  if (arr[a] == -1) return a;
  else return arr[a] = find(arr[a]);
}

onionfind::onionfind(int size) {
  arr = new int[size];
  for (int i = 0; i < size; i++) arr[i] = -1;
}

int bitbuffer::ceil(int bits) {
  return (bits >> 3) + !!(bits & 7);
}


bool bitbuffer::nbits(string s, int n, int & idx, unsigned int & out) {
# define NTHBIT(x) !! (s[(x) >> 3] & (1 << (7 - ((x) & 7))))

  out = 0;

  while(n--) {
    out <<= 1;
    /* check bounds */
    if ((unsigned)(idx >> 3) >= s.length()) return false;
    out |= NTHBIT(idx);
    idx ++;
  }
  return true;

# undef NTHBIT
}

string bitbuffer::getstring() {
  int n = ceil(bits);
  if (data) return string((char *)data, n);
  else return "";
}

void bitbuffer::writebits(int n, unsigned int b) {
  /* assumes position already holds 0 */
# define WRITEBIT(x, v) data[(x) >> 3] |= ((!!v) << (7 - ((x) & 7)))

  /* printf("writebits(%d, %d)\n", n, b); */

  for(int i = 0; i < n; i ++) {
    int bytes_needed = ceil(bits + 1);

    /* allocate more */
    if (bytes_needed > size) {
      int nsize = (size + 1) * 2;
      unsigned char * tmp = (unsigned char *) malloc(nsize * sizeof (unsigned char));
      if (!tmp) abort ();
      memset(tmp, 0, nsize);
      memcpy(tmp, data, size);
      free(data);
      data = tmp;
      size = nsize;
    }

    int bit = !!(b & (1 << (n - (i + 1))));
    /* printf("  write %d at %d\n", bit, bits); */
    WRITEBIT(bits, bit);
    bits++;
  }

# undef WRITEBIT
}

#if WIN32
// for ShellExecute
# include <shellapi.h>
# include <shlobj.h>
#endif

/* return true on success */
bool Util::launchurl(const string & url) {
  /* XXX ??? */
#if 0
#ifdef OSX
  CFURLRef urlcfurl = CFURLCreateWithBytes(kCFAllocatorDefault, 
					   (const UInt8*)url.c_str(), 
					   (CFIndex)strlen(urlstring), 
					   kCFStringEncodingASCII, NULL);
  if (urlcfurl) {
      OSStatus status = LSOpenCFURLRef(urlcfurl, NULL);
      CFRelease(urlcfurl);
      return (status == noErr);
    }
  return 0;
#endif
#endif

#if WIN32
  return ((size_t)ShellExecute(NULL, "open", url.c_str(), 
			       NULL, NULL, SW_SHOWNORMAL)) > 32;
#endif

  /* otherwise.. */
  return false;
}


float Util::randfrac() {
  return random() / (float)RAND_MAX;
}

/* XXX, could use better source of randomness (kernel)
   on systems that support it. But we don't have any
   real need for cryptographic randomness.

   web sequence numbers are chosen randomly, now, so we
   actually do. 
*/
int Util::random () {
# if defined(WIN32) || defined(__MINGW32__)
  return ::rand();
# else
  return ::random();
# endif
}

namespace {
/* ensure that random is seeded */
struct RandomSeed {
  RandomSeed() {
# if defined(WIN32) || defined(__MINGW32__)
    srand((int)time(NULL) ^ getpid());
# else
    srandom(time(0) ^ getpid());
# endif
    /* run it a bit */
    for(int i = 0; i < 256; i ++)
      (void)Util::random();
  }
};

RandomSeed randomseed__unused;
}  // namespace

