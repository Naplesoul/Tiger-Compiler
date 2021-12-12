#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"

extern frame::RegManager *reg_manager;

template <typename T>
std::set<T> Union(std::set<T> &s1, std::set<T> &s2) {
  std::set<T> result;
  std::set_union(s1.begin(), s1.end(), s2.begin(), s2.end(), std::inserter(result, result.begin()));
  return result;
}

template <typename T>
std::set<T> Union(std::set<T> *s1, std::set<T> *s2) {
  std::set<T> result;
  std::set_union(s1->begin(), s1->end(), s2->begin(), s2->end(), std::inserter(result, result.begin()));
  return result;
}

template <typename T>
std::set<T> Union(std::list<T> &s1, std::set<T> &s2) {
  std::set<T> result;
  std::set_union(s1.begin(), s1.end(), s2.begin(), s2.end(), std::inserter(result, result.begin()));
  return result;
}

template <typename T>
std::set<T> Intersect(std::set<T> &s1, std::set<T> &s2) {
  std::set<T> result;
  std::set_intersection(s1.begin(), s1.end(), s2.begin(), s2.end(), std::inserter(result, result.begin()));
  return result;
}

template <typename T>
std::set<T> Diff(std::set<T> &s1, std::set<T> &s2) {
  std::set<T> result;
  std::set_difference(s1.begin(), s1.end(), s2.begin(), s2.end(), std::inserter(result, result.begin()));
  return result;
}

template <typename T>
bool Equal(std::set<T> &s1, std::set<T> &s2) {
	if (s1.size() == s2.size()) {
		for (auto &e : s1) {
			if (s2.find(e) == s2.end()) {
				return false;
			}
		}
		return true;
	}
	return false;
}

namespace ra {
/* TODO: Put your lab6 code here */

void RegAllocator::RegAlloc() {
  ClearAll();
  K = reg_manager->Registers()->GetList().size();
	for (auto temp : reg_manager->Registers()->GetList()) {
		precolored.insert(temp);
	}
  Main();

	// enter colors
	temp::Map *coloring = temp::Map::Empty();
	temp::Map *temp_map = reg_manager->temp_map_;
	for (auto col : color) {
		coloring->Enter(col.first, temp_map->Look(col.second));
	}

	// remove useless moves
	assem::InstrList *instr_list = new assem::InstrList();

	for (auto instr : assem_instr->GetInstrList()->GetList()) {
		if (typeid(*instr) == typeid(assem::MoveInstr)) {
			assem::MoveInstr *move_instr = static_cast<assem::MoveInstr *>(instr);
			if (!move_instr->assem_.compare("movq `s0, `d0")
				&& !coloring->Look(move_instr->src_->NthTemp(0))->compare(*coloring->Look(move_instr->dst_->NthTemp(0)))) {
				
				continue;
			}
		}
		instr_list->Append(instr);
	}
	result->coloring_ = coloring;
	result->il_ = instr_list;
}

void RegAllocator::Main() {
  live::LiveGraph live_graph = LivenessAnalysis();
  Build(live_graph);
  MakeWorkList();

  while (!simplify_worklist.empty() || !worklist_moves.empty()
    || !freeze_worklist.empty() || !spill_worklist.empty()) {

    if (!simplify_worklist.empty()) { Simplify(); }
    else if (!worklist_moves.empty()) { Coalesce(); }
    else if (!freeze_worklist.empty()) { Freeze(); }
    else if (!spill_worklist.empty()) { SelectSpill(); }
  }

  AssignColors();
  if (!spilled_nodes.empty()) {
    RewriteProgram();
		ClearAll();
    Main();
  }
}

void RegAllocator::ClearAll() {
  // precolored.clear();
  initial.clear();

  simplify_worklist.clear();
  freeze_worklist.clear();
  spill_worklist.clear();

  spilled_nodes.clear();
  coalesced_nodes.clear();
  colored_nodes.clear();

  select_stack.clear();

  coalesced_moves.clear();
  constrained_moves.clear();
  frozen_moves.clear();
  worklist_moves.clear();
  active_moves.clear();

	adj_set.clear();
	adj_list.clear();
	degree.clear();
	move_list.clear();
	alias.clear();
	color.clear();
}

live::LiveGraph RegAllocator::LivenessAnalysis() {
  fg::FlowGraphFactory flow_graph_factory(assem_instr->GetInstrList());
  flow_graph_factory.AssemFlowGraph();
  fg::FGraphPtr flow_graph = flow_graph_factory.GetFlowGraph();

  live::LiveGraphFactory live_graph_factory(flow_graph);
  live_graph_factory.Liveness();
  return live_graph_factory.GetLiveGraph();
}

void RegAllocator::Build(live::LiveGraph &live_graph) {
	// build initial & initial adj_list, move_list, degree
	for (auto node : live_graph.interf_graph->Nodes()->GetList()) {
		temp::Temp *tmp = node->NodeInfo();
		degree[tmp] = 0;
		if (precolored.find(tmp) != precolored.end()) {
			color[tmp] = tmp;
			degree[tmp] = std::numeric_limits<int>::max() >> 1;
		} else {
			initial.insert(tmp);
		}
		alias[tmp] = tmp;
		adj_list[tmp] = std::set<temp::Temp *>();
		move_list[tmp] = std::set<Edge>();
	}

	// build worklist_moves & move_list
	for (auto move : live_graph.moves->GetList()) {
		temp::Temp *src = move.first->NodeInfo();
		temp::Temp *dst = move.second->NodeInfo();
		worklist_moves.insert(Edge(src, dst));
		move_list[src].insert(Edge(src, dst));
		move_list[dst].insert(Edge(src, dst));
	}


	// add edges
	for (auto node : live_graph.interf_graph->Nodes()->GetList()) {
		for (auto adj_node : node->Succ()->GetList()) {
			AddEdge(node->NodeInfo(), adj_node->NodeInfo());
		}
	}
}

void RegAllocator::MakeWorkList() {
  for (auto node : initial) {
	if (precolored.find(node) != precolored.end()) {
		continue;
	}
    if (degree[node] >= K) {
      spill_worklist.insert(node);
    } else if (MoveRelated(node)) {
			freeze_worklist.insert(node);
		} else {
			simplify_worklist.insert(node);
		}
  }
  initial.clear();
}

void RegAllocator::Simplify() {
	temp::Temp *node = *simplify_worklist.begin();
	simplify_worklist.erase(node);
  select_stack.push_back(node);
	for (auto m : Adjacent(node)) {
		DecrementDegree(m);
	}
}

void RegAllocator::Coalesce() {
	Edge m = *worklist_moves.begin();
	temp::Temp *x = m.first, *y = m.second;
	x = GetAlias(x);
	y = GetAlias(y);

	temp::Temp *u, *v;
	if (precolored.find(y) != precolored.end()) {
		u = y;
		v = x;
	} else {
		u = x;
		v = y;
	}
	worklist_moves.erase(m);

	if (u == v) {
		coalesced_moves.insert(m);
		AddWorkList(u);
	} else if (precolored.find(v) != precolored.end()
		|| adj_set.find(Edge(u, v)) != adj_set.end()) {

		constrained_moves.insert(m);
		AddWorkList(u);
		AddWorkList(v);
	} else {
		bool ok = true;
		for (auto t : Adjacent(v)) {
			ok = ok && OK(t, u);
		}

		auto adju = Adjacent(u);
		auto adjv = Adjacent(v);
		auto U = Union(adju, adjv);
		if (precolored.find(u) != precolored.end() && ok
			|| precolored.find(u) == precolored.end() && Conservative(U)) {

			coalesced_moves.insert(m);
			Combine(u, v);
			AddWorkList(u);
		} else {
			active_moves.insert(m);
		}
	}
}

void RegAllocator::Freeze() {
	temp::Temp *u = *freeze_worklist.begin();
	freeze_worklist.erase(u);
	simplify_worklist.insert(u);
	FreezeMoves(u);
}

void RegAllocator::SelectSpill() {
	int max_degree = -1;
	temp::Temp *node_to_spill = nullptr;
	for (auto node : spill_worklist) {
		if (spilled_nodes.find(node) == spilled_nodes.end()
			&& precolored.find(node) == precolored.end()
			&& degree[node] > max_degree) {
			
			node_to_spill = node;
			max_degree = degree[node];
		}
	}
	spill_worklist.erase(node_to_spill);
	simplify_worklist.insert(node_to_spill);
	FreezeMoves(node_to_spill);
}

void RegAllocator::AssignColors() {
	while (!select_stack.empty()) {
		temp::Temp *n = select_stack.back();
		select_stack.pop_back();
		std::set<temp::Temp *> ok_colors;
		for (auto reg : reg_manager->Registers()->GetList()) {
			ok_colors.insert(reg);
		}
		
		for (auto w : adj_list[n]) {
			if (colored_nodes.find(GetAlias(w)) != colored_nodes.end()
				|| precolored.find(GetAlias(w)) != precolored.end()) {
				
				ok_colors.erase(color[GetAlias(w)]);
			}
		}

		if (ok_colors.empty()) {
			spilled_nodes.insert(n);
		} else {
			colored_nodes.insert(n);
			color[n] = *ok_colors.begin();
		}
	}

	for (auto n : coalesced_nodes) {
		color[n] = color[GetAlias(n)];
	}
}

void RegAllocator::RewriteProgram() {
	std::map<temp::Temp *, frame::Access *> tmp_access_map;
	for (auto v : spilled_nodes) {
		tmp_access_map[v] = frame->AllocLocal(true);
	}
	// std::list<assem::Instr *> instr_list = assem_instr->GetInstrList()->GetList();
	for (auto instr = assem_instr->GetInstrList()->GetList().begin(); instr != assem_instr->GetInstrList()->GetList().end(); ++instr){
		temp::TempList *def_list = (*instr)->Def();
		temp::TempList *use_list = (*instr)->Use();
		std::map<temp::Temp *, temp::Temp *> new_tmp_map;

		if (def_list) {
			for (auto &def : def_list->GetList()) {
				if (tmp_access_map.find(def) != tmp_access_map.end()) {
					frame::Access *access = tmp_access_map[def];
					int offset = access->GetOffset();
					if (new_tmp_map.find(def) == new_tmp_map.end()) {
						temp::Temp *new_tmp = temp::TempFactory::NewTemp();
						new_tmp_map[def] = new_tmp;
						def = new_tmp;
					} else {
						def = new_tmp_map[def];
					}
					assem::MoveInstr *move_instr = new assem::MoveInstr(
						"movq `s0, (" + frame->name->Name() + "_framesize - " + std::to_string(-offset) + ")(`s1)",
						nullptr, new temp::TempList({ def, reg_manager->StackPointer() }));
					assem_instr->GetInstrList()->Insert(std::next(instr), move_instr);
				}
			}
		}

		if (use_list) {
			for (auto &use : use_list->GetList()) {
				if (tmp_access_map.find(use) != tmp_access_map.end()) {
					frame::Access *access = tmp_access_map[use];
					int offset = access->GetOffset();
					if (new_tmp_map.find(use) == new_tmp_map.end()) {
						temp::Temp *new_tmp = temp::TempFactory::NewTemp();
						new_tmp_map[use] = new_tmp;
						use = new_tmp;
					} else {
						use = new_tmp_map[use];
					}
					assem::MoveInstr *move_instr = new assem::MoveInstr(
						"movq (" + frame->name->Name() + "_framesize - " + std::to_string(-offset) + ")(`s0), `d0",
						new temp::TempList({ use }),
						new temp::TempList({ reg_manager->StackPointer() }));
					assem_instr->GetInstrList()->Insert(instr, move_instr);
				}
			}
		}
	}
}

void RegAllocator::AddEdge(temp::Temp *u, temp::Temp *v) {
	if (u != v && adj_set.find(Edge(u, v)) == adj_set.end()) {
		adj_set.insert(Edge(u, v));
		adj_set.insert(Edge(v, u));
		
		if (precolored.find(u) == precolored.end()) {
			adj_list[u].insert(v);
			degree[u] += 1;
		}
		if (precolored.find(v) == precolored.end()) {
			adj_list[v].insert(u);
			degree[v] += 1;
		}
	}
}

std::set<Edge> RegAllocator::NodeMoves(temp::Temp *node) {
  std::set<Edge> u = Union(active_moves, worklist_moves);
  return Intersect(move_list[node], u);
}

bool RegAllocator::MoveRelated(temp::Temp *node) {
  return !NodeMoves(node).empty();
}

std::set<temp::Temp *> RegAllocator::Adjacent(temp::Temp *node) {
	std::set<temp::Temp *> u = Union(select_stack, coalesced_nodes);
	return Diff(adj_list[node], u);
}

void RegAllocator::DecrementDegree(temp::Temp *node) {
	int d = degree[node];
	degree[node]--;
	if (d == K) {
		std::set<temp::Temp *> adj = Adjacent(node);
		adj.insert(node);
		EnableMoves(adj);
		spill_worklist.erase(node);
		if (MoveRelated(node)) {
			freeze_worklist.insert(node);
		} else {
			simplify_worklist.insert(node);
		}
	}
}

void RegAllocator::EnableMoves(std::set<temp::Temp *> nodes) {
	for (auto node : nodes) {
		for (auto m : NodeMoves(node)) {
			if (active_moves.find(m) != active_moves.end()) {
				active_moves.erase(m);
				worklist_moves.insert(m);
			}
		}
	}
}

temp::Temp *RegAllocator::GetAlias(temp::Temp *node) {
	if (coalesced_nodes.find(node) != coalesced_nodes.end()) {
		return GetAlias(alias[node]);
	} else {
		return node;
	}
}

void RegAllocator::AddWorkList(temp::Temp *node) {
	if (precolored.find(node) == precolored.end()
		&& !MoveRelated(node) && degree[node] < K) {
		
		freeze_worklist.erase(node);
		simplify_worklist.insert(node);
	}
}

bool RegAllocator::OK(temp::Temp *t, temp::Temp *r) {
	return degree[t] < K
		|| precolored.find(t) != precolored.end()
		|| adj_set.find(Edge(t, r)) != adj_set.end();
}

bool RegAllocator::Conservative(std::set<temp::Temp *> &nodes) {
	int k = 0;
	for (auto node : nodes) {
		if (degree[node] >= K) {
			++k;
		}
	}
	return (k < K);
}

void RegAllocator::Combine(temp::Temp *u, temp::Temp *v) {
	if (freeze_worklist.find(v) != freeze_worklist.end()) {
		freeze_worklist.erase(v);
	} else {
		spill_worklist.erase(v);
	}
	coalesced_nodes.insert(v);
	alias[v] = u;
	move_list[u].insert(move_list[v].begin(), move_list[v].end());
	EnableMoves(std::set<temp::Temp *>({v}));

	for (auto t : Adjacent(v)) {
		AddEdge(t, u);
		DecrementDegree(t);
	}

	if (precolored.find(u) == precolored.end() && degree[u] >= K && freeze_worklist.find(u) != freeze_worklist.end()) {
		freeze_worklist.erase(u);
		spill_worklist.insert(u);
	}
}


void RegAllocator::FreezeMoves(temp::Temp *u) {
	for (auto m : NodeMoves(u)) {
		temp::Temp *x = m.first, *y = m.second, *v;
		if (GetAlias(y) == GetAlias(u)) {
			v = GetAlias(x);
		} else {
			v = GetAlias(y);
		}
		active_moves.erase(m);
		frozen_moves.insert(m);
		if (NodeMoves(v).empty() && degree[v] < K) {
			freeze_worklist.erase(v);
			simplify_worklist.insert(v);
		}
	}
}

} // namespace ra