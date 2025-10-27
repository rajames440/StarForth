/*
                                  ***   StarForth   ***

  BestLiteralSelector.hpp- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:01.833-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/vampire-4.8/src/Kernel/BestLiteralSelector.hpp
 */
/**
 * @file BestLiteralSelector.hpp
 * Defines classes BestLiteralSelector and CompleteBestLiteralSelector.
 */


#ifndef __BestLiteralSelector__
#define __BestLiteralSelector__

#include <algorithm>

#include "Forwards.hpp"

#include "Lib/Comparison.hpp"
#include "Lib/DArray.hpp"
#include "Lib/List.hpp"
#include "Lib/Set.hpp"
#include "Lib/Stack.hpp"

#include "Term.hpp"
#include "Clause.hpp"
#include "Ordering.hpp"
#include "ApplicativeHelper.hpp"
#include "TermIterators.hpp"

#include "LiteralSelector.hpp"
#include "LiteralComparators.hpp"

namespace Kernel {

using namespace Lib;

/**
 * A literal selector class template that selects the best literal
 * (i.e. the maximal literal in quality ordering specified by
 * QComparator class). Using this literal selector does not
 * maintain completeness.
 *
 * Objects of the QComparator class must provide a method
 * Lib::Comparison compare(Literal*, Literal*)
 * that compares the quality of two literals.
 */
template<class QComparator>
class BestLiteralSelector
    : public LiteralSelector
      {
      public:
  CLASS_NAME(BestLiteralSelector);
  USE_ALLOCATOR(BestLiteralSelector);

  BestLiteralSelector(const Ordering& ordering, const Options& options) : LiteralSelector(ordering, options)
  {
    CALL("BestLiteralSelector::BestLiteralSelector");

    _comp.attachSelector(this);
  }

  bool isBGComplete() const override { return false; }
protected:
  void doSelection(Clause* c, unsigned eligible) override
  {
    CALL("BestLiteralSelector::doSelection");

    unsigned besti=0;
    Literal* best=(*c)[0];
    for(unsigned i=1;i<eligible;i++) {
      Literal* lit=(*c)[i];
      if(_comp.compare(best, lit)==LESS) {
        besti=i;
        best=lit;
      }
    }
    if(besti>0) {
      std::swap((*c)[0], (*c)[besti]);
    }
    c->setSelected(1);

#if VDEBUG
    ensureSomeColoredSelected(c, eligible);
    ASS_EQ(c->numSelected(), 1); //if there is colored, it should be selected by the QComparator
#endif
  }

      private:
  QComparator _comp;
      };


/**
 * A literal selector class template, that tries select the best
 * literal (i.e. the maximal literal in quality ordering specified by
 * QComparator class), but takes completeness of the selection into
 * account.
 *
// * If the best literal is negative, it is selected. If it is among
// * maximal literals, and there is no negative literal among them,
// * they all are selected. If there is a negative literal among
// * maximal literals, we select the best negative literal. If
// * the best literal is neither negative nor maximal, we consider
// * similarly the second best etc...
 * If the worst of maximal positive literal is better than all negative
 * literals, all maximal positive literals are selected. Otherwise the
 * best negative literal is selected.
 *
 * Objects of the QComparator class must provide a method
 * Lib::Comparison compare(Literal*, Literal*)
 * that compares the quality of two literals.
 */
template<class QComparator>
class CompleteBestLiteralSelector
    : public LiteralSelector
{
public:
  CLASS_NAME(CompleteBestLiteralSelector);
  USE_ALLOCATOR(CompleteBestLiteralSelector);

  CompleteBestLiteralSelector(const Ordering& ordering, const Options& options) : LiteralSelector(ordering, options)
  {
    CALL("CompleteBestLiteralSelector::CompleteBestLiteralSelector");

    _comp.attachSelector(this);
  }

  bool isBGComplete() const override { return true; }
protected:
  void doSelection(Clause* c, unsigned eligible) override
  {
    CALL("CompleteBestLiteralSelector::doSelection");
    ASS_G(eligible, 1); //trivial cases should be taken care of by the base LiteralSelector

//    static bool combSup = env.options->combinatorySup();

    static DArray<Literal*> litArr(64);
// TODO AYB decide what to do with selection
//    static Set<unsigned> maxTermHeads;
//    maxTermHeads.reset();
    litArr.initFromArray(eligible,*c);
    litArr.sortInversed(_comp);

    LiteralList* maximals=0;
    Literal* singleSelected=0; //If equals to 0 in the end, all maximal

/*    if(combSup){ 
      fillMaximals(maximals, litArr); 
      LiteralList::Iterator maxIt(maximals);
      while(maxIt.hasNext()){
        Literal* lit = maxIt.next();
        for(unsigned i = 0; i < lit->arity(); i ++){
          TermList t = *lit->nthArgument(i);
          TermList h = ApplicativeHelper::getHead(t);  
          if(h.isVar()){ maxTermHeads.insert(h.var()); }
        }
      }
    } */
    //literals will be selected.
    bool allSelected=false;

    if(isNegativeForSelection(litArr[0]) /*&& 
      (!combSup || canBeSelected(litArr[0], &maxTermHeads))*/) {
      singleSelected=litArr[0];
    } else {
      //if(!combSup){ fillMaximals(maximals, litArr); }
      fillMaximals(maximals, litArr);
      unsigned besti=0;
      LiteralList* nextMax=maximals;
      while(true) {
        if(nextMax->head()==litArr[besti]) {
          nextMax=nextMax->tail();
          if(nextMax==0) {
            break;
          }
        }
        besti++;
        ASS_L(besti,eligible);
        if(isNegativeForSelection(litArr[besti]) /*&& 
           (!combSup || canBeSelected(litArr[besti], &maxTermHeads))*/){
          singleSelected=litArr[besti];
          break;
        }
      }
    }
    if(!singleSelected && !maximals->tail()) {
      //there is only one maximal literal
      singleSelected=maximals->head();
    }
    if(!singleSelected) {
      unsigned selCnt=0;
      for(LiteralList* mit=maximals; mit; mit=mit->tail()) {
        ASS(isPositiveForSelection(mit->head()));
        selCnt++;
      }
      if(selCnt==eligible) {
        allSelected=true;
      }
    }
    if(allSelected) {
      c->setSelected(eligible);
    } else if(!singleSelected) {
      //select multiple maximal literals
      static Stack<Literal*> replaced(16);
      Set<Literal*> maxSet;
      unsigned selCnt=0;

      for(LiteralList* mit=maximals; mit; mit=mit->tail()) {
        maxSet.insert(mit->head());
      }

      while(maximals) {
        if(!maxSet.contains((*c)[selCnt])) {
          replaced.push((*c)[selCnt]);
        }
        (*c)[selCnt]=LiteralList::pop(maximals);
        selCnt++;
      }
      ASS_G(selCnt,1);
      ASS_LE(selCnt,eligible);

      //put back non-selected literals that were removed
      unsigned i=selCnt;
      while(replaced.isNonEmpty()) {
        while(!maxSet.contains((*c)[i])) {
          i++;
          ASS_L(i,eligible);
        }
        (*c)[i++]=replaced.pop();
      }

      c->setSelected(selCnt);
    } else {
      unsigned besti=c->getLiteralPosition(singleSelected);
      if(besti!=0) {
        std::swap((*c)[0],(*c)[besti]);
      }
      c->setSelected(1);
    }
    LiteralList::destroy(maximals);

    ensureSomeColoredSelected(c, eligible);
  }

  void fillMaximals(LiteralList*& maximals, DArray<Literal*> litArr)
  {
    CALL("CompleteBestLiteralSelector::fillMaximals");
    DArray<Literal*>::ReversedIterator rlit(litArr);
    while(rlit.hasNext()) {
      Literal* lit=rlit.next();
      LiteralList::push(lit,maximals);
    }
    _ord.removeNonMaximal(maximals);
  }

  /*bool canBeSelected(Literal* lit, Set<unsigned>* maxTermHeads)
  {
    CALL("CompleteBestLiteralSelector::canBeSelected");

    // TODO: 
    // - fsi always returns terms only.therefore the whole implementation would always return true anyways, so we can comment out this code.
    // - this was discovered during a refactoring. Why was this function here in the first place? 
    // it is definitely dead/unnecessary code at the time of refactoring
    //
    // FirstOrderSubtermIt fsi(lit);
    //
    // while(fsi.hasNext()){
    //   TermList t = TermList(fsi.next());
    //   if(t.isVar() && maxTermHeads->contains(t.var())){
    //     return false;
    //   }
    // }
    return true;
  }*/

private:
  QComparator _comp;
};

};

#endif /* __BestLiteralSelector__ */
