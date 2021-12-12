#include <map>

#include "tiger/liveness/flowgraph.h"

namespace fg {

void FlowGraphFactory::AssemFlowGraph() {
  /* TODO: Put your lab6 code here */
  std::list<assem::Instr *> instr_list = instr_list_->GetList();
  if (instr_list.size() == 0) {
    return;
  }

  std::map<temp::Label *, graph::Node<assem::Instr> *> label_node_map;
  for (auto instr : instr_list) {
    if (typeid(*instr) == typeid(assem::LabelInstr)) {
      label_node_map[static_cast<assem::LabelInstr *>(instr)->label_]
        = flowgraph_->NewNode(instr);
    }
  }

  auto next_instr = instr_list.begin(), cur_instr = next_instr++;
  graph::Node<assem::Instr> *cur_node = nullptr, *next_node = nullptr;

  if (typeid(**cur_instr) == typeid(assem::LabelInstr)) {
    cur_node = label_node_map[static_cast<assem::LabelInstr *>(*cur_instr)->label_];
  } else {
    cur_node = flowgraph_->NewNode(*cur_instr);
  }

  for ( ; next_instr != instr_list.end(); cur_instr = next_instr++) {
    
    if (typeid(**next_instr) == typeid(assem::LabelInstr)) {
      next_node = label_node_map[static_cast<assem::LabelInstr *>(*next_instr)->label_];
    } else {
      next_node = flowgraph_->NewNode(*next_instr);
    }

    assem::OperInstr *op_instr;
    if (typeid(**cur_instr) == typeid(assem::OperInstr)
      && (op_instr = static_cast<assem::OperInstr *>(*cur_instr))->jumps_) {
      
      for (auto label : *(op_instr->jumps_->labels_)) {
        flowgraph_->AddEdge(cur_node, label_node_map[label]);
      }
      
      // cjump instruction
      if (op_instr->assem_.find("jmp") == std::string::npos) {
        flowgraph_->AddEdge(cur_node, next_node);
      }
    } else {
      flowgraph_->AddEdge(cur_node, next_node);
    }

    cur_node = next_node;
  }
}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return new temp::TempList();
}

temp::TempList *MoveInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return dst_ ? dst_ : new temp::TempList();
}

temp::TempList *OperInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return dst_ ? dst_ : new temp::TempList();
}

temp::TempList *LabelInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return new temp::TempList();
}

temp::TempList *MoveInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return src_ ? src_ : new temp::TempList();
}

temp::TempList *OperInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return src_ ? src_ : new temp::TempList();
}
} // namespace assem
