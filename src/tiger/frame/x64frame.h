//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {
class X64RegManager : public RegManager {
  /* TODO: Put your lab5 code here */

public:
  temp::Temp *fp;
  temp::Temp *rsp, *rax, *r10, *r11;
  // regs for args
  temp::Temp *rdi, *rsi, *rdx, *rcx, *r8, *r9;
  // callee saved regs
  temp::Temp *rbx, *rbp, *r12, *r13, *r14, *r15;
  X64RegManager();
  temp::TempList *Registers() override;
  temp::TempList *ArgRegs() override;
  temp::TempList *CallerSaves() override;
  temp::TempList *CalleeSaves() override;
  temp::TempList *ReturnSink() override;
  int WordSize() override;
  temp::Temp *FramePointer() override;
  temp::Temp *StackPointer() override;
  temp::Temp *ReturnValue() override;
};


class X64Frame : public Frame {
  /* TODO: Put your lab5 code here */
public:
  tree::Stm *save_arg_stms;

  X64Frame(temp::Label *name, std::vector<bool> *escape_list);
  virtual frame::Access *AllocLocal(bool escape) override;
  virtual tree::Stm *ProcEntryExit1(tree::Stm *stm) override;
};

} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
