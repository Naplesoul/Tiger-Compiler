**Comments handling:**

碰到 /* 就进入\<COMMENT\>状态，并将comment_level_设为1

在\<COMMENT\>状态中碰到/*就将comment\_level\_加1，碰到\*\就将comment\_level\_减1并检查它是否为1。若是则说明comment结束，重新进入INITIAL。碰到其他的字符就跳过。碰到EOF则说明comment号不配对。

**Strings handling:**

碰到"则进入\<STR\>，并清空string_buf_

在\<STR\>中碰到"说明string结束，将string保存后重新进入INITIAL。

在string中碰到\n\t等转义符时将转义后的字符append到string_buf_上。

碰到\表明是ignore，进入\<IGNORE\>，在ignore中碰到空白字符就跳过，碰到\就返回STR，碰到其他字符就报错。

在string中碰到其他字符就append到string_buf_上，碰到EOF则说明引号不配对。

**Error handling:**

在不应该的时候碰到EOF或其他非法的字符时，通过errormsg_->Error() 报告错误的位置和相应信息。adjust()同样会被执行，以跳过出错的部分并继续向下parse。

**end-of-file handling:**

在\<COMMENT\>, \<STR\>, \<IGNORE\>状态下均有可能碰到EOF，这表明还没有碰到标志该状态结束的字符文件就已经结束了，即 /**/, "", \\\\号不配对，此时则通过errormsg_->Error() 报告错误的位置和相应信息。

