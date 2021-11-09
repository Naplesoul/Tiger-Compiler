#include "tiger/absyn/absyn.h"
#include "tiger/semant/semant.h"

#include <set>

namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  root_->SemAnalyze(venv, tenv, 0, errormsg);
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("simplevar:%s\n", sym_->Name().data());
  env::EnvEntry *entry = venv->Look(sym_);
  if (!entry || typeid(*entry) != typeid(env::VarEntry)) {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
    return type::IntTy::Instance(); 
  }
  return static_cast<env::VarEntry *>(entry)->ty_->ActualTy();
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("fieldvar\n");
  type::Ty *record_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!record_ty || typeid(*record_ty) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "not a record type");
    return type::IntTy::Instance();
  }
  auto field_list = (static_cast<type::RecordTy *>(record_ty))->fields_->GetList();
  std::string name = sym_->Name();
  for (const type::Field *field : field_list) {
    if (!name.compare(field->name_->Name())) {
      return field->ty_->ActualTy();
    }
  }
  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().data());
  return type::IntTy::Instance();
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("subscriptvar\n");
  type::Ty *array_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!array_ty || typeid(*array_ty) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "array type required");
    return type::IntTy::Instance();
  }

  type::Ty *sub_ty = subscript_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!sub_ty || typeid(*sub_ty) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "integer type variable expected");
    return type::IntTy::Instance();
  }

  return static_cast<type::ArrayTy *>(array_ty)->ty_->ActualTy();
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("varexp\n");
  return var_->SemAnalyze(venv, tenv, labelcount, errormsg);
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("nilexp\n");
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("intexp\n");
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("stringexp\n");
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("callexp:%s\n", func_->Name().data());
  env::EnvEntry *entry = venv->Look(func_);
  if (!entry || typeid(*entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    printf("callexpend:%s\n", func_->Name().data());
    return type::IntTy::Instance(); 
  }

  auto actual_list = args_->GetList();
  auto formal_list = static_cast<env::FunEntry *>(entry)->formals_->GetList();

  if (actual_list.size() == 0 && formal_list.size() == 0) {
    printf("callexpend:%s\n", func_->Name().data());
    return static_cast<env::FunEntry *>(entry)->result_->ActualTy();
  }

  auto actual_arg = actual_list.begin();
  auto formal_arg = formal_list.begin();

  while (true) {
    type::Ty *formal_arg_type = (*formal_arg)->ActualTy();
    type::Ty *actual_arg_type = (*actual_arg)->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!formal_arg_type->IsSameType(actual_arg_type)) {
      errormsg->Error(pos_, "para type mismatch");
      return type::IntTy::Instance(); 
    }
    
    if (++actual_arg == actual_list.end()) {
      if (++formal_arg == formal_list.end()) {
        return static_cast<env::FunEntry *>(entry)->result_->ActualTy();
      } else {
        errormsg->Error(pos_, "too few params in function %s", func_->Name().data());
        return type::IntTy::Instance(); 
      }
    } else if (++formal_arg == formal_list.end()) {
      errormsg->Error(pos_, "too many params in function %s", func_->Name().data());
      return type::IntTy::Instance(); 
    }
  }
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("opexp\n");
  type::Ty *left_ty = left_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *right_ty = right_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (oper_ == absyn::PLUS_OP || oper_ == absyn::MINUS_OP
      || oper_ == absyn::TIMES_OP || oper_ == absyn::DIVIDE_OP) {
    if (typeid(*left_ty) != typeid(type::IntTy)) {
      errormsg->Error(left_->pos_, "integer required");
    }
    if (typeid(*right_ty) != typeid(type::IntTy)) {
      errormsg->Error(right_->pos_, "integer required");
    }
    return type::IntTy::Instance();
  } else {
    if (!left_ty->IsSameType(right_ty)) {
      errormsg->Error(pos_, "same type required");
      return type::IntTy::Instance();
    }
  }
  return type::IntTy::Instance();
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("recordexp\n");
  type::Ty *record_ty = tenv->Look(typ_);
  if (!record_ty || typeid(*record_ty) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return type::IntTy::Instance();
  }

  auto ty_field_list = static_cast<type::RecordTy *>(record_ty)->fields_->GetList();
  auto val_field_list = fields_->GetList();

  auto ty_field = ty_field_list.begin();
  auto val_field = val_field_list.begin();
  while (true) {
    if ((*ty_field)->name_->Name().compare((*val_field)->name_->Name())) {
      errormsg->Error(pos_, "field name incompatiable");
      return type::IntTy::Instance();
    }

    type::Ty *ty_field_ty = (*ty_field)->ty_->ActualTy();
    type::Ty *val_field_ty = (*val_field)->exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!ty_field_ty->IsSameType(val_field_ty)) {
      errormsg->Error(pos_, "field %s type incompatiable", (*ty_field)->name_->Name().data());
      return type::IntTy::Instance();
    }

    if (++ty_field == ty_field_list.end()) {
      if (++val_field == val_field_list.end()) {
        return static_cast<type::RecordTy *>(record_ty)->ActualTy();
      } else {
        errormsg->Error(pos_, "fields incompatiable");
        return type::IntTy::Instance();
      }
    }

    if (++val_field == val_field_list.end()) {
      errormsg->Error(pos_, "fields incompatiable");
      return type::IntTy::Instance();
    }
  }
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("seqexp\n");
  type::Ty *return_ty = type::VoidTy::Instance();
  for (auto exp : seq_->GetList()) {
    return_ty = exp->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  return return_ty->ActualTy();
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("assignexp\n");
  type::Ty *var_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *exp_ty = exp_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (!var_ty->IsSameType(exp_ty)) {
    errormsg->Error(pos_, "unmatched assign exp");
    return type::IntTy::Instance(); 
  }

  if (typeid(*var_) == typeid(absyn::SimpleVar)) {
    env::EnvEntry *entry = venv->Look(static_cast<absyn::SimpleVar *>(var_)->sym_);
    if (entry->readonly_) {
      errormsg->Error(pos_, "loop variable can't be assigned");
      return type::IntTy::Instance(); 
    }
  }

  return type::VoidTy::Instance();
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("ifexp\n");
  type::Ty *cond_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*cond_ty) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "if condition should be an integer");
    printf("ifexpend\n");
    return type::IntTy::Instance(); 
  }

  printf("cond checked\n");

  type::Ty *then_ty = then_->SemAnalyze(venv, tenv, labelcount, errormsg);
  printf("then checked\n");
  if (elsee_) {
    type::Ty *else_ty = elsee_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!then_ty->IsSameType(else_ty)) {
      errormsg->Error(pos_, "then exp and else exp type mismatch");
      printf("ifexpend\n");
      return type::IntTy::Instance(); 
    }
    printf("else checked\n");
  } else if (typeid(*then_ty) != typeid(type::VoidTy)) {
    errormsg->Error(pos_, "if-then exp's body must produce no value");
    printf("ifexpend\n");
    return type::IntTy::Instance(); 
  }
  printf("ifexpend\n");

  return then_ty->ActualTy();
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("whileexp\n");
  type::Ty *cond_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*cond_ty) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "loop condition should be an integer");
    return type::IntTy::Instance(); 
  }

  type::Ty *loop_ty = body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);
  if (typeid(*loop_ty) != typeid(type::VoidTy)) {
    errormsg->Error(pos_, "while body must produce no value");
    return type::IntTy::Instance(); 
  }

  return type::VoidTy::Instance();
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("forexp\n");
  type::Ty *lo_ty = lo_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *hi_ty = hi_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*lo_ty) != typeid(type::IntTy) || typeid(*hi_ty) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "for exp's range type is not integer");
  }

  venv->BeginScope();
  tenv->BeginScope();
  venv->Enter(var_, new env::VarEntry(lo_ty, true));
  type::Ty *loop_ty = body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);
  venv->EndScope();
  tenv->EndScope();

  if (typeid(*loop_ty) != typeid(type::VoidTy)) {
    errormsg->Error(pos_, "for loop should not return any value");
    return type::IntTy::Instance(); 
  }

  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("breakexp\n");
  if (labelcount > 0) {
    return type::VoidTy::Instance();
  } else {
    errormsg->Error(pos_, "break is not inside any loop");
    return type::IntTy::Instance(); 
  }
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("letexp\n");
  venv->BeginScope();
  tenv->BeginScope();
  for (Dec *dec : decs_->GetList())
    dec->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *result;

  if (!body_)
    result = type::VoidTy::Instance();
  else
    result = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  
  venv->EndScope();
  tenv->EndScope();
  return result;
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("arrayexp\n");
  type::Ty *array_ty = tenv->Look(typ_);
  if (!array_ty) {
    errormsg->Error(pos_, "undefined array type");
    return type::IntTy::Instance(); 
  }
  array_ty = array_ty->ActualTy();
  if (typeid(*array_ty) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "type %s is not an type", typ_->Name().data());
    return type::IntTy::Instance(); 
  }

  type::Ty *size_ty = size_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*size_ty) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "array size should be an integer");
    return type::IntTy::Instance(); 
  }

  type::Ty *element_ty = static_cast<type::ArrayTy *>(array_ty)->ty_->ActualTy();
  type::Ty *val_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!element_ty->IsSameType(val_ty)) {
    errormsg->Error(pos_, "type mismatch");
    return type::IntTy::Instance(); 
  }

  return array_ty;
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("voidexp\n");
  return type::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("functiondec\n");
  std::set<std::string> defined_fun_names;

  for (const FunDec *fun_dec : functions_->GetList()) {
    if (defined_fun_names.find(fun_dec->name_->Name()) != defined_fun_names.end()) {
      errormsg->Error(pos_, "two functions have the same name");
      continue;
    }

    type::Ty *claimed_return_ty = fun_dec->result_ ? tenv->Look(fun_dec->result_) : type::VoidTy::Instance();
    if (!claimed_return_ty) {
      errormsg->Error(pos_, "undefined type %s", fun_dec->result_->Name().data());
      continue;
    }

    type::TyList *formal_list = fun_dec->params_->MakeFormalTyList(tenv, errormsg);
    env::FunEntry *fun_entry = new env::FunEntry(formal_list, claimed_return_ty);
    venv->Enter(fun_dec->name_, fun_entry);
    defined_fun_names.insert(fun_dec->name_->Name());
  }

  for (const FunDec *fun_dec : functions_->GetList()) {
    if (defined_fun_names.find(fun_dec->name_->Name()) == defined_fun_names.end()) {
      continue;
    }

    env::FunEntry *fun_entry = static_cast<env::FunEntry *>(venv->Look(fun_dec->name_));
    type::FieldList *field_list = fun_dec->params_->MakeFieldList(tenv, errormsg);
    venv->BeginScope();
    for (const type::Field *field : field_list->GetList()) {
      venv->Enter(field->name_, new env::VarEntry(field->ty_));
    }

    type::Ty *return_ty = fun_dec->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
    type::Ty *claimed_return_ty = fun_entry->result_->ActualTy();
    venv->EndScope();
    if (!return_ty->IsSameType(claimed_return_ty)) {
      if (typeid(*claimed_return_ty) == typeid(type::VoidTy)) {
        errormsg->Error(pos_, "procedure returns value");
      } else {
        errormsg->Error(pos_, "return type incompatiable");
      }
    }
  }
}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("vardec\n");
  type::Ty *init_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typ_) {
    type::Ty *dec_ty = tenv->Look(typ_);
    if (!dec_ty) {
      errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
      return;
    }
    if (!dec_ty->IsSameType(init_ty)) {
      errormsg->Error(pos_, "type mismatch");
      return;
    }
    venv->Enter(var_, new env::VarEntry(dec_ty));
  } else {
    if (typeid(*init_ty) == typeid(type::NilTy)) {
      errormsg->Error(pos_, "init should not be nil without type specified");
    } else {
      venv->Enter(var_, new env::VarEntry(init_ty));
    }
  }
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("typedec\n");
  std::set<std::string> defined_type_names;

  for (const absyn::NameAndTy *name_and_ty : types_->GetList()) {
    if (defined_type_names.find(name_and_ty->name_->Name()) != defined_type_names.end()) {
      errormsg->Error(pos_, "two types have the same name");
      continue;
    }
    tenv->Enter(name_and_ty->name_, new type::NameTy(name_and_ty->name_, nullptr));
    defined_type_names.insert(name_and_ty->name_->Name());
  }

  for (const absyn::NameAndTy *name_and_ty : types_->GetList()) {
    if (defined_type_names.find(name_and_ty->name_->Name()) == defined_type_names.end()) {
      continue;
    }
    type::NameTy *name_ty = static_cast<type::NameTy*>(tenv->Look(name_and_ty->name_));
    name_ty->ty_ = name_and_ty->ty_->SemAnalyze(tenv, errormsg);
  }

  for (const absyn::NameAndTy *name_and_ty : types_->GetList()) {
    if (defined_type_names.find(name_and_ty->name_->Name()) == defined_type_names.end()) {
      continue;
    }
    type::NameTy *name_ty = static_cast<type::NameTy*>(tenv->Look(name_and_ty->name_));
    type::Ty *check_ty = name_ty->ty_;
    while (typeid(*check_ty) == typeid(type::NameTy)) {
      check_ty = static_cast<type::NameTy*>(check_ty)->ty_;
      if (check_ty == name_ty) {
        errormsg->Error(pos_, "illegal type cycle");
        return;
      }
    }
  }
  for (const absyn::NameAndTy *name_and_ty : types_->GetList()) {
    if (defined_type_names.find(name_and_ty->name_->Name()) == defined_type_names.end()) {
      continue;
    }
    type::NameTy *name_ty = static_cast<type::NameTy*>(tenv->Look(name_and_ty->name_));
    name_ty->ty_ = name_ty->ActualTy();
    tenv->Set(name_and_ty->name_, name_ty->ty_);
  }
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("namety\n");
  type::Ty *ty = tenv->Look(name_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", name_->Name().data());
    return type::IntTy::Instance();
  }
  return new type::NameTy(name_, tenv->Look(name_));
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("recordty\n");
  return new type::RecordTy(record_->MakeFieldList(tenv, errormsg));
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  printf("arrayty\n");
  type::Ty *ty = tenv->Look(array_);
  if (!ty) {
    errormsg->Error(pos_, "undefined array type %s", array_->Name().data());
    return type::IntTy::Instance();
  }
  return new type::ArrayTy(ty);
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}

} // namespace tr
