%filenames = "scanner"

 /*
  * Please don't modify the lines above.
  */

 /* You can add lex definitions here. */
 /* TODO: Put your lab2 code here */

%x COMMENT STR IGNORE

%%

 /* reserved words */
"while" {adjust(); return Parser::WHILE;}
"for" {adjust(); return Parser::FOR;}
"to" {adjust(); return Parser::TO;}
"break" {adjust(); return Parser::BREAK;}
"let" {adjust(); return Parser::LET;}
"in" {adjust(); return Parser::IN;}
"end" {adjust(); return Parser::END;}
"function" {adjust(); return Parser::FUNCTION;}
"var" {adjust(); return Parser::VAR;}
"type" {adjust(); return Parser::TYPE;}
"array" {adjust(); return Parser::ARRAY;}
"if" {adjust(); return Parser::IF;}
"then" {adjust(); return Parser::THEN;}
"else" {adjust(); return Parser::ELSE;}
"do" {adjust(); return Parser::DO;}
"of" {adjust(); return Parser::OF;}
"nil" {adjust(); return Parser::NIL;}

 /* TODO: Put your lab2 code here */

[[:alpha:]][[:alnum:]_]* {adjust(); return Parser::ID;}
[[:digit:]]+ {adjust(); return Parser::INT;}
"," {adjust(); return Parser::COMMA;}
":" {adjust(); return Parser::COLON;}
";" {adjust(); return Parser::SEMICOLON;}
"(" {adjust(); return Parser::LPAREN;}
")" {adjust(); return Parser::RPAREN;}
"[" {adjust(); return Parser::LBRACK;}
"]" {adjust(); return Parser::RBRACK;}
"{" {adjust(); return Parser::LBRACE;}
"}" {adjust(); return Parser::RBRACE;}
"." {adjust(); return Parser::DOT;}
"+" {adjust(); return Parser::PLUS;}
"-" {adjust(); return Parser::MINUS;}
"*" {adjust(); return Parser::TIMES;}
"/" {adjust(); return Parser::DIVIDE;}
"=" {adjust(); return Parser::EQ;}
"<>" {adjust(); return Parser::NEQ;}
"<" {adjust(); return Parser::LT;}
"<=" {adjust(); return Parser::LE;}
">" {adjust(); return Parser::GT;}
">=" {adjust(); return Parser::GE;}
"&" {adjust(); return Parser::AND;}
"|" {adjust(); return Parser::OR;}
":=" {adjust(); return Parser::ASSIGN;}


 /* handle comments */
"/*" {adjust(); comment_level_ = 1; begin(StartCondition__::COMMENT);}

<COMMENT> {
  "/*" {adjust(); comment_level_++;}
  "*/" {
    adjust();
    if (!(--comment_level_)) {
      begin(StartCondition__::INITIAL);
    }
  }
  .|\n {adjustStr();}
  <<EOF>> {adjust(); errormsg_->Error(errormsg_->tok_pos_, "comments marks mismatch");}
}



 /* handle strings */
\" {adjust(); string_buf_.clear(); begin(StartCondition__::STR);}

<STR> {
  \" {adjustStr(); begin(StartCondition__::INITIAL); setMatched(string_buf_); return Parser::STRING;}
  R"(\n)" {adjustStr(); string_buf_ += '\n';}
  R"(\t)" {adjustStr(); string_buf_ += '\t';}
  \\\^[A-Z] {adjustStr(); string_buf_ += matched()[2] - 'A' + 1;}
  \\[[:digit:]]{3} {adjustStr(); string_buf_ += (char)stoi(matched().substr(1));}
  R"(\")" {adjustStr(); string_buf_ += '\"';}
  R"(\\)" {adjustStr(); string_buf_ += '\\';}
  \\ {adjustStr(); begin(StartCondition__::IGNORE);}
  . {adjustStr(); string_buf_ += matched();}
  <<EOF>> {adjust(); errormsg_->Error(errormsg_->tok_pos_, "quotation marks mismatch   " + string_buf_);}
}

<IGNORE> {
  \\ {adjustStr(); begin(StartCondition__::STR);}
  [ \n\t\r\f\v] {adjustStr();}
  . {adjust(); errormsg_->Error(errormsg_->tok_pos_, "invalid character in ignore");}
  <<EOF>> {adjust(); errormsg_->Error(errormsg_->tok_pos_, "ignore marks mismatch   " + string_buf_);}
}

 /*
  * skip white space chars.
  * space, tabs and LF
  */
[ \t]+ {adjust();}
\n {adjust(); errormsg_->Newline();}

 /* illegal input */
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
