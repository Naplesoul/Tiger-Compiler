#include "straightline/slp.h"

#include <iostream>

namespace A {
int A::CompoundStm::MaxArgs() const {
  // TODO: put your code here (lab1).
    int args1 = stm1->MaxArgs();
    int args2 = stm2->MaxArgs();
    return args1 > args2 ? args1 : args2;
}

Table *A::CompoundStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  return stm2->Interp(stm1->Interp(t));
}

int A::AssignStm::MaxArgs() const {
  // TODO: put your code here (lab1).
    return exp->MaxArgs();
}

Table *A::AssignStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  IntAndTable *int_table = exp->Interp(t);
  return int_table->t->Update(id, int_table->i);
}

int A::PrintStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  int args = exps->NumExps();
  int maxArgs = exps->MaxArgs();
  return args > maxArgs ? args : maxArgs;
}

Table *A::PrintStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  return exps->Interp(t)->t;
}

int A::IdExp::MaxArgs() const {
  return 0;
}

IntAndTable *A::IdExp::Interp(Table *table) const {
  return new IntAndTable(table->Lookup(id), table);
}

int A::NumExp::MaxArgs() const {
  return 0;
}

IntAndTable *A::NumExp::Interp(Table *table) const {
  return new IntAndTable(num, table);
}

int A::OpExp::MaxArgs() const {
  int args1 = left->MaxArgs();
  int args2 = right->MaxArgs();
  return args1 > args2 ? args1 : args2;
}

IntAndTable *A::OpExp::Interp(Table *table) const {
  IntAndTable *int_table_left = left->Interp(table);
  IntAndTable *int_table_right = right->Interp(int_table_left->t);
  int result_left = int_table_left->i;
  int result_right = int_table_right->i;
  switch (oper) {
  case BinOp::PLUS:
    return new IntAndTable(result_left + result_right, int_table_right->t);
  case BinOp::MINUS:
    return new IntAndTable(result_left - result_right, int_table_right->t);
  case BinOp::TIMES:
    return new IntAndTable(result_left * result_right, int_table_right->t);
  case BinOp::DIV:
    return new IntAndTable(result_left / result_right, int_table_right->t);
  default:
    assert(false);
  }
}

int A::EseqExp::MaxArgs() const {
  int args1 = stm->MaxArgs();
  int args2 = exp->MaxArgs();
  return args1 > args2 ? args1 : args2;
}

IntAndTable *A::EseqExp::Interp(Table *table) const {
  Table *table_stm = stm->Interp(table);
  return exp->Interp(table_stm);
}

int A::PairExpList::MaxArgs() const {
  int args1 = exp->MaxArgs();
  int args2 = tail->MaxArgs();
  return args1 > args2 ? args1 : args2;
}

int A::PairExpList::NumExps() const {
  return tail->NumExps() + 1;
}

IntAndTable *A::PairExpList::Interp(Table *table) const {
  IntAndTable *int_table = exp->Interp(table);
  std::cout << int_table->i;
  return tail->Interp(int_table->t);
}

int A::LastExpList::MaxArgs() const {
  return exp->MaxArgs();
}

int A::LastExpList::NumExps() const {
  return 1;
}

IntAndTable *A::LastExpList::Interp(Table *table) const {
  IntAndTable *int_table = exp->Interp(table);
  std::cout << int_table->i << std::endl;
  return int_table;
}

int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}
}  // namespace A
