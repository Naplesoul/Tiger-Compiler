#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/x64frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/frame.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace tr {

tree::BinOp convertBinOp(absyn::Oper oper) {
  switch (oper)
  {
  case absyn::PLUS_OP:
    return tree::PLUS_OP;
  case absyn::MINUS_OP:
    return tree::MINUS_OP;
  case absyn::TIMES_OP:
    return tree::MUL_OP;
  case absyn::DIVIDE_OP:
    return tree::DIV_OP;
  default:
    return tree::PLUS_OP;
  }
}

tree::RelOp convertRelOp(absyn::Oper oper) {
  switch (oper)
  {
  case absyn::EQ_OP:
    return tree::EQ_OP;
  case absyn::NEQ_OP:
    return tree::NE_OP;
  case absyn::GT_OP:
    return tree::GT_OP;
  case absyn::GE_OP:
    return tree::GE_OP;
  case absyn::LT_OP:
    return tree::LT_OP;
  case absyn::LE_OP:
    return tree::LE_OP;
  default:
    return tree::EQ_OP;
  }
}

Access *Access::AllocLocal(Level *level, bool escape) {
  /* TODO: Put your lab5 code here */
  return new tr::Access(level, level->frame_->AllocLocal(escape));
}

std::vector<tr::Access *> Level::FormalAccess() {
  std::vector<tr::Access *> formal_access;
  for (frame::Access *access : *(frame_->formals)) {
    formal_access.push_back(new Access(this, access));
  }
  return formal_access;
}

class Cx {
public:
  temp::Label **trues_;
  temp::Label **falses_;
  tree::Stm *stm_;

  Cx(temp::Label **trues, temp::Label **falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

class Exp {
public:
  [[nodiscard]] virtual tree::Exp *UnEx() const = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() const = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) const = 0;
};

class ExpAndTy {
public:
  tr::Exp *exp_;
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp {
public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() const override { 
    /* TODO: Put your lab5 code here */
    assert(exp_);
    return exp_;
  }
  [[nodiscard]] tree::Stm *UnNx() const override {
    /* TODO: Put your lab5 code here */
    return new tree::ExpStm(exp_);
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    /* TODO: Put your lab5 code here */
    tree::CjumpStm *cjump_stm = new tree::CjumpStm(tree::RelOp::NE_OP, exp_, new tree::ConstExp(0),
      temp::LabelFactory::NewLabel(), temp::LabelFactory::NewLabel());

    return Cx(&(cjump_stm->true_label_), &(cjump_stm->false_label_), cjump_stm);
  }
};

class NxExp : public Exp {
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() const override {
    /* TODO: Put your lab5 code here */
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() const override { 
    /* TODO: Put your lab5 code here */
    assert(stm_);
    return stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    /* TODO: Put your lab5 code here */
    errormsg->Error(0, "Nx cannot be converted to Cx\n");
    return Cx(nullptr, nullptr, nullptr);
  }
};

class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(temp::Label** trues, temp::Label** falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}
  
  [[nodiscard]] tree::Exp *UnEx() const override {
    /* TODO: Put your lab5 code here */
    temp::Temp *return_value = temp::TempFactory::NewTemp();
    temp::Label *true_target = temp::LabelFactory::NewLabel();
    temp::Label *false_target = temp::LabelFactory::NewLabel();
    *(cx_.trues_) = true_target;
    *(cx_.falses_) = false_target;
    return 
      new tree::EseqExp(new tree::MoveStm(new tree::TempExp(return_value), new tree::ConstExp(1)),
      new tree::EseqExp(cx_.stm_,
      new tree::EseqExp(new tree::LabelStm(false_target),
      new tree::EseqExp(new tree::MoveStm(new tree::TempExp(return_value), new tree::ConstExp(0)),
      new tree::EseqExp(new tree::LabelStm(true_target),
      new tree::TempExp(return_value))))));
  }
  [[nodiscard]] tree::Stm *UnNx() const override {
    /* TODO: Put your lab5 code here */
    assert(cx_.stm_);
    return cx_.stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override { 
    /* TODO: Put your lab5 code here */
    return cx_;
  }
};

Exp *compare(absyn::Oper oper, tree::Exp *left, tree::Exp *right) {
  tree::CjumpStm *cjump = new tree::CjumpStm(convertRelOp(oper), left, right, temp::LabelFactory::NewLabel(), temp::LabelFactory::NewLabel());
  return new CxExp(&(cjump->true_label_), &(cjump->false_label_), cjump);
}

void ProgTr::Translate() {
  /* TODO: Put your lab5 code here */
  temp::Label *main_label = temp::LabelFactory::NamedLabel("tigermain");
  main_level_ = std::make_unique<Level>(new frame::X64Frame(main_label, nullptr), nullptr);
  FillBaseVEnv();
  FillBaseTEnv();
  tr::ExpAndTy *result = absyn_tree_->Translate(venv_.get(), tenv_.get(), main_level_.get(), main_label, errormsg_.get());
  frags->PushBack(new frame::ProcFrag(result->exp_->UnNx(), main_level_.get()->frame_));
}

} // namespace tr

namespace absyn {

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return root_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  env::EnvEntry *entry = venv->Look(sym_);
  if (!entry) {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
    return nullptr;
  }
  env::VarEntry *var_entry = dynamic_cast<env::VarEntry *>(entry);

  tree::Exp *compute_staticlink = new tree::TempExp(reg_manager->FramePointer());
  tr::Level *dec_level = var_entry->access_->level_;
  tr::Level *cur_level = level;
  int word_size = reg_manager->WordSize();
  while (cur_level && cur_level != dec_level) {
    compute_staticlink = new tree::MemExp(new tree::BinopExp(
      tree::BinOp::PLUS_OP, compute_staticlink, new tree::ConstExp(word_size)));
    cur_level = cur_level->parent_;
  }
  tree::Exp *addr = var_entry->access_->access_->toExp(compute_staticlink);
  return new tr::ExpAndTy(new tr::ExExp(addr), var_entry->ty_);
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *record = var_->Translate(venv, tenv, level, label, errormsg);
  if (!record) {
    errormsg->Error(pos_, "undefined record");
    return nullptr;
  }

  type::Ty *type = nullptr;
  int offset = 0;
  int word_size = reg_manager->WordSize();
  auto field_list = dynamic_cast<type::RecordTy *>(record->ty_->ActualTy())->fields_->GetList();
  for (type::Field *field : field_list) {
    if (!field->name_->Name().compare(sym_->Name())) {
      type = field->ty_->ActualTy();
      break;
    }
    offset += word_size;
  }
  
  if (!type) {
    errormsg->Error(pos_, "field %s does not exist", sym_->Name().data());
    return nullptr;
  }

  tree::Exp *fetch_value = new tree::MemExp(new tree::BinopExp(
    tree::BinOp::PLUS_OP, record->exp_->UnEx(), new tree::ConstExp(offset)));
  
  return new tr::ExpAndTy(new tr::ExExp(fetch_value), type);
}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *array = var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *subscript = subscript_->Translate(venv, tenv, level, label, errormsg);

  tree::Exp *fetch_value = new tree::MemExp(new tree::BinopExp(
    tree::BinOp::PLUS_OP, array->exp_->UnEx(), new tree::BinopExp(
      tree::BinOp::MUL_OP, subscript->exp_->UnEx(), new tree::ConstExp(reg_manager->WordSize()))));

  return new tr::ExpAndTy(new tr::ExExp(fetch_value), static_cast<type::ArrayTy *>(array->ty_->ActualTy())->ty_->ActualTy());
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::NilTy::Instance());
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(val_)), type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  temp::Label *str_label = temp::LabelFactory::NewLabel();
  frame::StringFrag *str_frag = new frame::StringFrag(str_label, str_);
  frags->PushBack(str_frag);
  return new tr::ExpAndTy(new tr::ExExp(new tree::NameExp(str_label)), type::StringTy::Instance());
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  env::EnvEntry *func = venv->Look(func_);
  if (!func) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    return nullptr;
  }

  env::FunEntry *func_entry = dynamic_cast<env::FunEntry *>(func);

  std::list<absyn::Exp *> actuals = args_->GetList();
  int onstack_arg_num = actuals.size() > 6 ? actuals.size() - 6 : 0;
  tree::ExpList *actual_args = new tree::ExpList();
  for (auto actual : actuals) {
    tree::Exp *actual_arg = actual->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx();
    actual_args->Append(actual_arg);
  }

  tree::Exp *compute_staticlink;
  tr::Level *dec_parent_level = func_entry->level_->parent_;
  if (dec_parent_level) {
    compute_staticlink = new tree::TempExp(reg_manager->FramePointer());
    tr::Level *cur_level = level;
    int word_size = reg_manager->WordSize();
    while (cur_level && cur_level != dec_parent_level) {
      compute_staticlink = new tree::MemExp(new tree::BinopExp(
        tree::BinOp::PLUS_OP, compute_staticlink, new tree::ConstExp(word_size)));
      cur_level = cur_level->parent_;
    }
    // need staticlink
    onstack_arg_num++;
  } else {
    // it was defined in main_level
    compute_staticlink = nullptr;
  }
  
  int *max_arg_num = &(level->frame_->max_arg_num);
  if (onstack_arg_num > *max_arg_num) {
    *max_arg_num = onstack_arg_num;
  }

  return new tr::ExpAndTy(new tr::ExExp(new tree::CallExp(new tree::NameExp(func_), actual_args, compute_staticlink)), func_entry->result_);
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *left = left_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *right = right_->Translate(venv, tenv, level, label, errormsg);

  tree::ExpList *string_equal_args = new tree::ExpList({ left->exp_->UnEx(), right->exp_->UnEx() });

  switch (oper_)
  {
  case absyn::PLUS_OP:
  case absyn::MINUS_OP:
  case absyn::TIMES_OP:
  case absyn::DIVIDE_OP:
    return new tr::ExpAndTy(new tr::ExExp(
      new tree::BinopExp(tr::convertBinOp(oper_), left->exp_->UnEx(), right->exp_->UnEx())), left->ty_->ActualTy());
  case absyn::EQ_OP:
    if (left->ty_->ActualTy()->IsSameType(type::StringTy::Instance())) {
      return new tr::ExpAndTy(new tr::ExExp(
        new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel("string_equal")), string_equal_args, nullptr)),
          type::IntTy::Instance());
    }
  case absyn::NEQ_OP:
    if (left->ty_->ActualTy()->IsSameType(type::StringTy::Instance())) {
      return new tr::ExpAndTy(tr::compare(absyn::EQ_OP,
        new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel("string_equal")), string_equal_args, nullptr),
        new tree::ConstExp(0)), type::IntTy::Instance());
    }
  default:
    return new tr::ExpAndTy(tr::compare(oper_, left->exp_->UnEx(), right->exp_->UnEx()), type::IntTy::Instance());
  }
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,      
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty *type = tenv->Look(typ_)->ActualTy();
  std::list<absyn::EField *> fields = fields_->GetList();
  int field_length = fields.size();
  int word_size = reg_manager->WordSize();
  int total_size = word_size * field_length;
  tree::ExpList *alloc_args = new tree::ExpList({ new tree::ConstExp(total_size) });

  temp::Temp *record_ptr = temp::TempFactory::NewTemp();
  tree::Stm *alloc_stm = new tree::MoveStm(new tree::TempExp(record_ptr),
    new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel("alloc_record")), alloc_args, nullptr));

  tree::SeqStm *stm = new tree::SeqStm(alloc_stm, nullptr);
  tree::Stm **cur_stm = &(stm->right_);

  auto field = fields.begin();
  int i = 0;
  for (; i < field_length - 1; ++i) {
    tree::MoveStm *mov_stm = new tree::MoveStm(
      new tree::MemExp(new tree::BinopExp(tree::BinOp::PLUS_OP, new tree::TempExp(record_ptr), new tree::ConstExp(i * word_size))),
      (*field)->exp_->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx());
    *cur_stm = new tree::SeqStm(mov_stm, nullptr);
    cur_stm = &static_cast<tree::SeqStm *>(*cur_stm)->right_;
    ++field;
  }
  *cur_stm = new tree::MoveStm(
    new tree::MemExp(new tree::BinopExp(tree::BinOp::PLUS_OP, new tree::TempExp(record_ptr), new tree::ConstExp(i * word_size))),
    (*field)->exp_->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx());

  return new tr::ExpAndTy(new tr::ExExp(new tree::EseqExp(stm, new tree::TempExp(record_ptr))), type);
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  if (!seq_) {
    return new tr::ExpAndTy(nullptr, type::NilTy::Instance());
  }
  std::list<absyn::Exp *> exp_list = seq_->GetList();
  int exp_length = exp_list.size();

  if (exp_length == 1) {
    tr::ExpAndTy *exp_ty = exp_list.front()->Translate(venv, tenv, level, label, errormsg);
    return new tr::ExpAndTy(new tr::ExExp(exp_ty->exp_->UnEx()), exp_ty->ty_);
  }

  auto exp = exp_list.begin();
  tree::EseqExp *seq_exp = new tree::EseqExp((*exp)->Translate(venv, tenv, level, label, errormsg)->exp_->UnNx(), nullptr);
  tree::Exp **cur_exp = &(seq_exp->exp_);
  ++exp;

  for (int i = 1; i < exp_length - 1; ++i) {
    *cur_exp = new tree::EseqExp((*exp)->Translate(venv, tenv, level, label, errormsg)->exp_->UnNx(), nullptr);
    cur_exp = &static_cast<tree::EseqExp *>(*cur_exp)->exp_;
    ++exp;
  }

  tr::ExpAndTy *exp_ty = (*exp)->Translate(venv, tenv, level, label, errormsg);
  *cur_exp = exp_ty->exp_->UnEx();

  return new tr::ExpAndTy(new tr::ExExp(seq_exp), exp_ty->ty_);
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,                       
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *var = var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *exp = exp_->Translate(venv, tenv, level, label, errormsg);
  
  return new tr::ExpAndTy(new tr::NxExp(new tree::MoveStm(var->exp_->UnEx(), exp->exp_->UnEx())), type::VoidTy::Instance());
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *test = test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *then = then_->Translate(venv, tenv, level, label, errormsg);
  tr::Cx test_cx = test->exp_->UnCx(errormsg);

  if (elsee_) {
    temp::Label *joint_label = temp::LabelFactory::NewLabel();
    tr::ExpAndTy *elsee = elsee_->Translate(venv, tenv, level, label, errormsg);
    std::vector<temp::Label *> *jumps = new std::vector<temp::Label *>({ joint_label });

    temp::Temp *return_value = temp::TempFactory::NewTemp();

    return new tr::ExpAndTy(new tr::ExExp(
      new tree::EseqExp(test_cx.stm_,
      new tree::EseqExp(new tree::LabelStm(*test_cx.trues_),
      new tree::EseqExp(new tree::MoveStm(new tree::TempExp(return_value), then->exp_->UnEx()),
      new tree::EseqExp(new tree::JumpStm(new tree::NameExp(joint_label), jumps),
      new tree::EseqExp(new tree::LabelStm(*test_cx.falses_),
      new tree::EseqExp(new tree::MoveStm(new tree::TempExp(return_value), elsee->exp_->UnEx()),
      new tree::EseqExp(new tree::JumpStm(new tree::NameExp(joint_label), jumps),
      new tree::EseqExp(new tree::LabelStm(joint_label), new tree::TempExp(return_value)))))))))), then->ty_->ActualTy());

  } else {
    return new tr::ExpAndTy(new tr::NxExp(
      new tree::SeqStm(test_cx.stm_,
      new tree::SeqStm(new tree::LabelStm(*test_cx.trues_),
      new tree::SeqStm(then->exp_->UnNx(), new tree::LabelStm(*test_cx.falses_))))), type::VoidTy::Instance());
  }
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,            
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *test = test_->Translate(venv, tenv, level, label, errormsg);
  tr::Cx test_cx = test->exp_->UnCx(errormsg);
  tr::ExpAndTy *body = body_->Translate(venv, tenv, level, *test_cx.falses_, errormsg);

  temp::Label *test_label = temp::LabelFactory::NewLabel();
  
  std::vector<temp::Label *> *jumps = new std::vector<temp::Label *>({ test_label });

  assert(test_cx.stm_);
  return new tr::ExpAndTy(new tr::NxExp(
    new tree::SeqStm(new tree::LabelStm(test_label),
    new tree::SeqStm(test_cx.stm_,
    new tree::SeqStm(new tree::LabelStm(*test_cx.trues_),
    new tree::SeqStm(body->exp_->UnNx(),
    new tree::SeqStm(new tree::JumpStm(new tree::NameExp(test_label), jumps),
    new tree::LabelStm(*test_cx.falses_))))))), type::VoidTy::Instance());
}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  venv->BeginScope();
  tr::ExpAndTy *hi = hi_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *lo = lo_->Translate(venv, tenv, level, label, errormsg);

  temp::Label *test_label = temp::LabelFactory::NewLabel();
  temp::Label *body_label = temp::LabelFactory::NewLabel();
  temp::Label *joint_label = temp::LabelFactory::NewLabel();

  tr::Access *loop_var = tr::Access::AllocLocal(level, escape_);
  venv->Enter(var_, new env::VarEntry(loop_var, type::IntTy::Instance(), true));
  tr::ExpAndTy *body = body_->Translate(venv, tenv, level, joint_label, errormsg);
  venv->EndScope();

  std::vector<temp::Label *> *jumps = new std::vector<temp::Label *>({ test_label });

  tree::Exp *loop_var_exp_1 = loop_var->access_->toExp(new tree::TempExp(reg_manager->FramePointer()));
  tree::Exp *loop_var_exp_2 = loop_var->access_->toExp(new tree::TempExp(reg_manager->FramePointer()));
  tree::Exp *loop_var_exp_3 = loop_var->access_->toExp(new tree::TempExp(reg_manager->FramePointer()));
  tree::Exp *loop_var_exp_4 = loop_var->access_->toExp(new tree::TempExp(reg_manager->FramePointer()));

  return new tr::ExpAndTy(new tr::NxExp(
    new tree::SeqStm(new tree::MoveStm(loop_var_exp_1, lo->exp_->UnEx()),
    new tree::SeqStm(new tree::LabelStm(test_label),
    new tree::SeqStm(new tree::CjumpStm(tree::LE_OP, loop_var_exp_2, hi->exp_->UnEx(), body_label, joint_label),
    new tree::SeqStm(new tree::LabelStm(body_label),
    new tree::SeqStm(body->exp_->UnNx(),
    new tree::SeqStm(new tree::MoveStm(loop_var_exp_3, new tree::BinopExp(tree::PLUS_OP, loop_var_exp_4, new tree::ConstExp(1))),
    new tree::SeqStm(new tree::JumpStm(new tree::NameExp(test_label), jumps),
    new tree::LabelStm(joint_label))))))))), type::VoidTy::Instance());
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  std::vector<temp::Label *> *jumps = new std::vector<temp::Label *>({ label });
  return new tr::ExpAndTy(new tr::NxExp(new tree::JumpStm(new tree::NameExp(label), jumps)), type::VoidTy::Instance());
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  if (!decs_) {
    tr::ExpAndTy *body = body_->Translate(venv, tenv, level, label, errormsg);
    return new tr::ExpAndTy(new tr::ExExp(body->exp_->UnEx()), body->ty_);
  }

  venv->BeginScope();
  tenv->BeginScope();
  tree::Stm *dec_stm = nullptr;
  std::list<absyn::Dec *> dec_list = decs_->GetList();
  int dec_length = dec_list.size();

  auto dec = dec_list.begin();
  if (dec_length == 0) {
    assert(0);
  } else if (dec_length == 1) {
    dec_stm = (*dec)->Translate(venv, tenv, level, label, errormsg)->UnNx();
  } else {
    dec_stm = new tree::SeqStm((*dec)->Translate(venv, tenv, level, label, errormsg)->UnNx(), nullptr);
    tree::Stm **cur_stm = &static_cast<tree::SeqStm *>(dec_stm)->right_;
    ++dec;

    for (int i = 1; i < dec_length - 1; ++i) {
      *cur_stm = new tree::SeqStm((*dec)->Translate(venv, tenv, level, label, errormsg)->UnNx(), nullptr);
      cur_stm = &static_cast<tree::SeqStm *>(*cur_stm)->right_;
      ++dec;
    }

    *cur_stm = (*dec)->Translate(venv, tenv, level, label, errormsg)->UnNx();
  }

  tr::ExpAndTy *body = body_->Translate(venv, tenv, level, label, errormsg);
  venv->EndScope();
  tenv->EndScope();

  

  return new tr::ExpAndTy(new tr::ExExp(new tree::EseqExp(dec_stm, body->exp_->UnEx())), body->ty_);
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,                    
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *size = size_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *init = init_->Translate(venv, tenv, level, label, errormsg);
  temp::Temp *array_ptr = temp::TempFactory::NewTemp();
  tree::ExpList *alloc_args = new tree::ExpList({ size->exp_->UnEx(), init->exp_->UnEx() });
  tree::Stm *alloc_stm = new tree::MoveStm(new tree::TempExp(array_ptr), 
    new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel("init_array")), alloc_args, nullptr));
  
  return new tr::ExpAndTy(new tr::ExExp(new tree::EseqExp(alloc_stm, new tree::TempExp(array_ptr))), tenv->Look(typ_)->ActualTy());
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::NxExp(new tree::ExpStm(new tree::ConstExp(0))), type::VoidTy::Instance());
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  for (const FunDec *fun_dec : functions_->GetList()) {
    type::Ty *return_ty = fun_dec->result_ ? tenv->Look(fun_dec->result_) : type::VoidTy::Instance();
    type::TyList *formal_list = fun_dec->params_->MakeFormalTyList(tenv, errormsg);

    temp::Label *name_label = temp::LabelFactory::NamedLabel(fun_dec->name_->Name());
    std::vector<bool> *escape_list = new std::vector<bool>();
    std::list<absyn::Field *> field_list = fun_dec->params_->GetList();

    for (auto field : field_list) {
      escape_list->push_back(field->escape_);
    }
    
    tr::Level *new_level = new tr::Level(new frame::X64Frame(name_label, escape_list), level);
    env::FunEntry *fun_entry = new env::FunEntry(new_level, name_label, formal_list, return_ty);
    venv->Enter(fun_dec->name_, fun_entry);
  }

  for (const FunDec *fun_dec : functions_->GetList()) {
    env::FunEntry *fun_entry = static_cast<env::FunEntry *>(venv->Look(fun_dec->name_));
    std::list<absyn::Field *> field_list = fun_dec->params_->GetList();
    venv->BeginScope();

    std::vector<tr::Access *> formals = fun_entry->level_->FormalAccess();
    auto formal = formals.begin();

    for (auto field : field_list) {
      venv->Enter(field->name_, new env::VarEntry(*formal, tenv->Look(field->typ_)->ActualTy(), false));
      ++formal;
    }

    tr::ExpAndTy *body = fun_dec->body_->Translate(venv, tenv, fun_entry->level_, label, errormsg);
    venv->EndScope();

    tree::Stm *stm = new tree::MoveStm(new tree::TempExp(reg_manager->ReturnValue()), body->exp_->UnEx());
    frame::ProcFrag *proc_frag = new frame::ProcFrag(fun_entry->level_->frame_->ProcEntryExit1(stm), fun_entry->level_->frame_);
    frags->PushBack(proc_frag);
  }

  return new tr::ExExp(new tree::ConstExp(0));
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *init = init_->Translate(venv, tenv, level, label, errormsg);
  type::Ty *type = typ_ ? tenv->Look(typ_)->ActualTy() : init->ty_;
  tr::Access *access = tr::Access::AllocLocal(level, escape_);
  venv->Enter(var_, new env::VarEntry(access, type, false));
  return new tr::NxExp(new tree::MoveStm(access->access_->toExp(new tree::TempExp(reg_manager->FramePointer())), init->exp_->UnEx()));
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  for (const absyn::NameAndTy *name_and_ty : types_->GetList()) {
    tenv->Enter(name_and_ty->name_, new type::NameTy(name_and_ty->name_, nullptr));
  }

  for (const absyn::NameAndTy *name_and_ty : types_->GetList()) {
    type::NameTy *name_ty = static_cast<type::NameTy*>(tenv->Look(name_and_ty->name_));
    name_ty->ty_ = name_and_ty->ty_->Translate(tenv, errormsg);
    tenv->Set(name_and_ty->name_, name_ty->ty_);
  }

  return new tr::ExExp(new tree::ConstExp(0));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return tenv->Look(name_)->ActualTy();
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new type::RecordTy(record_->MakeFieldList(tenv, errormsg));
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new type::ArrayTy(tenv->Look(array_));
}

} // namespace absyn
