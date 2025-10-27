/*
                                  ***   StarForth   ***

  Heap.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:03.193-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/minisat-2.2.1-1/src/minisat/mtl/Heap.h
 */

#ifndef Minisat_Heap_h
#define Minisat_Heap_h

#include "minisat/mtl/Vec.h"
#include "minisat/mtl/IntMap.h"

namespace Minisat {

//=================================================================================================
// A heap implementation with support for decrease/increase key.


template<class K, class Comp, class MkIndex = MkIndexDefault<K> >
class Heap {
    vec<K>                heap;     // Heap of Keys
    IntMap<K,int,MkIndex> indices;  // Each Key's position (index) in the Heap
    Comp                  lt;       // The heap is a minimum-heap with respect to this comparator

    // Index "traversal" functions
    static inline int left  (int i) { return i*2+1; }
    static inline int right (int i) { return (i+1)*2; }
    static inline int parent(int i) { return (i-1) >> 1; }


    void percolateUp(int i)
    {
        K   x  = heap[i];
        int p  = parent(i);
        
        while (i != 0 && lt(x, heap[p])){
            heap[i]          = heap[p];
            indices[heap[p]] = i;
            i                = p;
            p                = parent(p);
        }
        heap   [i] = x;
        indices[x] = i;
    }


    void percolateDown(int i)
    {
        K x = heap[i];
        while (left(i) < heap.size()){
            int child = right(i) < heap.size() && lt(heap[right(i)], heap[left(i)]) ? right(i) : left(i);
            if (!lt(heap[child], x)) break;
            heap[i]          = heap[child];
            indices[heap[i]] = i;
            i                = child;
        }
        heap   [i] = x;
        indices[x] = i;
    }


  public:
    Heap(const Comp& c, MkIndex _index = MkIndex()) : indices(_index), lt(c) {}

    int  size      ()          const { return heap.size(); }
    bool empty     ()          const { return heap.size() == 0; }
    bool inHeap    (K k)       const { return indices.has(k) && indices[k] >= 0; }
    int  operator[](int index) const { assert(index < heap.size()); return heap[index]; }

    void decrease  (K k) { assert(inHeap(k)); percolateUp  (indices[k]); }
    void increase  (K k) { assert(inHeap(k)); percolateDown(indices[k]); }


    // Safe variant of insert/decrease/increase:
    void update(K k)
    {
        if (!inHeap(k))
            insert(k);
        else {
            percolateUp(indices[k]);
            percolateDown(indices[k]); }
    }


    void insert(K k)
    {
        indices.reserve(k, -1);
        assert(!inHeap(k));

        indices[k] = heap.size();
        heap.push(k);
        percolateUp(indices[k]);
    }


    void remove(K k)
    {
        assert(inHeap(k));

        int k_pos  = indices[k];
        indices[k] = -1;

        if (k_pos < heap.size()-1){
            heap[k_pos]          = heap.last();
            indices[heap[k_pos]] = k_pos;
            heap.pop();
            percolateDown(k_pos);
        }else
            heap.pop();
    }


    K removeMin()
    {
        K x              = heap[0];
        heap[0]          = heap.last();
        indices[heap[0]] = 0;
        indices[x]       = -1;
        heap.pop();
        if (heap.size() > 1) percolateDown(0);
        return x; 
    }


    // Rebuild the heap from scratch, using the elements in 'ns':
    void build(const vec<K>& ns) {
        for (int i = 0; i < heap.size(); i++)
            indices[heap[i]] = -1;
        heap.clear();

        for (int i = 0; i < ns.size(); i++){
            // TODO: this should probably call reserve instead of relying on it being reserved already.
            assert(indices.has(ns[i]));
            indices[ns[i]] = i;
            heap.push(ns[i]); }

        for (int i = heap.size() / 2 - 1; i >= 0; i--)
            percolateDown(i);
    }

    void clear(bool dispose = false) 
    { 
        // TODO: shouldn't the 'indices' map also be dispose-cleared?
        for (int i = 0; i < heap.size(); i++)
            indices[heap[i]] = -1;
        heap.clear(dispose); 
    }
};


//=================================================================================================
}

#endif
