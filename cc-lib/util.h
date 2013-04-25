
#ifndef __UTIL_H
#define __UTIL_H

#include <cstdlib>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

using namespace std;

#ifdef WIN32
#   define DIRSEP  "\\"
#   define DIRSEPC '\\'
#else
#   define DIRSEP  "/"
#   define DIRSEPC '/'
#endif

#define UTIL_PI 3.141592653589f

string itos(int i);
int stoi(string s);
string dtos(double d);

struct Util {

  static string ReadFile(const string &f);
  static bool WriteFile(const string &f, const string &s);

  // Reads the lines in the file to the vector. Ignores all
  // carriage returns, including ones not followed by newline.
  static vector<string> ReadFileToLines(const string &f);

  static vector<unsigned char> ReadFileBytes(const string &f);
  static bool WriteFileBytes(const string &f, const vector<unsigned char> &b);

  // XXX terrible names
  static int shout(int, string, unsigned int &);
  static string shint(int b, int i);
  /* converts int to byte string that represents it */
  static string sizes(int i);

  /* only read if the file begins with the magic string */
  static bool hasmagic(string, const string &magic);
  static string readfilemagic(string, const string &magic);


  static string ptos(void *);
  static unsigned int hash(string s);
  /* give /home/tom/ of /home/tom/.bashrc */
  static string pathof(string s);
  static string fileof(string s);

  static string ensureext(string f, string ext);
  static string lcase(string in);
  static string ucase(string in);

  static bool ExistsFile(string);

  /* dirplus("/usr/local", "core") and
     dirplus("/usr/local/", core")  both give  "/usr/local/core"
     dirplus("/usr/local", "/etc/passwd")  gives "/etc/passwd"  */
  static string dirplus(const string &dir, const string &file);

  /* spec is a character spec like "A-Z0-9`,."
     XXX document */
  static bool matchspec(string spec, char c);

  /* An ordering on strings that gives a more "natural" sort:
     Tutorial 1, ..., Tutorial 9, Tutorial 10, Tutorial 11, ...
     rather than
     Tutorial 1, Tutorial 10, Tutorial 11, ..., Tutorial 2, Tutorial 20, ...
  */
  static int natural_compare(const string & l, const string & r);

  /* Same as above, but ignore 'the' at the beginning */
  static int library_compare(const string & l, const string & r);

  /* Is string s alphabetized under char k? */
  static bool library_matches(char k, const string & s);

  /* open a new file. if it exists, return 0 */
  static FILE * open_new(string s);
  /* 0 on failure */
  static int changedir(string s);
  static int random();
  /* random in 0.0 .. 1.0 */
  static float randfrac();
  static int getpid();
  /* anything ending with \n. ignores \r.
     modifies str. */
  static string getline(string & str);
  /* same, for open file. */
  static string fgetline(FILE * f);

  /* chop the first token (ignoring whitespace) off
     of line, modifying line. */
  static string chop(string & line);

  /* number of entries (not . or ..) in dir d */
  static int dirsize(string d);

  /* mylevels/good_tricky   to
     mylevels               to
     . */
  static string cdup(const string & dir);

  /* true iff big ends with small */
  static bool endswith(string big_, string small_);
  /* starts */
  static bool startswith(string big_, string small_);

  /* split the string up to the first
     occurrence of character c. The character
     is deleted from both the returned string and
     the line */
  static string chopto(char c, string & line);

  /* erase any whitespace up to the first
     non-whitespace char. */
  static string losewhitel(const string & s);

  /* try to remove the file. If it
     doesn't exist or is successfully
     removed, then return true. */
  static bool remove(string f);

  /* move a file from src to dst. Return
     true on success. */
  static bool move(string src, string dst);

  /* make a copy by reading/writing */
  static bool copy(string src, string dst);

  static string tempfile(string suffix);

  /* does this file exist and is it a directory? */
  static bool isdir(string s);

  /* same as isdir */
  static bool existsdir(string);

  static bool makedir(string);

  /* try to launch the url with the default browser;
     doesn't work on all platforms. true on success */
  static bool launchurl(const string &);

  /* creates directories for f */
  static void createpathfor(string f);

  /* open, creating directories if necessary */
  static FILE * fopenp(string f, string mode);

  /* replace all occurrences of 'findme' with 'replacewith' in 'src' */
  static string replace(string src, string findme, string replacewith);

  /* called minimum, maximum because some includes
     define these with macros, ugh */
  static int minimum(int a, int b) {
    if (a < b) return a;
    else return b;
  }

  static int maximum(int a, int b) {
    if (a > b) return a;
    else return b;
  }

};

/* drawing lines with Bresenham's algorithm */
struct line {
  static line * create(int x0, int y0, int x1, int y1);
  virtual void destroy() = 0;
  virtual bool next(int & x, int & y) = 0;
  virtual ~line() {};
};

/* union find structure, n.b. union is a keyword */
struct onionfind {
  int * arr;

  int find(int);
  void onion(int,int);

  onionfind(int);
  ~onionfind () { delete [] arr; }

  private:

  onionfind(const onionfind &) { abort (); }
};

/* treats strings as buffers of bits */
struct bitbuffer {
  /* read n bits from the string s from bit offset idx.
     (high bits first)
     write that to output and return true.
     if an error occurs (such as going beyond the end of the string),
     then return false, perhaps destroying idx and output */
  static bool nbits(string s, int n, int & idx, unsigned int & output);

  /* create a new empty bit buffer */
  bitbuffer() : data(0), size(0), bits(0) { }

  /* appends bits to the bit buffer */
  void writebits(int width, unsigned int thebits);

  /* get the contents of the buffer as a string,
     padded at the end if necessary */
  string getstring();

  /* give the number of bytes needed to store n bits */
  static int ceil(int bits);


  ~bitbuffer() { free(data); }

  private:
  unsigned char * data;
  int size;
  int bits;
};

#endif
