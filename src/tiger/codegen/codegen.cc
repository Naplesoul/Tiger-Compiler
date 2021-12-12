#include "tiger/codegen/codegen.h"

#include <cassert>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;


} // namespace

namespace cg {

void CodeGen::Codegen() {
  /* TODO: Put your lab5 code here */
  assem::InstrList *instr_list = new assem::InstrList();
  std::string frame_size(frame_->name->Name() + "_framesize");

  std::list<tree::Stm *> stms = traces_->GetStmList()->GetList();
  for (auto stm : stms) {
    stm->Munch(*instr_list, frame_size);
  }

  assem_instr_ = std::make_unique<AssemInstr>(frame::ProcEntryExit2(instr_list));
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}
} // namespace cg

namespace tree {
/* TODO: Put your lab5 code here */

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  left_->Munch(instr_list, fs);
  right_->Munch(instr_list, fs);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::LabelInstr(label_->Name(), label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::OperInstr(
    "jmp `j0",
    nullptr, nullptr, new assem::Targets(jumps_)));
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::OperInstr(
    "cmpq `s0, `s1",
    nullptr,
    new temp::TempList({ right_->Munch(instr_list, fs), left_->Munch(instr_list, fs) }),
    nullptr));
  
  std::string instr_str;
  switch (op_)
  {
  case tree::EQ_OP:
    instr_str = "je `j0";
    break;
  
  case tree::NE_OP:
    instr_str = "jne `j0";
    break;

  case tree::LT_OP:
    instr_str = "jl `j0";
    break;

  case tree::LE_OP:
    instr_str = "jle `j0";
    break;

  case tree::GT_OP:
    instr_str = "jg `j0";
    break;

  case tree::GE_OP:
    instr_str = "jge `j0";
    break;

  default:
    assert(0);
  }

  assem::Targets *targets = new assem::Targets(new std::vector<temp::Label *>({ true_label_ }));
  instr_list.Append(new assem::OperInstr(instr_str, nullptr, nullptr, targets));
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  if (typeid(*dst_) == typeid(tree::MemExp)) {
    tree::MemExp *dst = static_cast<tree::MemExp *>(dst_);
    if (typeid(*src_) == typeid(tree::ConstExp)) {
      // move(const, mem)
      tree::ConstExp *src = static_cast<tree::ConstExp *>(src_);
      tree::BinopExp *binop_exp = nullptr;
      if (typeid(*(dst->exp_)) == typeid(tree::BinopExp)
        && (binop_exp = static_cast<tree::BinopExp *>(dst->exp_))->op_ == tree::PLUS_OP
        && (typeid(*(binop_exp->left_)) == typeid(tree::ConstExp)
          || typeid(*(binop_exp->right_)) == typeid(tree::ConstExp))) {
        
        // move(const, mem(+(const, temp)))
        std::string instr_str;
        temp::Temp *exp_val = nullptr;

        if (typeid(*(binop_exp->left_)) == typeid(tree::ConstExp)) {
          exp_val = static_cast<tree::ConstExp *>(binop_exp->right_)->Munch(instr_list, fs);
          instr_str = "movq $" + std::to_string(src->consti_) + ", " + std::to_string(static_cast<tree::ConstExp *>(binop_exp->left_)->consti_) + "(`s0)";
        } else {
          exp_val = static_cast<tree::ConstExp *>(binop_exp->left_)->Munch(instr_list, fs);
          instr_str = "movq $" + std::to_string(src->consti_) + ", " + std::to_string(static_cast<tree::ConstExp *>(binop_exp->right_)->consti_) + "(`s0)";
        }

        instr_list.Append(new assem::MoveInstr(
          instr_str,
          nullptr, new temp::TempList(exp_val)));

      } else {
        // move(const, mem)
        instr_list.Append(new assem::MoveInstr(
          "movq $" + std::to_string(src->consti_) + ", (`s0)",
          nullptr, new temp::TempList(dst->exp_->Munch(instr_list, fs))));
      }
      
    } else {
      // move(exp, mem)
      temp::Temp *tmp = src_->Munch(instr_list, fs);
      tree::BinopExp *binop_exp = nullptr;
      if (typeid(*(dst->exp_)) == typeid(tree::BinopExp)
        && (binop_exp = static_cast<tree::BinopExp *>(dst->exp_))->op_ == tree::PLUS_OP
        && (typeid(*(binop_exp->left_)) == typeid(tree::ConstExp)
          || typeid(*(binop_exp->right_)) == typeid(tree::ConstExp))) {
        // move(exp, mem(+(const, temp)))
        std::string instr_str;
        temp::Temp *exp_val = nullptr;

        if (typeid(*(binop_exp->left_)) == typeid(tree::ConstExp)) {
          exp_val = static_cast<tree::ConstExp *>(binop_exp->right_)->Munch(instr_list, fs);
          instr_str = "movq `s0, " + std::to_string(static_cast<tree::ConstExp *>(binop_exp->left_)->consti_) + "(`s1)";
        } else {
          exp_val = static_cast<tree::ConstExp *>(binop_exp->left_)->Munch(instr_list, fs);
          instr_str = "movq `s0, " + std::to_string(static_cast<tree::ConstExp *>(binop_exp->right_)->consti_) + "(`s1)";
        }

        instr_list.Append(new assem::MoveInstr(
          instr_str,
          nullptr, new temp::TempList({ tmp, exp_val })));

      } else {
        // move(exp, mem)
        instr_list.Append(new assem::MoveInstr(
          "movq `s0, (`s1)",
          nullptr, new temp::TempList({ tmp, dst->exp_->Munch(instr_list, fs) })));
      }
    }
  } else {
    // move(exp, temp)
    assert(typeid(*dst_) == typeid(tree::TempExp));
    instr_list.Append(new assem::MoveInstr(
      "movq `s0, `d0",
      new temp::TempList(dst_->Munch(instr_list, fs)),
      new temp::TempList(src_->Munch(instr_list, fs))));
  }
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  exp_->Munch(instr_list, fs);
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  frame::X64RegManager *x64_reg_manager = static_cast<frame::X64RegManager *>(reg_manager);
  temp::Temp *result = temp::TempFactory::NewTemp();
  switch (op_)
  {
  case tree::PLUS_OP:
    if (typeid(*left_) == typeid(tree::ConstExp)) {
      instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0",
        new temp::TempList(result),
        new temp::TempList(right_->Munch(instr_list, fs))));

      instr_list.Append(new assem::OperInstr(
        "addq $" + std::to_string(static_cast<tree::ConstExp *>(left_)->consti_) + ", `d0",
        new temp::TempList(result), new temp::TempList(result), nullptr));

    } else if (typeid(*right_) == typeid(tree::ConstExp)) {
      instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0",
        new temp::TempList(result),
        new temp::TempList(left_->Munch(instr_list, fs))));

      instr_list.Append(new assem::OperInstr(
        "addq $" + std::to_string(static_cast<tree::ConstExp *>(right_)->consti_) + ", `d0",
        new temp::TempList(result), new temp::TempList(result), nullptr));

    } else {
      instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0",
        new temp::TempList(result),
        new temp::TempList(left_->Munch(instr_list, fs))));
      
      instr_list.Append(new assem::OperInstr(
        "addq `s0, `d0",
        new temp::TempList(result),
        new temp::TempList({ right_->Munch(instr_list, fs), result }), nullptr));
    }
    return result;
  
  case tree::MINUS_OP:
    if (typeid(*right_) == typeid(tree::ConstExp)) {
      instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0",
        new temp::TempList(result),
        new temp::TempList(left_->Munch(instr_list, fs))));

      instr_list.Append(new assem::OperInstr(
        "subq $" + std::to_string(static_cast<tree::ConstExp *>(right_)->consti_) + ", `d0",
        new temp::TempList(result), new temp::TempList(result), nullptr));
        
    } else {
      instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0",
        new temp::TempList(result),
        new temp::TempList(left_->Munch(instr_list, fs))));
      
      instr_list.Append(new assem::OperInstr(
        "subq `s0, `d0",
        new temp::TempList(result),
        new temp::TempList({ right_->Munch(instr_list, fs), result }), nullptr));
    }

    return result;

  case tree::MUL_OP:
    instr_list.Append(new assem::MoveInstr(
      "movq `s0, `d0",
      new temp::TempList(x64_reg_manager->rax),
      new temp::TempList(left_->Munch(instr_list, fs))));

    instr_list.Append(new assem::OperInstr(
      "imulq `s0",
      new temp::TempList({ x64_reg_manager->rdx, x64_reg_manager->rax }),
      new temp::TempList({right_->Munch(instr_list, fs), x64_reg_manager->rax }), nullptr));
    
    instr_list.Append(new assem::MoveInstr(
      "movq `s0, `d0",
      new temp::TempList(result),
      new temp::TempList(x64_reg_manager->rax)));

    return result;

  case tree::DIV_OP:
    instr_list.Append(new assem::MoveInstr(
      "movq `s0, `d0",
      new temp::TempList(x64_reg_manager->rax),
      new temp::TempList(left_->Munch(instr_list, fs))));
    
    instr_list.Append(new assem::OperInstr(
      "cqto",
      new temp::TempList({ x64_reg_manager->rdx, x64_reg_manager->rax }),
      new temp::TempList(x64_reg_manager->rax), nullptr));

    instr_list.Append(new assem::OperInstr(
      "idivq `s0",
      new temp::TempList({ x64_reg_manager->rdx, x64_reg_manager->rax }),
      new temp::TempList({ right_->Munch(instr_list, fs), x64_reg_manager->rdx, x64_reg_manager->rax }), nullptr));

    instr_list.Append(new assem::MoveInstr(
      "movq `s0, `d0",
      new temp::TempList(result),
      new temp::TempList(x64_reg_manager->rax)));

    return result;

  default:
    assert(0);
    return nullptr;
  }
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *result = temp::TempFactory::NewTemp();

  tree::BinopExp *binop_exp = nullptr;
  if (typeid(*exp_) == typeid(tree::BinopExp)
    && (binop_exp = static_cast<tree::BinopExp *>(exp_))->op_ == tree::PLUS_OP
    && (typeid(*(binop_exp->left_)) == typeid(tree::ConstExp)
      || typeid(*(binop_exp->right_)) == typeid(tree::ConstExp))) {

    std::string instr_str;
    temp::Temp *exp_val = nullptr;

    if (typeid(*(binop_exp->left_)) == typeid(tree::ConstExp)) {
      exp_val = static_cast<tree::ConstExp *>(binop_exp->right_)->Munch(instr_list, fs);
      instr_str = "movq " + std::to_string(static_cast<tree::ConstExp *>(binop_exp->left_)->consti_) + "(`s0), `d0";
    } else {
      exp_val = static_cast<tree::ConstExp *>(binop_exp->left_)->Munch(instr_list, fs);
      instr_str = "movq " + std::to_string(static_cast<tree::ConstExp *>(binop_exp->right_)->consti_) + "(`s0), `d0";
    }

    instr_list.Append(new assem::MoveInstr(
      instr_str,
      new temp::TempList(result),
      new temp::TempList(exp_val)));

  } else {

    instr_list.Append(new assem::MoveInstr(
      "movq (`s0), `d0",
      new temp::TempList(result),
      new temp::TempList(exp_->Munch(instr_list, fs))));
  }

  return result;
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  if (temp_ == reg_manager->FramePointer()) {
    temp::Temp *result = temp::TempFactory::NewTemp();

    std::string frame_size(fs.data(), fs.size());
    instr_list.Append(new assem::MoveInstr(
      "leaq " + frame_size + "(`s0), `d0",
      new temp::TempList(result),
      new temp::TempList(reg_manager->StackPointer())));
      // nullptr));
      
    return result;
  } else {
    return temp_;
  }
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  stm_->Munch(instr_list, fs);
  return exp_->Munch(instr_list, fs);
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *result = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::OperInstr(
    "leaq " + name_->Name() + "(%rip), `d0",
    new temp::TempList(result), nullptr, nullptr));
  return result;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *result = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::MoveInstr(
    "movq $" + std::to_string(consti_) + ", `d0",
    new temp::TempList(result), nullptr));
  return result;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::TempList *args = args_->MunchArgs(instr_list, staticlink_, fs);
  args->Append(reg_manager->StackPointer());
  
  std::string name = static_cast<tree::NameExp *>(fun_)->name_->Name();
  // temp::TempList *dst = new temp::TempList(reg_manager->CallerSaves()->GetList().begin(), reg_manager->CallerSaves()->GetList().end());
  // dst->Append(reg_manager->ReturnValue());
  instr_list.Append(new assem::OperInstr(
    "call " + name,
    new temp::TempList(reg_manager->CallerSaves()->GetList().begin(), reg_manager->CallerSaves()->GetList().end()),
    args, nullptr));
  
  temp::Temp *result = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::MoveInstr(
    "movq `s0, `d0",
    new temp::TempList(result),
    new temp::TempList(reg_manager->ReturnValue())));
  
  return result;
}

temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list, Exp *staticlink, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  int word_size = reg_manager->WordSize();
  if (staticlink) {
    // handle staticlink
    instr_list.Append(new assem::MoveInstr(
      "movq `s0, (`s1)", 
      nullptr,
      new temp::TempList({ staticlink->Munch(instr_list, fs), reg_manager->StackPointer() })));
  }

  std::list<temp::Temp *> arg_regs = reg_manager->ArgRegs()->GetList();
  int arg_reg_num = arg_regs.size();
  int arg_num = exp_list_.size();
  temp::TempList *results = new temp::TempList();

  // first on-stack arg is staticlink, which should be move to (%rsp + word_size)
  int offset = staticlink ? word_size : 0;
  auto arg = exp_list_.begin();
  auto cur_reg = arg_regs.begin();
  for (int i = 0; i < arg_num; ++i) {
    if (i < arg_reg_num) {
      instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0",
        new temp::TempList(*cur_reg),
        new temp::TempList((*arg)->Munch(instr_list, fs))));
      results->Append(*cur_reg);
      ++arg;
      ++cur_reg;
    } else {
      instr_list.Append(new assem::MoveInstr(
        "movq `s0, " + std::to_string(offset) + "(`s1)", 
        nullptr,
        new temp::TempList({ (*arg)->Munch(instr_list, fs), reg_manager->StackPointer() })));
      ++arg;
      offset += word_size;
    }
  }

  return results;
}

} // namespace tree
