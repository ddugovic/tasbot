/* Reimplementation of mutable heaps from scratch, based on
   my sml implementation.
*/

#ifndef __CCLIB_HEAP_H
#define __CCLIB_HEAP_H

#include <vector>

struct Heapable {
  /* The Heap uses this to store the index of this element in the heap,
     which allows you to update the Heapable value and then tell the
     heap to fix the heap invariants.

     You usually shouldn't read or write it.

     If this becomes -1, then the item has been deleted. */
  int location;
};

template<class Priority /* has comparison operators, value semantics */,
         class Value /* :> Heapable */>
class Heap {
 public:
  struct Cell {
    Priority priority;
    Value *value;
  };

  bool Valid(Value *v) { return v->location != -1; }

  // With empty cells.
  Heap() {}

  void Insert(Priority p, Value *v) {
    Cell c;
    c.priority = p;
    c.value = v;
    // a la SetElem.
    cells.push_back(c);
    v->location = cells.size() - 1;

    // No children, so invariant violations are always upward.
    PercolateUp(cells.size() - 1);
  }

  void Delete(Value *v) {
    int i = v->location;
    if (i == -1) {
      fprintf(stderr, "Tried to delete value more than once.\n");
      abort();
    }

    // Invalidate v because it's being deleted.
    v->location = -1;

    // Need old priority to know which direction to percolate.
    Priority pold = cells[i].priority;

    /* if a handle is valid, then heap size > 0 */
    Cell replacement = RemoveLast();
    if (replacement.value == v) {
      // Deleting the last element, so RemoveLast has taken
      // care of everything for us.
      return;
    }

    // Write the replacement over the deleted element.
    SetElem(i, replacement);

    /* might have violated heap invariant */
    if (replacement.priority < pold) {
      PercolateUp(i);
    } else if (pold < replacement.priority) {
      PercolateDown(i);
    }
  }

  bool Empty() const {
    return cells.empty();
  }

  // May not be empty.
  Cell PopMinimum() {
    if (cells.empty()) {
      fprintf(stderr, "Can't PopMinimum on an empty heap.\n");
      abort();
    }

    Cell c = cells[0];
    // PERF could short-circuit some of the code of Delete
    // since we know we are deleting the root.
    Delete(c.value);
    return c;
  }

  Value *PopMinimumValue() {
    return PopMinimum().value;
  }

  Cell GetCell(Value *v) const {
    if (v->location == -1) {
      fprintf(stderr, "Attempt to GetCell on deleted value.\n");
      abort();
    }

    return cells[v->location];
  }

  void AdjustPriority(Value *v, Priority p) {
    /* PERF this is simple, but we could also test whether this
       is an increase or decrease, and then percolate_up or
       percolate_down! */
    Delete(v);
    Insert(p, v);
  }

  int Size() const {
    return cells.size();
  }

 private:
  // Requires that the heap be nonempty.
  Cell RemoveLast() {
    Cell c = cells[cells.size() - 1];
    cells.resize(cells.size() - 1);
    return c;
  }

  /* modify the heap to hold this element at i.
     updates the handle, but doesn't worry about
     what it's overwriting */
  // assumes i is in range
  void SetElem(int i, const Cell &c) {
    cells[i] = c;
    c.value->location = i;
  }

  // Note copy of cells, since they are modified.
  inline void SwapPercDown(int i, const Cell me, int childi, const Cell child) {
    SetElem(childi, me);
    SetElem(i, child);
    PercolateDown(childi);
  }

  /* the element i may violate the order invariant by being too high.
     swap it with children until it doesn't. */
  void PercolateDown(int i) {
    // If we're at the end of the heap, nothing to do. */
    if (2 * i + 1 >= cells.size()) return;

    const Cell &me = cells[i];
    // Left and right children.
    const int li = 2 * i + 1;
    const int ri = 2 * i + 2;

    /* compare to the two children */
    const Cell &cl = cells[li];

    if (me.priority > cl.priority) {
      /* Need to swap, but with which child? */
      if (ri >= cells.size()) {
        // No right child.
        SwapPercDown(i, me, li, cl);
      } else {
        const Cell &cr = cells[ri];
        if (cl.priority < cr.priority) {
          SwapPercDown(i, me, li, cl);
        } else {
          SwapPercDown(i, me, ri, cr);
        }
      }
    } else {
      /* Consider swap with right then. */
      if (ri >= cells.size()) {
        // No right child, done.
        return;
      } else {
        const Cell &cr = cells[ri];
        if (me.priority > cr.priority) {
          SwapPercDown(i, me, ri, cr);
        }
      }
    }
  }

  // Same but for percolating up; again note copy.
  inline void SwapPercUp(int i, const Cell me, int parenti, const Cell parent) {
    SetElem(parenti, me);
    SetElem(i, parent);
    PercolateUp(parenti);
  }

  /* the element i may also violate the order invariant by being too
     low. swap it with its parent until it doesn't.


          a
        /   \
       b     c
      / \   / \
     d   e f   g

     suppose i is the index of c
     we know b<d,e, a<b, c<f,g, a<f,g
     but it may not be the case that a<c, which violates
     the invt.


          c
        /   \
       b     a
      / \   / \
     d   e f   g

     if we swap a and c, we fix this (perhaps introducing
     the same problem now with i=indexof(a)).

     c < b because c < a < b.
     */

  void PercolateUp(int i) {
    // Root? Done.
    if (i == 0) return;
    const Cell &me = cells[i];

    int pi = (i - 1) >> 1;
    const Cell &parent = cells[pi];
    if (me.priority < parent.priority) {
      SwapPercUp(i, me, pi, parent);
    }
  }

  std::vector<Cell> cells;
};

#endif
