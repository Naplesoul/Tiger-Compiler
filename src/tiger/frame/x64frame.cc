#include "tiger/frame/x64frame.h"

extern frame::RegManager *reg_manager;

namespace frame {
/* TODO: Put your lab5 code here */
class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset) : offset(offset) {}
  /* TODO: Put your lab5 code here */

  virtual tree::Exp *toExp(tree::Exp *fp) const override {
    return new tree::MemExp(new tree::BinopExp(tree::PLUS_OP, fp, new tree::ConstExp(offset)));
  }

  virtual int GetOffset() override { return offset; }
};


class InRegAccess : public Access {
public:
  temp::Temp *reg;

  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
  /* TODO: Put your lab5 code here */

  virtual tree::Exp *toExp(tree::Exp *fp) const override {
    return new tree::TempExp(reg);
  }
};

/* TODO: Put your lab5 code here */
X64RegManager::X64RegManager() {
  fp = temp::TempFactory::NewTemp();
  rsp = temp::TempFactory::NewTemp();
  rax = temp::TempFactory::NewTemp();
  r10 = temp::TempFactory::NewTemp();
  r11 = temp::TempFactory::NewTemp();
  rdi = temp::TempFactory::NewTemp();
  rsi = temp::TempFactory::NewTemp();
  rdx = temp::TempFactory::NewTemp();
  rcx = temp::TempFactory::NewTemp();
  r8 = temp::TempFactory::NewTemp();
  r9 = temp::TempFactory::NewTemp();
  rbx = temp::TempFactory::NewTemp();
  rbp = temp::TempFactory::NewTemp();
  r12 = temp::TempFactory::NewTemp();
  r13 = temp::TempFactory::NewTemp();
  r14 = temp::TempFactory::NewTemp();
  r15 = temp::TempFactory::NewTemp();

  temp_map_->Enter(rsp, new std::string("%rsp"));
  temp_map_->Enter(rax, new std::string("%rax"));
  temp_map_->Enter(r10, new std::string("%r10"));
  temp_map_->Enter(r11, new std::string("%r11"));
  temp_map_->Enter(rdi, new std::string("%rdi"));
  temp_map_->Enter(rsi, new std::string("%rsi"));
  temp_map_->Enter(rdx, new std::string("%rdx"));
  temp_map_->Enter(rcx, new std::string("%rcx"));
  temp_map_->Enter(r8, new std::string("%r8"));
  temp_map_->Enter(r9, new std::string("%r9"));
  temp_map_->Enter(rbx, new std::string("%rbx"));
  temp_map_->Enter(rbp, new std::string("%rbp"));
  temp_map_->Enter(r12, new std::string("%r12"));
  temp_map_->Enter(r13, new std::string("%r13"));
  temp_map_->Enter(r14, new std::string("%r14"));
  temp_map_->Enter(r15, new std::string("%r15"));

  regs_ = std::vector<temp::Temp *>({
    rsp, rax, r10, r11, rdi, rsi, rdx, rcx, r8, r9,
    rbx, rbp, r12, r13, r14, r15
  });
}

temp::TempList *X64RegManager::Registers() {
  static temp::TempList *regs = new temp::TempList({
    rsp, rax, r10, r11, rdi, rsi, rdx, rcx, r8, r9,
    rbx, rbp, r12, r13, r14, r15
  });
  return regs;
}

temp::TempList *X64RegManager::ArgRegs() {
  static temp::TempList *arg_regs = new temp::TempList({
    rdi, rsi, rdx, rcx, r8, r9
  });
  return arg_regs;
}

temp::TempList *X64RegManager::CallerSaves() {
  static temp::TempList *caller_saved_regs = new temp::TempList({
    rdi, rsi, rdx, rcx, r8, r9, r10, r11, rax
  });
  return caller_saved_regs;
}

temp::TempList *X64RegManager::CalleeSaves() {
  static temp::TempList *callee_saved_regs = new temp::TempList({
    rbx, rbp, r12, r13, r14, r15
  });
  return callee_saved_regs;
}

temp::TempList *X64RegManager::ReturnSink() {
  static temp::TempList *return_sink_regs = new temp::TempList({
    rbx, rbp, r12, r13, r14, r15, rsp, rax
  });
  return return_sink_regs;
}

int X64RegManager::WordSize() {
  return 8;
}

temp::Temp *X64RegManager::FramePointer() {
  return fp;
}

temp::Temp *X64RegManager::StackPointer() {
  return rsp;
}

temp::Temp *X64RegManager::ReturnValue() {
  return rax;
}


X64Frame::X64Frame(temp::Label *name, std::vector<bool> *escape_list): Frame(name, escape_list) {
  formals = new std::vector<frame::Access *>();
  std::list<temp::Temp *> arg_regs = reg_manager->ArgRegs()->GetList();
  int arg_regs_num = arg_regs.size();
  int word_size = reg_manager->WordSize();
  std::vector<tree::Stm *> stms;
  int args_num = escape_list ? escape_list->size() : 0;

  if (escape_list) {
    auto cur_reg = arg_regs.begin();
    for (int i = 0; i < args_num; ++i) {
      frame::Access *access = AllocLocal((*escape_list)[i]);
      (*formals).push_back(access);
      
      tree::Stm *stm;
      if (i < arg_regs_num) {
        stm = new tree::MoveStm(
          access->toExp(new tree::TempExp(reg_manager->FramePointer())),
          new tree::TempExp(*cur_reg));
        ++cur_reg;
      } else {
        // (fp + 1 * word_size) is for staticlink
        stm = new tree::MoveStm(
          access->toExp(new tree::TempExp(reg_manager->FramePointer())),
          new tree::MemExp(new tree::BinopExp(tree::PLUS_OP,
          new tree::TempExp(reg_manager->FramePointer()),
          new tree::ConstExp((i - 4) * word_size))));
      }
      stms.push_back(stm);
    }
  }

  int stm_num = stms.size();
  if (stm_num == 0) {
    save_arg_stms = new tree::ExpStm(new tree::ConstExp(0));
  } else if (stm_num == 1) {
    save_arg_stms = stms.front();
  } else {
    auto stm = stms.begin();
    save_arg_stms = new tree::SeqStm(*stm, nullptr);
    tree::Stm **cur_stm = &(static_cast<tree::SeqStm *>(save_arg_stms)->right_);
    ++stm;

    for (int i = 1; i < stm_num - 1; ++i) {
      *cur_stm = new tree::SeqStm(*stm, nullptr);
      cur_stm = &static_cast<tree::SeqStm *>(*cur_stm)->right_;
      ++stm;
    }

    *cur_stm = *stm;
  }
}

frame::Access *X64Frame::AllocLocal(bool escape) {
  if (escape) {
    frame_size += reg_manager->WordSize();
    return new InFrameAccess(-frame_size);
  } else {
    return new InRegAccess(temp::TempFactory::NewTemp());
  }
}

tree::Stm *X64Frame::ProcEntryExit1(tree::Stm *stm) {
  std::list<temp::Temp *> callee_saved_regs = reg_manager->CalleeSaves()->GetList();
  int callee_saved_regs_num = callee_saved_regs.size();

  auto reg = callee_saved_regs.begin();
  frame::Access *access = AllocLocal(true);
  tree::Stm *save_reg_stms = new tree::SeqStm(new tree::MoveStm(
    access->toExp(new tree::TempExp(reg_manager->FramePointer())),
    new tree::TempExp(*reg)), nullptr);
  tree::Stm *restore_reg_stms = new tree::SeqStm(new tree::MoveStm(
    new tree::TempExp(*reg),
    access->toExp(new tree::TempExp(reg_manager->FramePointer()))), nullptr);

  tree::Stm **cur_save_stm = &static_cast<tree::SeqStm *>(save_reg_stms)->right_;
  tree::Stm **cur_restore_stm = &static_cast<tree::SeqStm *>(restore_reg_stms)->right_;

  ++reg;
  for (int i = 1; i < callee_saved_regs_num - 1; ++i) {
    access = AllocLocal(true);
    *cur_save_stm = new tree::SeqStm(new tree::MoveStm(
      access->toExp(new tree::TempExp(reg_manager->FramePointer())),
      new tree::TempExp(*reg)), nullptr);
    *cur_restore_stm = new tree::SeqStm(new tree::MoveStm(
      new tree::TempExp(*reg),
      access->toExp(new tree::TempExp(reg_manager->FramePointer()))), nullptr);

    cur_save_stm = &static_cast<tree::SeqStm *>(*cur_save_stm)->right_;
    cur_restore_stm = &static_cast<tree::SeqStm *>(*cur_restore_stm)->right_;
    ++reg;
  }

  access = AllocLocal(true);
  *cur_save_stm = new tree::MoveStm(
    access->toExp(new tree::TempExp(reg_manager->FramePointer())),
    new tree::TempExp(*reg));
  *cur_restore_stm = new tree::MoveStm(
    new tree::TempExp(*reg),
    access->toExp(new tree::TempExp(reg_manager->FramePointer())));

  return new tree::SeqStm(save_arg_stms,
    new tree::SeqStm(save_reg_stms,
    new tree::SeqStm(stm,
    restore_reg_stms)));
}

assem::InstrList *ProcEntryExit2(assem::InstrList *body) {
  body->Append(new assem::OperInstr("", new temp::TempList(), reg_manager->ReturnSink(), nullptr));
  return body;
}

assem::Proc *ProcEntryExit3(frame::Frame *frame, assem::InstrList *body) {
  frame->frame_size += reg_manager->WordSize() * frame->max_arg_num;
  std::string prologue, epilogue;
  std::string sp = *(reg_manager->temp_map_->Look(reg_manager->StackPointer()));

  prologue = "  .set " + frame->name->Name() + "_framesize, " + std::to_string(frame->frame_size) + "\n"
    + frame->name->Name() + ":\n"
    + "\tsubq $" + std::to_string(frame->frame_size) + ", " + sp + "\n";

  epilogue = "\taddq $" + std::to_string(frame->frame_size) + ", " + sp + "\n"
    + "\tretq\n";

  return new assem::Proc(prologue, body, epilogue);
}

} // namespace frame