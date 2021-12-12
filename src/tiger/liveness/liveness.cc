#include <set>
#include <map>

#include "tiger/liveness/liveness.h"

template <typename T>
std::set<T> *Union(std::set<T> *s1, std::set<T> *s2) {
  std::set<T> *result = new std::set<T>();
  std::set_union(s1->begin(), s1->end(), s2->begin(), s2->end(), std::inserter(*result, result->begin()));
  return result;
}

template <typename T>
std::set<T> *Diff(std::set<T> *s1, std::set<T> *s2) {
  std::set<T> *result = new std::set<T>();
  std::set_difference(s1->begin(), s1->end(), s2->begin(), s2->end(), std::inserter(*result, result->begin()));
  return result;
}

template <typename T>
bool Equal(std::set<T> *s1, std::set<T> *s2) {
	if (s1->size() == s2->size()) {
		for (auto &e : *s1) {
			if (s2->find(e) == s2->end()) {
				return false;
			}
		}
		return true;
	}
	return false;
}

extern frame::RegManager *reg_manager;

namespace live {

bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList(*this);
  for (auto move : list->GetList()) {
    if (!Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

struct LiveNode {
  std::set<temp::Temp *> *in, *out, *use, *def;

  LiveNode(std::set<temp::Temp *> *_def,
    std::set<temp::Temp *> *_use):
    in(new std::set<temp::Temp *>()),
    out(new std::set<temp::Temp *>()),
    def(_def), use(_use) {}
  
  LiveNode() = delete;

  ~LiveNode() {
    delete in, out, use, def;
  }
};

void LiveGraphFactory::LiveMap() {
  /* TODO: Put your lab6 code here */
  std::map<graph::Node<assem::Instr>*, LiveNode *> live_node_map;
  std::list<graph::Node<assem::Instr>*> node_list = flowgraph_->Nodes()->GetList();

  for (auto node : node_list) {
    live_node_map[node] = new LiveNode(node->NodeInfo()->Def()->GetSet(),
      node->NodeInfo()->Use()->GetSet());
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (auto node : node_list) {
      LiveNode *live_node = live_node_map[node];
      std::set<temp::Temp *> *new_out = new std::set<temp::Temp *>();
      std::set<temp::Temp *> *new_in = new std::set<temp::Temp *>(live_node->use->begin(), live_node->use->end());
      
      // update in set
      std::set<temp::Temp *> *out_def = Diff(live_node->out, live_node->def);
      new_in->insert(out_def->begin(), out_def->end());
      delete out_def;

      // update out set
      if (node->Succ()) {
        std::list<fg::FNodePtr> succ_nodes = node->Succ()->GetList();
        for (auto succ_node : succ_nodes) {
          LiveNode *succ_live_node = live_node_map[succ_node];
          new_out->insert(succ_live_node->in->begin(), succ_live_node->in->end());
        }
      }

      changed = changed || !Equal(new_in, live_node->in) || !Equal(new_out, live_node->out);
      
      delete live_node->in;
      delete live_node->out;

      live_node->in = new_in;
      live_node->out = new_out;
    }
  };

  for (auto &live_node_pair : live_node_map) {
    in_->Enter(live_node_pair.first,
      new temp::TempList(live_node_pair.second->in->begin(), live_node_pair.second->in->end()));
    out_->Enter(live_node_pair.first,
      new temp::TempList(live_node_pair.second->out->begin(), live_node_pair.second->out->end()));
  }
}

void LiveGraphFactory::InterfGraph() {
  /* TODO: Put your lab6 code here */
  std::map<temp::Temp *, INodePtr> temp_node_map;

  // add precolored registers
  std::list<temp::Temp *> reg_list = reg_manager->Registers()->GetList();
  for (auto reg : reg_list) {
    temp_node_map[reg] = live_graph_.interf_graph->NewNode(reg);
  }

  for (auto reg1 : reg_list) {
    for (auto reg2 : reg_list) {
      if (reg1 != reg2) {
        live_graph_.interf_graph->AddEdge(temp_node_map[reg1], temp_node_map[reg2]);
      }
    }
  }

  // add all nodes
  std::list<graph::Node<assem::Instr>*> node_list = flowgraph_->Nodes()->GetList();
  for (auto node : node_list) {
    assem::Instr *instr = node->NodeInfo();
    std::list<temp::Temp *> def = instr->Def()->GetList();
    for (auto def_tmp : def) {
      if (temp_node_map.find(def_tmp) == temp_node_map.end()) {
        temp_node_map[def_tmp] = live_graph_.interf_graph->NewNode(def_tmp);
      }
    }
    std::list<temp::Temp *> use = instr->Use()->GetList();
    for (auto use_tmp : use) {
      if (temp_node_map.find(use_tmp) == temp_node_map.end()) {
        temp_node_map[use_tmp] = live_graph_.interf_graph->NewNode(use_tmp);
      }
    }
  }
  
  // add edges to interf_graph
  for (auto node : node_list) {
    assem::Instr *instr = node->NodeInfo();
    std::list<temp::Temp *> out_list = out_->Look(node)->GetList();
    
    if (typeid(*instr) == typeid(assem::MoveInstr)
      && !(static_cast<assem::MoveInstr *>(instr)->assem_.compare("movq `s0, `d0"))) {
      assem::MoveInstr *mov_instr = static_cast<assem::MoveInstr *>(instr);
      INodePtr use = temp_node_map[mov_instr->Use()->NthTemp(0)];
      INodePtr def = temp_node_map[mov_instr->Def()->NthTemp(0)];

      if (!live_graph_.moves->Contain(use, def)) {
        live_graph_.moves->Append(use, def);
      }

      for (auto out_tmp : out_list) {
        INodePtr out = temp_node_map[out_tmp];
        if (use != out) {
          live_graph_.interf_graph->AddEdge(def, out);
        }
      }
    } else {
      if (instr->Def()) {
        for (auto def_tmp : instr->Def()->GetList()) {
          INodePtr def = temp_node_map[def_tmp];
          for (auto out_tmp : out_list) {
            INodePtr out = temp_node_map[out_tmp];
            live_graph_.interf_graph->AddEdge(def, out);
          }
        }
      }
    }
  }
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}

} // namespace live