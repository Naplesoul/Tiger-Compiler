#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include <set>
#include <list>
#include <stack>
#include <map>

#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"

namespace ra {

using Edge = std::pair<temp::Temp *, temp::Temp *>;

class Result {
public:
  temp::Map *coloring_;
  assem::InstrList *il_;

  Result() : coloring_(nullptr), il_(nullptr) {}
  Result(temp::Map *coloring, assem::InstrList *il)
      : coloring_(coloring), il_(il) {}
  Result(const Result &result) = delete;
  Result(Result &&result) = delete;
  Result &operator=(const Result &result) = delete;
  Result &operator=(Result &&result) = delete;
  ~Result() { delete coloring_, il_; }
};

class RegAllocator {
  /* TODO: Put your lab6 code here */
private:
  int K;

  frame::Frame *frame;
  std::unique_ptr<cg::AssemInstr> assem_instr;
  std::unique_ptr<ra::Result> result;

  std::set<temp::Temp *> precolored, initial;
  std::set<temp::Temp *> simplify_worklist, freeze_worklist, spill_worklist;
  std::set<temp::Temp *> spilled_nodes, coalesced_nodes, colored_nodes;
  std::list<temp::Temp *> select_stack;

  std::set<Edge> coalesced_moves, constrained_moves, frozen_moves, worklist_moves, active_moves;
  
  std::set<Edge> adj_set;
  std::map<temp::Temp *, std::set<temp::Temp *>> adj_list;
  std::map<temp::Temp *, int> degree;
  std::map<temp::Temp *, std::set<Edge>> move_list;
  std::map<temp::Temp *, temp::Temp *> alias;
  std::map<temp::Temp *, temp::Temp *> color;

  void Main();
  void ClearAll();
  live::LiveGraph LivenessAnalysis();
  void Build(live::LiveGraph &live_graph);
  void MakeWorkList();
  void Simplify();
  void Coalesce();
  void Freeze();
  void SelectSpill();
  void AssignColors();
  void RewriteProgram();

  void AddEdge(temp::Temp *u, temp::Temp *v);
  std::set<Edge> NodeMoves(temp::Temp *node);
  bool MoveRelated(temp::Temp *node);
  std::set<temp::Temp *> Adjacent(temp::Temp *node);
  void DecrementDegree(temp::Temp *node);
  void EnableMoves(std::set<temp::Temp *> nodes);
  temp::Temp *GetAlias(temp::Temp *node);
  void AddWorkList(temp::Temp *node);
  bool OK(temp::Temp *t, temp::Temp *r);
  bool Conservative(std::set<temp::Temp *> &nodes);
  void Combine(temp::Temp *u, temp::Temp *v);
  void FreezeMoves(temp::Temp *u);

public:
  RegAllocator(frame::Frame *frame, std::unique_ptr<cg::AssemInstr> assem_instr):
    frame(frame),
    assem_instr(std::move(assem_instr)),
    result(std::make_unique<ra::Result>()) {}

  void RegAlloc();
  std::unique_ptr<ra::Result> TransferResult() { return std::move(result); }
};

} // namespace ra

#endif