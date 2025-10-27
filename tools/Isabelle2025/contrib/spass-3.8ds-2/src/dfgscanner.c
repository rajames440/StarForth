#include "dfgscanner.h"

DFG_TOKEN nextTokenorWS(DFG_LEXER lex){
  char* buffer = lex->buffer;
  int buff_pos=-1;
  DFG_TOKEN token;
  while(NEXTCHAR == EOF || CURRCHAR == '%'){
    if(CURRCHAR == '%'){
      buff_pos--;
      eraseCommentline(lex);
    }else{
      return createToken(DFG_FileEnd,lex->line,lex->pos,createText("$EOF$",5));
    }
  }
  if(CURRCHAR==' ' || CURRCHAR=='\t' || CURRCHAR=='\x0b' || CURRCHAR=='\f' || CURRCHAR=='\r'){
    token = createToken(DFG_WhiteSpace,lex->pos + 1, lex->line, createText(" ",1));
    lex->pos = lex->pos + 1;
  } else if( CURRCHAR == '\n'){
    token = createToken(DFG_NextLine, 0, lex->line+1,createText(" ",1));
    lex->pos = 0;
    lex->line = lex->line + 1;
  } else{
    UNGETCHAR;
    token = helpnextToken(lex);
  }
  return token;
}

DFG_TOKEN nextToken(DFG_LEXER lex){
  char* buffer = lex->buffer;
  int buff_pos=-1;
  DFG_TOKEN token;

  while(1){
    switch(NEXTCHAR){
    case '\n':
      lex->line = lex->line + 1;
      lex->pos = 0;
      buff_pos--;
      continue;
    case ' ':
      lex->pos = lex->pos + 1;
      buff_pos--;
      continue;
    case '%':
      buff_pos--;
      eraseCommentline(lex);
      continue;
    case EOF:
      return createToken(DFG_FileEnd,lex->line,lex->pos,createText("$EOF$",5));
    case '\t':
    case '\x0b':
    case '\f':
    case '\r':
      buff_pos--;
      continue;
    default:
      UNGETCHAR;
    }
    break;
  }
  token = helpnextToken(lex);
  return token;
}

char* createText (const char* in , int length){
  char* text = (char*)memory_Malloc(sizeof(char)*(length+1));
  strcpy(text,in);
  return text;
}

DFG_TOKEN helpnextToken(DFG_LEXER lex){
  FILE* input = lex->input;
  char* buffer = lex->buffer;
  DFG_TOKEN token;
  int line = lex->line;
  int pos = lex-> pos;
  int buff_pos=-1;
  
  
  switch(NEXTCHAR){
  case EOF:
    token = createToken(DFG_FileEnd,line,pos,createText("$EOF$",5));
    break;
  case '.':
    token = createToken(DFG_POINT,line,pos,createText("$.$",buff_pos+3));
    break;
  case ',':
    token = createToken(DFG_COMMA,line,pos,createText("$,$",buff_pos+3));
    break;
  case '(':
    token = createToken(DFG_OPENBR,line,pos,createText("$($",buff_pos+3));
    break;
  case ')':
    token = createToken(DFG_CLOSEBR,line,pos,createText("$)$",buff_pos+3));	
    break; 
  case '[':
    token = createToken(DFG_OPENEBR,line,pos,createText("$[$",buff_pos+3));	
    break;
  case ']':
    token = createToken(DFG_CLOSEEBR,line,pos,createText("$]$",buff_pos+3));
    break;
  case '<':
    token = createToken(DFG_OPENPBR,line,pos,createText("<",buff_pos+1));
    break;
  case '>':
    token = createToken(DFG_CLOSEPBR,line,pos,createText(">",buff_pos+1));
    break;
  case '-':
    if(NEXTCHAR=='>'){
      token = createToken(DFG_ARROW,line,pos,createText("$->$",buff_pos+3));
    }else{
      UNGETCHAR;
      token = createToken(DFG_UNARY_MINUS,line,pos,createText("$-$",buff_pos+3));
    }
    break;
  case '+':
    token = createToken(DFG_SYMB_PLUS,line,pos,createText("$+$",buff_pos+3));
    break;
  case ':':
    token = createToken(DFG_COLON,line,pos,createText("$:$",buff_pos+3));
    break;
  case '|':
    if(NEXTCHAR=='|'){
      token = createToken(DFG_ARROW_DOUBLELINE,line,pos,createText("$||$",buff_pos+3));
    }else{
      //TO_DO error
    }	
    break;
  case '3':
    if(NEXTCHAR=='T'){
      if(NEXTCHAR=='A'){
	if(NEXTCHAR=='P'){
	  if(NEXTNOIDENT){
	    UNGETCHAR;
	    token = createToken(DFG_3TAP,line,pos,createText("$3TAP$",buff_pos+3));
	  }else{
	    UNGETCHAR;
            UNGETCHAR;
	    UNGETCHAR;
	    UNGETCHAR;
	    UNGETCHAR;
	    token = getNumber(lex,&buff_pos);
	  }
        }else{
	  UNGETCHAR;
          UNGETCHAR;
	  UNGETCHAR;
	  UNGETCHAR;
	  token = getNumber(lex,&buff_pos);
        }
      }else{
	UNGETCHAR;
        UNGETCHAR;
	UNGETCHAR;
	token = getNumber(lex,&buff_pos);
      }
    }else{
      UNGETCHAR;
      UNGETCHAR;
      token = getNumber(lex,&buff_pos);
    }	
    break;
  default:
    if(CURRCHAR >= '0' && CURRCHAR <= '9'){
      UNGETCHAR;
      token = getNumber(lex,&buff_pos);
    }else if((CURRCHAR >= 'a'&& CURRCHAR <= 'z') ||
	     (CURRCHAR >= 'A' && CURRCHAR <= 'Z' ) ){
	
      if((CURRCHAR >= 'a' && CURRCHAR <= 'z') || (CURRCHAR >= 'A' && CURRCHAR <= 'Z' ) ){
	UNGETCHAR;
	token = getKeyword(lex,&buff_pos);
      }else{
	UNGETCHAR;
	token = getIdentifier(lex,&buff_pos);
      }
    }else if(CURRCHAR == '{' && NEXTCHAR == '*'){
      if((lex->ignoreText) == 0){
	lex->pos = lex->pos + buff_pos +1;
        restoreLex(lex, buff_pos);
	token = nextToken(lex);
	return token;
      }
      buff_pos=-1;
      token = getText(lex, &buff_pos);
    }else if(CURRCHAR == '*' && NEXTCHAR == '}'){
      if(!(lex->ignoreText)){
	lex->pos = lex->pos + buff_pos +1;
        restoreLex(lex, buff_pos);
	return nextToken(lex);
      }else{
	//ERROR
      }
    }else{
      //Error
    }
  }
  lex->pos = lex->pos + buff_pos +1;
  restoreLex(lex, buff_pos);
  return token;
}


DFG_TOKEN createToken(dfgtokentype type, int line, int pos, char* text){
  DFG_TOKEN Result = (DFG_TOKEN)memory_Malloc(sizeof(DFG_TOKEN_NODE));
  Result->type=type;
  Result->line=line;
  Result->pos=pos;
  Result->text=text;
  return Result;
}


DFG_LEXER createLexer(FILE* file){
  DFG_LEXER Result = (DFG_LEXER)memory_Malloc(sizeof(DFG_LEXER_NODE)); 
  Result->input = file;
  setlinebuf (Result->input);
  Result->line = 0;
  Result->pos = 0;
  Result->buffer=(char*)memory_Malloc(sizeof(char)*(DFG_BUFFER_SIZE + 1));
  Result->buffered = 0;
  Result->ignoreText = 1;
  Result->ignoreWS = 1;
  return Result;
}

void restoreLex(DFG_LEXER lex,int buff_pos){  
  int restore = lex->buffered;
  while(restore > 0){
    lex->buffer[restore-1]=lex->buffer[buff_pos+restore];
    restore--;
  }
}

void freeToken(DFG_TOKEN tok){
  string_StringFree(tok->text);
  memory_Free(tok,sizeof(DFG_TOKEN_NODE));
}


void freeLexer(DFG_LEXER lex){
  memory_Free(lex->buffer,sizeof(char)*(DFG_BUFFER_SIZE + 1));
  memory_Free(lex,sizeof(DFG_LEXER_NODE));
}

int getNextChar(DFG_LEXER lex, int* pos){
  if((lex->buffered)>0){
    (lex->buffered)--;
    return (lex->buffer)[++(*pos)];
  }
  int next = fgetc(lex->input);
  if(next!=EOF){
    lex->buffer[++(*pos)] = (char)next;
  }
  return next;
}

void ungetChar(DFG_LEXER lex,int* pos){
  (*pos)--;
  (lex->buffered)++;
}

void eraseCommentline(DFG_LEXER lex){
  int ch = getc(lex->input);
  while(ch!=EOF && ch!='\n'){
    ch = getc(lex->input);
  }
  lex->line = lex->line + 1;
  lex->pos = 0;
}

DFG_TOKEN getNumber(DFG_LEXER lex,int* pbuff_pos){
  int buff_pos = *pbuff_pos;
  char* buffer = lex->buffer;
  char* intermediate;
  
  int resize = 1;

  while(NEXTCHAR != EOF){

    if((CURRCHAR < '0' || CURRCHAR > '9')){
      UNGETCHAR;
      break;
    }

    if(buff_pos >= (resize * DFG_BUFFER_SIZE) - 1){
      intermediate = memory_Malloc(sizeof(char) * ((++resize) * DFG_BUFFER_SIZE + 1) );
      strncpy(intermediate,buffer,buff_pos+1);

      memory_Free(buffer,sizeof(char) * ((resize-1) * DFG_BUFFER_SIZE + 1)) ;

      lex->buffer = intermediate;
      buffer = intermediate;
    }
    
  }

  char* text = (char*)memory_Malloc(sizeof(char)*(buff_pos+2));
  strncpy(text,buffer,buff_pos+1);
  
  text[buff_pos+1]='\0';
  
  restoreLex(lex,buff_pos);
  intermediate =  memory_Malloc(sizeof(char) * (DFG_BUFFER_SIZE + 1) );
  strncpy(intermediate,buffer,(DFG_BUFFER_SIZE + 1));
  lex->buffer=intermediate;
  memory_Free(buffer,sizeof(char) * ((resize) * DFG_BUFFER_SIZE + 1));
  lex->pos+=(buff_pos+1);
  *pbuff_pos=-1; 
  
  return (createToken(DFG_NUMBER,lex->line,lex->pos - buff_pos -1,text));
}

DFG_TOKEN getText(DFG_LEXER lex,int* pbuff_pos){  
  int line = lex->line;
  int pos = lex->pos;
  

  int buff_pos = *pbuff_pos;
  char* buffer = lex->buffer;
  char* intermediate;
  
  int resize = 1;

  while(1){
    if(NEXTCHAR == EOF){
      //ERROR
    }else if(CURRCHAR == '\n'){
      (lex->line)++;
      lex->pos=0;
    }
    lex->pos++;

    if(buff_pos >= (resize * DFG_BUFFER_SIZE) - 1){
      intermediate = memory_Malloc(sizeof(char) * ((++resize) * DFG_BUFFER_SIZE + 1) );
      strncpy(intermediate,buffer,buff_pos+1);
      
      memory_Free(buffer,sizeof(char) * ((resize-1) * DFG_BUFFER_SIZE + 1));
      
      lex->buffer = intermediate;
      buffer = intermediate;
    }
    
    if(CURRCHAR == '*'){
      if(NEXTCHAR == '}'){
	buffer[buff_pos-1]='\0';
	break;
      }
      UNGETCHAR;
    }

  }
  char* text = (char*)memory_Malloc(sizeof(char)*(buff_pos));
  strncpy(text,buffer,(buff_pos));
  
  restoreLex(lex,buff_pos);
  intermediate =  memory_Malloc(sizeof(char) * (DFG_BUFFER_SIZE + 1) );
  strncpy(intermediate,buffer,(DFG_BUFFER_SIZE + 1));
  lex->buffer=intermediate;
  memory_Free(buffer,sizeof(char) * ((resize) * DFG_BUFFER_SIZE + 1));
  
  lex->pos += 2; //to outbalance the overjumped symbols "*}"
  
  *pbuff_pos=-1;
  
  return (createToken(DFG_TEXT,line,pos,text));
}

DFG_TOKEN getIdentifier(DFG_LEXER lex,int* pbuff_pos){
  int buff_pos = *pbuff_pos;
  char* buffer = lex->buffer;
  char* intermediate;
  
  int resize = 1;

  while(NEXTCHAR != EOF){

    if((CURRCHAR < 'a' || CURRCHAR > 'z') && (CURRCHAR < 'A' || CURRCHAR > 'Z') &&
       (CURRCHAR < '0' || CURRCHAR > '9') && CURRCHAR != '_'){
      UNGETCHAR;
      break;
    }

    if(buff_pos >= (resize * DFG_BUFFER_SIZE) - 1){
      intermediate = (char*)memory_Malloc(sizeof(char) * ((++resize) * DFG_BUFFER_SIZE + 1) );
      
      strncpy(intermediate,buffer,buff_pos+1);

      memory_Free(buffer,sizeof(char) * ((resize-1) * DFG_BUFFER_SIZE + 1));

      lex->buffer = intermediate;
      buffer = intermediate;
    }
    
  }

  char* text = (char*)memory_Malloc(sizeof(char)*(buff_pos+2));
  
  
  strncpy(text,buffer,buff_pos+1);
  
  text[buff_pos+1]='\0';
  
  restoreLex(lex,buff_pos);
  
  intermediate =  (char*) memory_Malloc(sizeof(char) * (DFG_BUFFER_SIZE + 1) );
  strncpy(intermediate,buffer,(DFG_BUFFER_SIZE + 1));
  
  lex->buffer=intermediate;
  memory_Free(buffer,sizeof(char) * ((resize) * DFG_BUFFER_SIZE + 1));
  lex->pos+=(buff_pos+1);
  *pbuff_pos=-1;  
  return (createToken(DFG_IDENTIFIER,lex->line,lex->pos - buff_pos -1,text));
}

DFG_TOKEN getKeyword(DFG_LEXER lex,int* pbuff_pos){
  char* buffer = lex->buffer;
  int buff_pos = *pbuff_pos;
  lexerstate state = LEX_start;
  DFG_TOKEN Return;
  while(state!=LEX_END){
    switch(state){
    case LEX_start :
      switch(NEXTCHAR){
      case 'a':
	state = LEX_a_;
	break;
      case 'b':
	state = LEX_b_;
	break;
      case 'c':
	state = LEX_c_;
	break;
      case 'd':
	state = LEX_d_;
	break;
      case 'e':
	state = LEX_e_;
	break;
      case 'f':
	state = LEX_f_;
	break;
      case 'g':
	state = LEX_g_;
	break;
      case 'i':
	state = LEX_i_;
	break;
      case 'h':
	state = LEX_hypothesis;
	break;
      case 'l':
	state = LEX_l_;
	break;
      case 'm':
	state = LEX_m_;
	break;
      case 'n':
	state = LEX_n_;
	break;
      case 'o':
	state = LEX_or;
	break;
      case 'p':
	state = LEX_p_;
	break;
      case 'r':
	state = LEX_r_;
	break;
      case 's':
	state = LEX_s_;
	break;
      case 't':
	state = LEX_t_;
	break;
      case 'u':
	state = LEX_un_;
	break;
      case 'v':
	state = LEX_version;
	break;
      case 'w':
	state = LEX_weights;
	break;
      case 'A':
	state = LEX_A_;
	break;
      case 'C':
	state = LEX_C_;
	break;
      case 'D':
	state = LEX_Def;
	break;
      case 'E':
	state = LEX_E_;
	break;
      case 'F':
	state = LEX_Fac;
	break;
      case 'I':
	state = LEX_In_;
	break;
      case 'K':
	state = LEX_KIV;
	break;
      case 'L':
	state = LEX_LEM;
	break;
      case 'M':
	state = LEX_M_;
	break;
      case 'N':
	state = LEX_Natural;
	break;
      case 'O':
	state = LEX_O_;
	break;
      case 'P':
	state = LEX_PROTEIN;
	break;
      case 'R':
	state = LEX_R_;
	break;
      case 'S':
	state = LEX_S_;
	break;
      case 'T':
	state = LEX_T_;
	break;
      case 'U':
	state = LEX_U_;
	break;
      default:
	state = LEX_identifier;

      }
      break;
    case LEX_a_ :
      switch(NEXTCHAR){
      case 'l':
	state = LEX_all;
	break;
      case 'n':
	state = LEX_and;
	break;
      case 'u':
	state = LEX_author;
	break;
      case 'x':
	state = LEX_axioms;
	break;
      default:
	state = LEX_identifier;
      }
      break;
    case LEX_all :
      if(NEXTCHAR == 'l' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_all ,lex->line ,lex->pos,createText("$all$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_and :
      if(NEXTCHAR == 'd' &&
         NEXTNOIDENT ){
        UNGETCHAR;
	Return = createToken( DFG_and ,lex->line ,lex->pos,createText("$and$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_author :
      if(NEXTCHAR == 't' &&
         NEXTCHAR == 'h' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'r' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_author ,lex->line ,lex->pos,createText("$author$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_axioms :
      if(NEXTCHAR == 'i' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'm' &&
         NEXTCHAR == 's' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_axioms ,lex->line ,lex->pos,createText("$axioms$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;


    case LEX_b_ :
      switch(NEXTCHAR){
      case 'e':
	state = LEX_begin_problem;
	break;
      case 'o':
	state = LEX_box;
	break;
      default:
	state = LEX_identifier;
      }
      break;

    case LEX_begin_problem :
      if(NEXTCHAR == 'g' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'n' &&
         NEXTCHAR == '_' &&
         NEXTCHAR == 'p' &&
         NEXTCHAR == 'r' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'b' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == 'm' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_begin_problem ,lex->line ,lex->pos,createText("$begin_problem$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_box :
      if(NEXTCHAR == 'x' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_box ,lex->line ,lex->pos,createText("$box$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;


    case LEX_c_ :
      switch(NEXTCHAR){
      case 'l':
	state = LEX_clause;
	break;
      case 'n':
	state = LEX_cnf;
	break;
      case 'o':
	state = LEX_co_;
	break;
      default:
	state = LEX_identifier;
      }
      break;

    case LEX_clause :
      if(NEXTCHAR == 'a' &&
         NEXTCHAR == 'u' &&
         NEXTCHAR == 's' &&
         NEXTCHAR == 'e' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_clause ,lex->line ,lex->pos,createText("$clause$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_cnf :
      if(NEXTCHAR == 'f' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_cnf ,lex->line ,lex->pos,createText("$cnf$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
      
    case LEX_co_ :
      switch(NEXTCHAR){
      case 'm':
	state = LEX_comp;
	break;
      case 'n':
	state = LEX_con_;
	break;
      default:
	state = LEX_identifier;
      }
      break;
    case LEX_comp :
      if(NEXTCHAR == 'p' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_comp ,lex->line ,lex->pos,createText("$comp$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_con_ :
      switch(NEXTCHAR){
      case 'c':
	state = LEX_concept_formula;
	break;
      case 'j':
	state = LEX_conjectures;
	break;
      case 'v':
	state = LEX_conv;
	break;
      default:
	state = LEX_identifier;
      }
      break;

    case LEX_concept_formula :
      if(NEXTCHAR == 'e' &&
         NEXTCHAR == 'p' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == '_' &&
         NEXTCHAR == 'f' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'r' &&
         NEXTCHAR == 'm' &&
         NEXTCHAR == 'u' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'a' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_concept_formula ,lex->line ,lex->pos,createText("$concept_formula$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_conjectures :
      if(NEXTCHAR == 'e' &&
         NEXTCHAR == 'c' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == 'u' &&
         NEXTCHAR == 'r' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == 's' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_conjectures ,lex->line ,lex->pos,createText("$conjectures$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_conv :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_conv ,lex->line ,lex->pos,createText("$conv$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;


    case LEX_d_ :
      switch(NEXTCHAR){
      case 'a':
	state = LEX_dat_;
	break;
      case 'e':
	state = LEX_de_;
	break;
      case 'i':
	state = LEX_di_;
	break;
      case 'l':
	state = LEX_dl;
	break;
      case 'o':
	state = LEX_dom_;
	break;
      default:
	state = LEX_identifier;
      }
      break;

    case LEX_dat_ :
      if(NEXTCHAR == 't'){
	switch(NEXTCHAR){
	case 'a':
	  state = LEX_datatype;
	  break;
	case 'e':
	  state = LEX_date;
	  break;
	default:
	  state = LEX_identifier;
	}
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_datatype :
      if(NEXTCHAR == 't' &&
         NEXTCHAR == 'y' &&
         NEXTCHAR == 'p' &&
         NEXTCHAR == 'e' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_datatype ,lex->line ,lex->pos,createText("$datatype$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_date :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_date ,lex->line ,lex->pos,createText("$date$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_de_ :
      switch(NEXTCHAR){
      case 'f':
	state = LEX_def;
	break;
      case 's':
	state = LEX_description;
	break;
      default:
	state = LEX_identifier;
      }
      break;

    case LEX_def :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_def ,lex->line ,lex->pos,createText("$def$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_description :
      if(NEXTCHAR == 'c' &&
         NEXTCHAR == 'r' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'p' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'n' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_description ,lex->line ,lex->pos,createText("$description$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_di_ :
      switch(NEXTCHAR){
      case 'a':
	state = LEX_dia;
	break;
      case 's':
	state = LEX_distinct_symbols;
	break;
      case 'v':
	state = LEX_div;
	break;
      default:
	state = LEX_identifier;
      }
      break;

    case LEX_dia :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_dia ,lex->line ,lex->pos,createText("$dia$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_distinct_symbols :
      if(NEXTCHAR == 't' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'n' &&
         NEXTCHAR == 'c' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == '_' &&
         NEXTCHAR == 's' &&
         NEXTCHAR == 'y' &&
         NEXTCHAR == 'm' &&
         NEXTCHAR == 'b' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 's' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_distinct_symbols ,lex->line ,lex->pos,createText("$distinct_symbols$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
      
    case LEX_div :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_div ,lex->line ,lex->pos,createText("$div$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_dl :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_dl ,lex->line ,lex->pos,createText("$dl$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_dom_ :
      if(NEXTCHAR == 'm'){
	switch(NEXTCHAR){
	case 'a':
	  state = LEX_domain;
	  break;
	case 'r':
	  state = LEX_domrestr;
	  break;
	default:
	  state = LEX_identifier;
	}
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_domain :
      if(NEXTCHAR == 'i' &&
         NEXTCHAR == 'n' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_domain ,lex->line ,lex->pos,createText("$domain$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_domrestr :
      if(NEXTCHAR == 'e' &&
         NEXTCHAR == 's' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == 'r' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_domrestr ,lex->line ,lex->pos,createText("$domrestr$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;


    case LEX_e_ :
      switch(NEXTCHAR){
      case 'm':
	state = LEX_eml;
	break;
      case 'n':
	state = LEX_end__;
	break;	
      case 'q':
	state = LEX_equ_;
	break;
      case 'x':
	state = LEX_exists;
	break;  
      default:
	state = LEX_identifier;
      }
      break;

    case LEX_eml :
      if(NEXTCHAR == 'l' &&
	 NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_eml ,lex->line ,lex->pos,createText("$eml$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_end__ :
      if(NEXTCHAR == 'd' &&
         NEXTCHAR == '_' ){
	switch(NEXTCHAR){
	case 'o':
	  state = LEX_end_of_list;
	  break;
	case 'p':
	  state = LEX_end_problem;
	  break;
	default:
	  state = LEX_identifier;
	}
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_end_of_list :
      if(NEXTCHAR == 'f' &&
         NEXTCHAR == '_' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 's' &&
         NEXTCHAR == 't' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_end_of_list ,lex->line ,lex->pos,createText("$end_of_list$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_end_problem :
      if(NEXTCHAR == 'r' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'b' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == 'm' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_end_problem ,lex->line ,lex->pos,createText("$end_problem$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_equ_ :
      if(NEXTCHAR == 'u' ){
	switch(NEXTCHAR){
	case 'a':
	  state = LEX_equal;
	  break;
	case 'i':
	  state = LEX_equiv;
	  break;
	default:
	  state = LEX_identifier;
	}
      } else{
        state=LEX_identifier;
      }
      break;


    case LEX_equal :
      if(NEXTCHAR == 'l' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_equal ,lex->line ,lex->pos,createText("$equal$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_equiv :
      if(NEXTCHAR == 'v' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_equiv ,lex->line ,lex->pos,createText("$equiv$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_exists :
      if(NEXTCHAR == 'i' &&
         NEXTCHAR == 's' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == 's' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_exists ,lex->line ,lex->pos,createText("$exists$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;


    case LEX_f_ :
      switch(NEXTCHAR){
      case 'a':
	state = LEX_false;
	break;
      case 'o':
	state = LEX_for_;
	break;	
      case 'r':
	state = LEX_fract;
	break;
      case 'u':
	state = LEX_function;
	break;  
      default:
	state = LEX_identifier;
      }
      break;

    case LEX_false :
      if(NEXTCHAR == 'l' &&
	 NEXTCHAR == 's' &&
	 NEXTCHAR == 'e' &&
	 NEXTNOIDENT ){
	UNGETCHAR;
	Return = createToken( DFG_false ,lex->line ,lex->pos,createText("$false$",buff_pos+3));
	state = LEX_END;
      } else{
	state=LEX_identifier;
      }
      break;

    case LEX_for_ :
      if(NEXTCHAR == 'r'){
	switch(NEXTCHAR){
	case 'a':
	  state = LEX_forall;
	  break;
	case 'm':
	  state = LEX_formula;
	  break;
	default:
	  state = LEX_identifier;
	}
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_forall :
      if(NEXTCHAR == 'l' &&
         NEXTCHAR == 'l' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_forall ,lex->line ,lex->pos,createText("$forall$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_formula :
      if(NEXTCHAR == 'u' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'a' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_formula ,lex->line ,lex->pos,createText("$formula$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_fract :
      if(NEXTCHAR == 'a' &&
         NEXTCHAR == 'c' &&
         NEXTCHAR == 't' &&
         NEXTNOIDENT ){
        UNGETCHAR;
	//Return = createToken( DFG_fract ,lex->line ,lex->pos,createText("$fract$",buff_pos+3));
        Return = createToken( DFG_IDENTIFIER ,lex->line ,lex->pos,createText("fract",buff_pos+1));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_function :
      if(NEXTCHAR == 'n' &&
         NEXTCHAR == 'c' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'n' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_function ,lex->line ,lex->pos,createText("$function$",buff_pos+3));
        state = LEX_END;
      } else{
	UNGETCHAR;
        state=LEX_functions;
      }
      break;

    case LEX_functions :
      if(NEXTCHAR == 's' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_functions ,lex->line ,lex->pos,createText("$functions$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;


    case LEX_g_ :
      switch(NEXTCHAR){
      case 'e':
	state = LEX_ge;
	break;
      case 's':
	state = LEX_gs;
	break;	
      default:
	state = LEX_identifier;
      }
      break;

    case LEX_ge :
      if(NEXTNOIDENT ){
	UNGETCHAR;
	Return = createToken( DFG_ge ,lex->line ,lex->pos,createText("$ge$",buff_pos+3));
	state = LEX_END;
      } else{
	state=LEX_identifier;
      }
      break;

    case LEX_gs :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_gs ,lex->line ,lex->pos,createText("$gs$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

      
    case LEX_i_ :
      switch(NEXTCHAR){
      case 'd':
	state = LEX_id;
	break;
      case 'm':
	state = LEX_implie_;
	break;	
      case 'n':
	state = LEX_include;
	break;
      default:
	state = LEX_identifier;
      }
      break;

    
    case LEX_id :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_id ,lex->line ,lex->pos,createText("$id$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
    
      
    case LEX_implie_ :
      if(NEXTCHAR == 'p' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'e' ){
	switch(NEXTCHAR){
	case 'd':
	  state = LEX_implied;
	  break;
	case 's':
	  state = LEX_implies;
	  break;
	default:
	  state = LEX_identifier;
	}
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_implied :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_implied ,lex->line ,lex->pos,createText("$implied$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_implies :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_implies ,lex->line ,lex->pos,createText("$implies$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_include :
      if(NEXTCHAR == 'c' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'u' &&
         NEXTCHAR == 'd' &&
         NEXTCHAR == 'e' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_include ,lex->line ,lex->pos,createText("$include$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
      
      
      
    case LEX_hypothesis :
      if(NEXTCHAR == 'y' &&
         NEXTCHAR == 'p' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == 'h' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == 's' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 's' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_hypothesis ,lex->line ,lex->pos,createText("$hypothesis$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
      
      
      
    case LEX_l_ :
      switch(NEXTCHAR){
      case 'e':
	state = LEX_le;
	break;
      case 'i':
	state = LEX_list_of__;
	break;
      case 'o':
	state = LEX_logic;
	break;
      case 'r':
	state = LEX_lr;
	break;
      case 's':
	state = LEX_ls;
	break;	
      case 't':
	state = LEX_lt;
	break;
      default:
	state = LEX_identifier;
      }
      break;

    case LEX_le :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_le ,lex->line ,lex->pos,createText("$le$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_list_of__ :
      if(NEXTCHAR == 's' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == '_' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'f' &&
         NEXTCHAR == '_' ){
	switch(NEXTCHAR){
	case 'c':
	  state = LEX_list_of_clauses;
	  break;
	case 'd':
	  state = LEX_list_of_de_;
	  break;
	case 'f':
	  state = LEX_list_of_formulae;
	  break;
	case 'g':
	  state = LEX_list_of_general_settings;
	  break;
	case 'i':
	  state = LEX_list_of_includes;
	  break;
	case 'p':
	  state = LEX_list_of_proof;
	  break;
	case 's':
	  state = LEX_list_of_s_;
	  break;
	default:
	  state = LEX_identifier;
	}
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_list_of_clauses :
      if(NEXTCHAR == 'l' &&
         NEXTCHAR == 'a' &&
         NEXTCHAR == 'u' &&
         NEXTCHAR == 's' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == 's' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_list_of_clauses ,lex->line ,lex->pos,createText("$list_of_clauses$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_list_of_de_ :
      if(NEXTCHAR == 'e'){
	switch(NEXTCHAR){
	case 'c':
	  state = LEX_list_of_declarations;
	  break;
	case 's':
	  state = LEX_list_of_descriptions;
	  break;
	default:
	  state = LEX_identifier;
	}
      } else{
        state=LEX_identifier;
      }
      break;


    case LEX_list_of_declarations :
      if(NEXTCHAR == 'l' &&
         NEXTCHAR == 'a' &&
         NEXTCHAR == 'r' &&
         NEXTCHAR == 'a' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'n' &&
         NEXTCHAR == 's' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_list_of_declarations ,lex->line ,lex->pos,createText("$list_of_declarations$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_list_of_descriptions :
      if(NEXTCHAR == 'c' &&
         NEXTCHAR == 'r' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'p' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'n' &&
         NEXTCHAR == 's' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_list_of_descriptions ,lex->line ,lex->pos,createText("$list_of_descriptions$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_list_of_formulae :
      if(NEXTCHAR == 'o' &&
         NEXTCHAR == 'r' &&
         NEXTCHAR == 'm' &&
         NEXTCHAR == 'u' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'a' &&
         NEXTCHAR == 'e' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_list_of_formulae ,lex->line ,lex->pos,createText("$list_of_formulae$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_list_of_general_settings :
      if(NEXTCHAR == 'e' &&
         NEXTCHAR == 'n' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == 'r' &&
         NEXTCHAR == 'a' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == '_' &&
         NEXTCHAR == 's' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'n' &&
         NEXTCHAR == 'g' &&
         NEXTCHAR == 's' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_list_of_general_settings ,lex->line ,lex->pos,createText("$list_of_general_settings$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
      
    case LEX_list_of_includes :
      if(NEXTCHAR == 'n' &&
         NEXTCHAR == 'c' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'u' &&
         NEXTCHAR == 'd' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == 's' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_list_of_includes ,lex->line ,lex->pos,createText("$list_of_includes$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
      
    case LEX_list_of_proof :
      if(NEXTCHAR == 'r' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'f' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_list_of_proof ,lex->line ,lex->pos,createText("$list_of_proof$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
      
    case LEX_list_of_s_ :
      switch(NEXTCHAR){
	case 'e':
	  state = LEX_list_of_settings;
	  break;
	case 'p':
	  state = LEX_list_of_special_formulae;
	  break;
	case 'y':
	  state = LEX_list_of_symbols;
	  break;
	default:
	  state = LEX_identifier;
      }	  
      break;
    
    case LEX_list_of_settings :
      if(NEXTCHAR == 't' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'n' &&
         NEXTCHAR == 'g' &&
         NEXTCHAR == 's' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_list_of_settings ,lex->line ,lex->pos,createText("$list_of_settings$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
      
    case LEX_list_of_special_formulae :
      if(NEXTCHAR == 'e' &&
         NEXTCHAR == 'c' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'a' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == '_' &&
         NEXTCHAR == 'f' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'r' &&
         NEXTCHAR == 'm' &&
         NEXTCHAR == 'u' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'a' &&
         NEXTCHAR == 'e' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_list_of_special_formulae ,lex->line ,lex->pos,createText("$list_of_special_formulae$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
    
    case LEX_list_of_symbols :
      if(NEXTCHAR == 'm' &&
         NEXTCHAR == 'b' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 's' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_list_of_symbols ,lex->line ,lex->pos,createText("$list_of_symbols$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
      
    case LEX_logic :
      if(NEXTCHAR == 'g' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'c' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_logic ,lex->line ,lex->pos,createText("$logic$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_lr :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_lr ,lex->line ,lex->pos,createText("$lr$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
    case LEX_lt :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_lt ,lex->line ,lex->pos,createText("$lt$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_ls :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_ls ,lex->line ,lex->pos,createText("$ls$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;


    case LEX_m_ :
      switch(NEXTCHAR){
      case 'i':
	state = LEX_minus;
	break;
      case 'u':
	state = LEX_mult;
	break;	
      default:
	state = LEX_identifier;
      }
      break;

    case LEX_minus :
      if(NEXTCHAR == 'n' &&
         NEXTCHAR == 'u' &&
         NEXTCHAR == 's' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        //Return = createToken( DFG_minus ,lex->line ,lex->pos,createText("$minus$",buff_pos+3));
	Return = createToken( DFG_IDENTIFIER ,lex->line ,lex->pos,createText("minus",buff_pos+1));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_mult :
      if(NEXTCHAR == 'l' &&
         NEXTCHAR == 't' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        //Return = createToken( DFG_mult ,lex->line ,lex->pos,createText("$mult$",buff_pos+3));
	Return = createToken( DFG_IDENTIFIER ,lex->line ,lex->pos,createText("mult",buff_pos+1));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;


    case LEX_n_ :
      switch(NEXTCHAR){
      case 'a':
	state = LEX_name;
	break;
      case 'o':
	state = LEX_not;
	break;
      default:
	state = LEX_identifier;
      }
      break;

    case LEX_name :
      if(NEXTCHAR == 'm' &&
         NEXTCHAR == 'e' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_name ,lex->line ,lex->pos,createText("$name$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_not :
      if(NEXTCHAR == 't' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_not ,lex->line ,lex->pos,createText("$not$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;


    case LEX_or :
      if(NEXTCHAR == 'r' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_or ,lex->line ,lex->pos,createText("$or$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
  
    case LEX_p_ :
      switch(NEXTCHAR){
      case 'l':
	state = LEX_plus;
	break;
      case 'r':
	state = LEX_pr_;
	break;	
      default:
	state = LEX_identifier;
      }
      break;

    case LEX_plus :
      if(NEXTCHAR == 'u' &&
         NEXTCHAR == 's' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        //Return = createToken( DFG_plus ,lex->line ,lex->pos,createText("$plus$",buff_pos+3));
	Return = createToken( DFG_IDENTIFIER ,lex->line ,lex->pos,createText("plus",buff_pos+1));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
   
    case LEX_pr_ :
      switch(NEXTCHAR){
      case 'e':
	state = LEX_predicate;
	break;
      case 'o':
	state = LEX_prop_formula;
	break;	
      default:
	state = LEX_identifier;
      }
      break;
    case LEX_predicate :
      if(NEXTCHAR == 'd' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'c' &&
         NEXTCHAR == 'a' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == 'e' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_predicate ,lex->line ,lex->pos,createText("$predicate$",buff_pos+3));
        state = LEX_END;
      } else{
	UNGETCHAR;
        state=LEX_predicates;
      }
      break;

    case LEX_predicates :
      if(NEXTCHAR == 's' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_predicates ,lex->line ,lex->pos,createText("$predicates$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_prop_formula :
      if(NEXTCHAR == 'p' &&
         NEXTCHAR == '_' &&
         NEXTCHAR == 'f' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'r' &&
         NEXTCHAR == 'm' &&
         NEXTCHAR == 'u' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'a' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_prop_formula ,lex->line ,lex->pos,createText("$prop_formula$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;


    case LEX_r_ :
      switch(NEXTCHAR){
      case 'a':
	state = LEX_ran_;
	break;
      case 'e':
	state = LEX_rel_formula;
	break;
      case 'o':
	state = LEX_role_formula;
	break;
      default:
	state = LEX_identifier;
      }
      break;

    case LEX_ran_ :
      if(NEXTCHAR == 'n'){
	switch(NEXTCHAR){
	case 'g':
	  state = LEX_range;
	  break;
	case 'r':
	  state = LEX_ranrester;
	  break;
	default:
	  state = LEX_identifier;
	}
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_range :
      if(NEXTCHAR == 'e' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_range ,lex->line ,lex->pos,createText("$range$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_ranrester :
      if(NEXTCHAR == 'e' &&
         NEXTCHAR == 's' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == 'r' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_ranrester ,lex->line ,lex->pos,createText("$ranrester$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_rel_formula :
      if(NEXTCHAR == 'l' &&
         NEXTCHAR == '_' &&
         NEXTCHAR == 'f' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'r' &&
         NEXTCHAR == 'm' &&
         NEXTCHAR == 'u' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'a' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_rel_formula ,lex->line ,lex->pos,createText("$rel_formula$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_role_formula :
      if(NEXTCHAR == 'l' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == '_' &&
         NEXTCHAR == 'f' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'r' &&
         NEXTCHAR == 'm' &&
         NEXTCHAR == 'u' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'a' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_role_formula ,lex->line ,lex->pos,createText("$role_formula$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

  
    case LEX_s_ :
      switch(NEXTCHAR){
      case 'a':
	state = LEX_satisfiable;
	break;
      case 'e':
	state = LEX_set__;
	break;
      case 'o':
	state = LEX_so_;
	break;
      case 'p':
	state = LEX_splitlevel;
	break;
      case 't':
	state = LEX_st_;
	break;
      case 'u':
	state = LEX_su_;
	break;  
      default:
	state = LEX_identifier;
      }
      break;

    case LEX_satisfiable :
      if(NEXTCHAR == 't' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 's' &&
         NEXTCHAR == 'f' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'a' &&
         NEXTCHAR == 'b' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'e' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_satisfiable ,lex->line ,lex->pos,createText("$satisfiable$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

      
    case LEX_set__ :
      if(NEXTCHAR == 't' &&
         NEXTCHAR == '_' ){
        switch(NEXTCHAR){
	  case 'f':
	    state = LEX_set_flag;
	    break;
	  case 'p':
	    state = LEX_set_precedence;
	    break;
	  case 's':
	    state = LEX_set_selection;
	    break;
	  case 'C':
	    state = LEX_set_ClauseFormulaRelation;
	    break;
	  case 'D':
	    state = LEX_set_DomPred;
	    break;
	  default:
	    state=LEX_identifier;
	}
      } else{
        state=LEX_identifier;
      }
      break;
      
    case LEX_set_flag :
      if(NEXTCHAR == 'l' &&
         NEXTCHAR == 'a' &&
         NEXTCHAR == 'g' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_set_flag ,lex->line ,lex->pos,createText("$set_flag$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
      
    case LEX_set_precedence :
      if(NEXTCHAR == 'r' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == 'c' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == 'd' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == 'n' &&
         NEXTCHAR == 'c' &&
         NEXTCHAR == 'e' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_set_precedence ,lex->line ,lex->pos,createText("$set_precedence$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
      
    case LEX_set_selection :
      if(NEXTCHAR == 'e' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == 'c' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'n' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_set_selection ,lex->line ,lex->pos,createText("$set_selection$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
      
    case LEX_set_ClauseFormulaRelation :
      if(NEXTCHAR == 'l' &&
         NEXTCHAR == 'a' &&
         NEXTCHAR == 'u' &&
         NEXTCHAR == 's' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == 'F' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'r' &&
         NEXTCHAR == 'm' &&
         NEXTCHAR == 'u' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'a' &&
         NEXTCHAR == 'R' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'a' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'n' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_set_ClauseFormulaRelation ,lex->line ,lex->pos,createText("$set_ClauseFormulaRelation$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
      
    case LEX_set_DomPred :
      if(NEXTCHAR == 'o' &&
         NEXTCHAR == 'm' &&
         NEXTCHAR == 'P' &&
         NEXTCHAR == 'r' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == 'd' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_set_DomPred ,lex->line ,lex->pos,createText("$set_DomPred$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
      
      
    case LEX_so_ :
      switch(NEXTCHAR){
      case 'm':
	state = LEX_some;
	break;
      case 'r':
	state = LEX_sorts;
	break;
      default:
	state = LEX_identifier;
      }
      break;

    case LEX_some :
      if(NEXTCHAR == 'e' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_some ,lex->line ,lex->pos,createText("$some$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_sorts :
      if(NEXTCHAR == 't' &&
         NEXTCHAR == 's' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_sorts ,lex->line ,lex->pos,createText("$sorts$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
     
    case LEX_splitlevel :
      if(NEXTCHAR == 'l' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == 'v' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == 'l' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_splitlevel ,lex->line ,lex->pos,createText("$splitlevel$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
      
    case LEX_st_ :
      switch(NEXTCHAR){
      case 'a':
	state = LEX_status;
	break;
      case 'e':
	state = LEX_step;
	break;
      default:
	state = LEX_identifier;
      }
      break;      
      
    case LEX_status :
      if(NEXTCHAR == 't' &&
         NEXTCHAR == 'u' &&
         NEXTCHAR == 's' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_status ,lex->line ,lex->pos,createText("$status$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
      
    case LEX_step :
      if(NEXTCHAR == 'p' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_step ,lex->line ,lex->pos,createText("$step$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_su_ :
      switch(NEXTCHAR){
      case 'b':
	state = LEX_subsort;
	break;
      case 'm':
	state = LEX_sum;
	break;
      default:
	state = LEX_identifier;
      }
      break;

    case LEX_subsort :
      if(NEXTCHAR == 's' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'r' &&
         NEXTCHAR == 't' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_subsort ,lex->line ,lex->pos,createText("$subsort$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_sum :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_sum ,lex->line ,lex->pos,createText("$sum$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;


    case LEX_t_ :
      switch(NEXTCHAR){
      case 'e':
	state = LEX_test;
	break;
      case 'r':
	state = LEX_tr_;
	break;
      default:
	state = LEX_identifier;
      }
      break;

    case LEX_test :
      if(NEXTCHAR == 's' &&
         NEXTCHAR == 't' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_test ,lex->line ,lex->pos,createText("$test$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_tr_ :
      switch(NEXTCHAR){
      case 'a':
	state = LEX_translpairs;
	break;
      case 'u':
	state = LEX_true;
	break;
      default:
	state = LEX_identifier;
      }
      break;

    case LEX_translpairs :
      if(NEXTCHAR == 'n' &&
         NEXTCHAR == 's' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'p' &&
         NEXTCHAR == 'a' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'r' &&
         NEXTCHAR == 's' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_translpairs ,lex->line ,lex->pos,createText("$translpairs$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_true :
      if(NEXTCHAR == 'e' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_true ,lex->line ,lex->pos,createText("$true$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;


    case LEX_un_ :
      if(NEXTCHAR == 'n' ){
	switch(NEXTCHAR){
	case 'k':
	  state = LEX_unknown;
	  break;
	case 's':
	  state = LEX_unsatisfiable;
	  break;
	default:
	  state = LEX_identifier;
	}
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_unknown :
      if(NEXTCHAR == 'n' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'w' &&
         NEXTCHAR == 'n' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_unknown ,lex->line ,lex->pos,createText("$unknown$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_unsatisfiable :
      if(NEXTCHAR == 'a' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 's' &&
         NEXTCHAR == 'f' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'a' &&
         NEXTCHAR == 'b' &&
         NEXTCHAR == 'l' &&
         NEXTCHAR == 'e' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_unsatisfiable ,lex->line ,lex->pos,createText("$unsatisfiable$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;


    case LEX_version :
      if(NEXTCHAR == 'e' &&
         NEXTCHAR == 'r' &&
         NEXTCHAR == 's' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'n' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_version ,lex->line ,lex->pos,createText("$version$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_weights :
    	if(NEXTCHAR == 'e' &&
    	    	   NEXTCHAR == 'i' &&
    	    	   NEXTCHAR == 'g' &&
    	    	   NEXTCHAR == 'h' &&
    	    	   NEXTCHAR == 't' &&
    	    	   NEXTCHAR == 's' &&
    			NEXTNOIDENT ){
    		UNGETCHAR;
    		Return = createToken( DFG_weights ,lex->line ,lex->pos,createText("$weights$",buff_pos+3));
    		state = LEX_END;
    	} else{
    		state=LEX_identifier;
    	}
    	break;

    case LEX_A_ :
      switch(NEXTCHAR){
	case 'p':
	  state = LEX_App;
	  break;
	case 'E':
	  state = LEX_AED;
	  break;
	default:
	  state = LEX_identifier;
	}
      break;

    case LEX_App :
      if(NEXTCHAR == 'p' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_App ,lex->line ,lex->pos,createText("$App$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_AED :
      if(NEXTCHAR == 'D' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_AED ,lex->line ,lex->pos,createText("$AED$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_C_ :
      switch(NEXTCHAR){
	case 'o':
	  state = LEX_Con;
	  break;
	case 'R':
	  state = LEX_CRW;
	  break;
	default:
	  state = LEX_identifier;
	}
      break;

    case LEX_Con :
      if(NEXTCHAR == 'n' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_Con ,lex->line ,lex->pos,createText("$Con$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_CRW :
      if(NEXTCHAR == 'W' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_CRW ,lex->line ,lex->pos,createText("$CRW$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_Def :
      if(NEXTCHAR == 'e' &&
         NEXTCHAR == 'f' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_Def ,lex->line ,lex->pos,createText("$Def$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_E_ :
      switch(NEXTCHAR){
	case 'm':
	  state = LEX_EmS;
	  break;
	case 'q':
	  state = LEX_Eq_;
	  break;
	default:
	  state = LEX_identifier;
	}
      break;

    case LEX_EmS :
      if(NEXTCHAR == 'S' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_EmS ,lex->line ,lex->pos,createText("$EmS$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_Eq_ :
      switch(NEXTCHAR){
	case 'F':
	  state = LEX_EqF;
	  break;
	case 'R':
	  state = LEX_EqR;
	  break;
	default:
	  state = LEX_identifier;
	}
      break;

    case LEX_EqF :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_EqF ,lex->line ,lex->pos,createText("$EqF$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_EqR :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_EqR ,lex->line ,lex->pos,createText("$EqR$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_Fac :
      if(NEXTCHAR == 'a' &&
         NEXTCHAR == 'c' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_Fac ,lex->line ,lex->pos,createText("$Fac$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_In_ :
      if(NEXTCHAR == 'n'){
        switch(NEXTCHAR){
	case 'p':
	  state = LEX_Inp;
	  break;
	case 't':
	  state = LEX_Integer;
	  break;
	default:
	  state = LEX_identifier;
	}
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_Integer :
      if(NEXTCHAR == 'e' &&
         NEXTCHAR == 'g' &&
         NEXTCHAR == 'e' &&
         NEXTCHAR == 'r' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        //Return = createToken( DFG_Integer ,lex->line ,lex->pos,createText("$Integer$",buff_pos+3));
	Return = createToken( DFG_IDENTIFIER ,lex->line ,lex->pos,createText("Integer",buff_pos+1));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
      
    case LEX_Inp :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_Inp ,lex->line ,lex->pos,createText("$Inp$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_KIV :
      if(NEXTCHAR == 'I' &&
         NEXTCHAR == 'V' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_KIV ,lex->line ,lex->pos,createText("$KIV$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_LEM :
      if(NEXTCHAR == 'E' &&
         NEXTCHAR == 'M' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_LEM ,lex->line ,lex->pos,createText("$LEM$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_M_ :
      switch(NEXTCHAR){
	case 'p':
	  state = LEX_Mpm;
	  break;
	case 'R':
	  state = LEX_MRR;
	  break;
	default:
	  state = LEX_identifier;
	}
      break;

    case LEX_Mpm :
      if(NEXTCHAR == 'm' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_Mpm ,lex->line ,lex->pos,createText("$Mpm$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_MRR :
      if(NEXTCHAR == 'R' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_MRR ,lex->line ,lex->pos,createText("$MRR$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_Natural :
      if(NEXTCHAR == 'a' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == 'u' &&
         NEXTCHAR == 'r' &&
         NEXTCHAR == 'a' &&
         NEXTCHAR == 'l' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        //Return = createToken( DFG_Natural ,lex->line ,lex->pos,createText("$Natural$",buff_pos+3));
	Return = createToken( DFG_IDENTIFIER ,lex->line ,lex->pos,createText("Natural",buff_pos+1));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_O_ :
      switch(NEXTCHAR){
	case 'b':
	  state = LEX_Obv;
	  break;
	case 'h':
	  state = LEX_Ohy;
	  break;
        case 'p':
	  state = LEX_Opm;
	  break;
	case 'T':
	  state = LEX_OTTER;
	  break;
	default:
	  state = LEX_identifier;
	}
      break;

    case LEX_Obv :
      if(NEXTCHAR == 'v' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_Obv ,lex->line ,lex->pos,createText("$Obv$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
 
    case LEX_Ohy :
      if(NEXTCHAR == 'y' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_Ohy ,lex->line ,lex->pos,createText("$Ohy$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_Opm :
      if(NEXTCHAR == 'm' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_Opm ,lex->line ,lex->pos,createText("$Opm$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_OTTER :
      if(NEXTCHAR == 'T' &&
         NEXTCHAR == 'E' &&
         NEXTCHAR == 'R' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_OTTER ,lex->line ,lex->pos,createText("$OTTER$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_PROTEIN :
      if(NEXTCHAR == 'R' &&
         NEXTCHAR == 'O' &&
         NEXTCHAR == 'T' &&
         NEXTCHAR == 'E' &&
         NEXTCHAR == 'I' &&
         NEXTCHAR == 'N' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_PROTEIN ,lex->line ,lex->pos,createText("$PROTEIN$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
    
    case LEX_R_ :
      switch(NEXTCHAR){
	case 'a':
	  state = LEX_Rational;
	  break;
	case 'e':
	  state = LEX_Re_;
	  break;
	default:
	  state = LEX_identifier;
	}
      break;
      
      
    case LEX_Rational :
      if(NEXTCHAR == 'a' &&
         NEXTCHAR == 't' &&
         NEXTCHAR == 'i' &&
         NEXTCHAR == 'o' &&
         NEXTCHAR == 'n' &&
         NEXTCHAR == 'a' &&
         NEXTCHAR == 'l' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        //Return = createToken( DFG_Rational ,lex->line ,lex->pos,createText("$Rational$",buff_pos+3));
	Return = createToken( DFG_IDENTIFIER ,lex->line ,lex->pos,createText("Rational",buff_pos+1));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
      
    case LEX_Re_ :
      switch(NEXTCHAR){
	case 'a':
	  state = LEX_Real;
	  break;
	case 's':
	  state = LEX_Res;
	  break;
        case 'w':
	  state = LEX_Rew;
	  break;
	default:
	  state = LEX_identifier;
	}
      break;
      
    case LEX_Real :
      if(NEXTCHAR == 'l' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        //Return = createToken( DFG_Real ,lex->line ,lex->pos,createText("$Real$",buff_pos+3));
	Return = createToken( DFG_IDENTIFIER ,lex->line ,lex->pos,createText("Real",buff_pos+1));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_Rew :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_Rew ,lex->line ,lex->pos,createText("$Rew$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_Res :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_Res ,lex->line ,lex->pos,createText("$Res$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
      
    case LEX_S_ :
      switch(NEXTCHAR){
	case 'h':
	  state = LEX_Shy;
	  break;
	case 'o':
	  state = LEX_SoR;
	  break;
        case 'p':
	  state = LEX_Sp_;
          break;
        case 's':
	  state = LEX_Ssi;
          break;
        case 'A':
	  state = LEX_SATURAT;
          break;
        case 'E':
	  state = LEX_SETHEO;
          break;
        case 'P':
	  state = LEX_SP_;
	  break;
	default:
	  state = LEX_identifier;
	}
      break;

    case LEX_Shy :
      if(NEXTCHAR == 'y' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_Shy ,lex->line ,lex->pos,createText("$Shy$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_SoR :
      if(NEXTCHAR == 'R' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_SoR ,lex->line ,lex->pos,createText("$SoR$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_Sp_ :
      switch(NEXTCHAR){
	case 'L':
	  state = LEX_SpL;
	  break;
	case 's':
	  state = LEX_SpR;
	  break;
        case 't':
	  state = LEX_Spt;
	  break;
	default:
	  state = LEX_identifier;
	}
      break;

    case LEX_SpL :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_SpL ,lex->line ,lex->pos,createText("$SpL$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_SpR :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_SpR ,lex->line ,lex->pos,createText("$SpR$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_Spt :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_Spt ,lex->line ,lex->pos,createText("$Spt$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_Ssi :
      if(NEXTCHAR == 'i' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_Ssi ,lex->line ,lex->pos,createText("$Ssi$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_SATURAT :
      if(NEXTCHAR == 'T' &&
         NEXTCHAR == 'U' &&
         NEXTCHAR == 'R' &&
         NEXTCHAR == 'A' &&
         NEXTCHAR == 'T' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_SATURAT ,lex->line ,lex->pos,createText("$SATURAT$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_SETHEO :
      if(NEXTCHAR == 'T' &&
         NEXTCHAR == 'H' &&
         NEXTCHAR == 'E' &&
         NEXTCHAR == 'O' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_SETHEO ,lex->line ,lex->pos,createText("$SETHEO$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_SP_ :
      switch(NEXTCHAR){
	case 'A':
	  state = LEX_SPASS;
	  break;
	case 'm':
	  state = LEX_SPm;
	  break;
	default:
	  state = LEX_identifier;
	}
      break;

    case LEX_SPm :
      if(NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_SPm ,lex->line ,lex->pos,createText("$SPm$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_SPASS :
      if(NEXTCHAR == 'S' &&
         NEXTCHAR == 'S' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_SPASS ,lex->line ,lex->pos,createText("$SPASS$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
    
    case LEX_T_ :
      switch(NEXTCHAR){
	case 'e':
	  state = LEX_Ter;
	  break;
	case 'o':
	  state = LEX_Top;
	  break;
	default:
	  state = LEX_identifier;
	}
      break;

    case LEX_Ter :
      if(NEXTCHAR == 'r' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_Ter ,lex->line ,lex->pos,createText("$Ter$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
    
    case LEX_Top :
      if(NEXTCHAR == 'p' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        //Return = createToken( DFG_Top ,lex->line ,lex->pos,createText("$Top$",buff_pos+3));
	Return = createToken( DFG_IDENTIFIER ,lex->line ,lex->pos,createText("Top",buff_pos+1));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;


    case LEX_U_ :
      switch(NEXTCHAR){
	case 'n':
	  state = LEX_UnC;
	  break;
	case 'm':
	  state = LEX_URR;
	  break;
	default:
	  state = LEX_identifier;
	}
      break;

    case LEX_UnC :
      if(NEXTCHAR == 'C' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_UnC ,lex->line ,lex->pos,createText("$UnC$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;

    case LEX_URR :
      if(NEXTCHAR == 'R' &&
         NEXTNOIDENT ){
        UNGETCHAR;
        Return = createToken( DFG_URR ,lex->line ,lex->pos,createText("$URR$",buff_pos+3));
        state = LEX_END;
      } else{
        state=LEX_identifier;
      }
      break;
    
    case LEX_identifier :
      UNGETCHAR;
      Return = getIdentifier(lex,&buff_pos);
      *pbuff_pos=buff_pos;
      return Return;
      break;
    
    default:
      //ERROR
      ;
    }
  }
  *pbuff_pos=buff_pos;
  return Return;
}

void dfg_ErrorPrintToken(DFG_TOKEN tok)
{
  if(tok == NULL){
    misc_UserErrorReport("(NULL)");
    return;
  }
  misc_UserErrorReport("%s (line: %d, pos: %d)",tok->text,tok->line,tok->pos);
}


void dfg_ErrorPrintType(dfgtokentype type)
{
  switch(type){
  case DFG_POINT: misc_UserErrorReport("."); break;
  case DFG_COMMA: misc_UserErrorReport(","); break;
  case DFG_OPENBR: misc_UserErrorReport("("); break;
  case DFG_CLOSEBR: misc_UserErrorReport(")"); break;
  case DFG_OPENEBR: misc_UserErrorReport("["); break;
  case DFG_CLOSEEBR: misc_UserErrorReport("]"); break;
  case DFG_OPENPBR: misc_UserErrorReport("<"); break;
  case DFG_CLOSEPBR: misc_UserErrorReport(">"); break;
  case DFG_UNARY_MINUS: misc_UserErrorReport("-"); break;
  case DFG_ARROW: misc_UserErrorReport("->"); break;
  case DFG_SYMB_PLUS: misc_UserErrorReport("+"); break;
  case DFG_ARROW_DOUBLELINE: misc_UserErrorReport("||"); break;
  case DFG_COLON: misc_UserErrorReport(":"); break;
  case DFG_3TAP: misc_UserErrorReport("3TAP"); break;
    
  case DFG_TEXT: misc_UserErrorReport("Text"); break;
			
  case DFG_all: misc_UserErrorReport("all"); break;
  case DFG_and: misc_UserErrorReport("and"); break;
  case DFG_author: misc_UserErrorReport("author"); break;
  case DFG_axioms: misc_UserErrorReport("axioms"); break;
			
  case DFG_begin_problem: misc_UserErrorReport("begin_problem"); break;
  case DFG_box: misc_UserErrorReport("box"); break;
    
  case DFG_clause: misc_UserErrorReport("clause"); break;
  case DFG_cnf: misc_UserErrorReport("cnf"); break;
  case DFG_comp: misc_UserErrorReport("comp"); break;
  case DFG_concept_formula: misc_UserErrorReport("concept_formula"); break;
  case DFG_conjectures: misc_UserErrorReport("conjectures"); break;
  case DFG_conv: misc_UserErrorReport("conv"); break;
    
  case DFG_datatype: misc_UserErrorReport("datatype"); break;
  case DFG_date: misc_UserErrorReport("date"); break;
  case DFG_def: misc_UserErrorReport("def"); break;
  case DFG_description: misc_UserErrorReport("description"); break;
  case DFG_dia: misc_UserErrorReport("dia"); break;
  case DFG_div: misc_UserErrorReport("div"); break;
  case DFG_distinct_symbols: misc_UserErrorReport("distinct_symbols"); break;
  case DFG_dl: misc_UserErrorReport("dl"); break;
  case DFG_domain: misc_UserErrorReport("domain"); break;
  case DFG_domrestr: misc_UserErrorReport("domrestr"); break;

  case DFG_eml: misc_UserErrorReport("eml"); break;
  case DFG_end_of_list: misc_UserErrorReport("end_of_list"); break;
  case DFG_end_problem: misc_UserErrorReport("end_problem"); break;
  case DFG_equal: misc_UserErrorReport("equal"); break;
  case DFG_equiv: misc_UserErrorReport("equiv"); break;
  case DFG_exists: misc_UserErrorReport("exists"); break;

  case DFG_false: misc_UserErrorReport("false"); break;
  case DFG_forall: misc_UserErrorReport("forall"); break;
  case DFG_formula: misc_UserErrorReport("formula"); break;
  case DFG_fract: misc_UserErrorReport("fract"); break;
  case DFG_function: misc_UserErrorReport("function"); break;
  case DFG_functions: misc_UserErrorReport("functions"); break;
    
  case DFG_ge: misc_UserErrorReport("ge"); break;
  case DFG_gs: misc_UserErrorReport("gs"); break;
    
  case DFG_hypothesis: misc_UserErrorReport("hypothesis"); break;
    
  case DFG_id: misc_UserErrorReport("id"); break;
  case DFG_implied: misc_UserErrorReport("implied"); break;
  case DFG_implies: misc_UserErrorReport("implies"); break;
  case DFG_include: misc_UserErrorReport("include"); break;

  case DFG_le: misc_UserErrorReport("le"); break;
  case DFG_list_of_clauses: misc_UserErrorReport("list_of_clauses"); break;
  case DFG_list_of_declarations: misc_UserErrorReport("list_of_declarations"); break;
  case DFG_list_of_descriptions: misc_UserErrorReport("list_of_descriptions"); break;
  case DFG_list_of_formulae: misc_UserErrorReport("list_of_formulae"); break;
  case DFG_list_of_general_settings: misc_UserErrorReport("list_of_general_settings"); break;
  case DFG_list_of_includes: misc_UserErrorReport("list_of_includes"); break;
  case DFG_list_of_proof: misc_UserErrorReport("list_of_proof"); break;
  case DFG_list_of_settings: misc_UserErrorReport("list_of_settings"); break;
  case DFG_list_of_special_formulae: misc_UserErrorReport("list_of_special_formulae"); break;
  case DFG_list_of_symbols: misc_UserErrorReport("list_of_symbols"); break;
  case DFG_logic: misc_UserErrorReport("logic"); break;
  case DFG_lr: misc_UserErrorReport("lr"); break;
  case DFG_ls: misc_UserErrorReport("ls"); break;
  case DFG_lt: misc_UserErrorReport("lt"); break;

  case DFG_minus: misc_UserErrorReport("minus"); break;
  case DFG_mult: misc_UserErrorReport("mult"); break;
    
  case DFG_name: misc_UserErrorReport("name"); break;
  case DFG_not: misc_UserErrorReport("not"); break;
    
  case DFG_or: misc_UserErrorReport("or"); break;
  
  case DFG_plus: misc_UserErrorReport("plus"); break;
  case DFG_predicate: misc_UserErrorReport("predicate"); break;
  case DFG_predicates: misc_UserErrorReport("predicates"); break;
  case DFG_prop_formula: misc_UserErrorReport("prop_formula"); break;
    
  case DFG_range: misc_UserErrorReport("range"); break;
  case DFG_ranrester: misc_UserErrorReport("ranrester"); break;
  case DFG_rel_formula: misc_UserErrorReport("rel_formula"); break;
  case DFG_role_formula: misc_UserErrorReport("role_formula"); break;
  
  case DFG_satisfiable: misc_UserErrorReport("satisfiable"); break;
  case DFG_set_flag: misc_UserErrorReport("set_flag"); break;
  case DFG_set_precedence: misc_UserErrorReport("set_precedence"); break;
  case DFG_set_selection: misc_UserErrorReport("set_selection"); break;
  case DFG_set_ClauseFormulaRelation: misc_UserErrorReport("set_ClauseFormulaRelation"); break;
  case DFG_set_DomPred: misc_UserErrorReport("set_DomPred"); break;
  case DFG_some: misc_UserErrorReport("some"); break;
  case DFG_sorts: misc_UserErrorReport("sorts"); break;
  case DFG_splitlevel: misc_UserErrorReport("splitlevel"); break;
  case DFG_status: misc_UserErrorReport("status"); break;
  case DFG_step: misc_UserErrorReport("step"); break;
  case DFG_subsort: misc_UserErrorReport("subsort"); break;
  case DFG_sum: misc_UserErrorReport("sum"); break;
    
  case DFG_test: misc_UserErrorReport("test"); break;

  case DFG_translpairs: misc_UserErrorReport("translpairs"); break;
  case DFG_true: misc_UserErrorReport("true"); break;

  case DFG_unknown: misc_UserErrorReport("unknown"); break;
  case DFG_unsatisfiable: misc_UserErrorReport("unsatisfiable"); break;

  case DFG_version: misc_UserErrorReport("version"); break;

  case DFG_App: misc_UserErrorReport("App"); break;
  case DFG_AED: misc_UserErrorReport("AED"); break;
  
  case DFG_Con: misc_UserErrorReport("Con"); break;
  case DFG_CRW: misc_UserErrorReport("CRW"); break;
  
  case DFG_Def: misc_UserErrorReport("Def"); break;
  
  case DFG_EmS: misc_UserErrorReport("EmS"); break;
  case DFG_EqF: misc_UserErrorReport("EqF"); break;
  case DFG_EqR: misc_UserErrorReport("EqR"); break;
    
  case DFG_Fac: misc_UserErrorReport("Fac"); break;
      
  case DFG_Inp: misc_UserErrorReport("Inp"); break;
  case DFG_Integer: misc_UserErrorReport("Integer"); break;
  
  case DFG_KIV: misc_UserErrorReport("KIV"); break;
  
  case DFG_LEM: misc_UserErrorReport("LEM"); break;
  
  case DFG_Mpm: misc_UserErrorReport("Mpm"); break;
  case DFG_MRR: misc_UserErrorReport("MRR"); break;
  
  case DFG_Natural: misc_UserErrorReport("Natural"); break;
  
  case DFG_Obv: misc_UserErrorReport("Obv"); break;
  case DFG_Ohy: misc_UserErrorReport("Ohy"); break;
  case DFG_Opm: misc_UserErrorReport("Opm"); break;
  case DFG_OTTER: misc_UserErrorReport("OTTER"); break;
  
  case DFG_PROTEIN: misc_UserErrorReport("PROTEIN"); break;
  
  case DFG_Rational: misc_UserErrorReport("Rational"); break;
  case DFG_Real: misc_UserErrorReport("Real"); break;
  case DFG_Rew: misc_UserErrorReport("Rew"); break;
  case DFG_Res: misc_UserErrorReport("Res"); break;
			
  case DFG_Shy: misc_UserErrorReport("Shy"); break;
  case DFG_SoR: misc_UserErrorReport("SoR"); break;
  case DFG_SpL: misc_UserErrorReport("SpL"); break;
  case DFG_SpR: misc_UserErrorReport("SpR"); break;
  case DFG_SPm: misc_UserErrorReport("SPm"); break;
  case DFG_Spt: misc_UserErrorReport("Spt"); break;
  case DFG_Ssi: misc_UserErrorReport("Ssi"); break;
  case DFG_SATURAT: misc_UserErrorReport("SATURAT"); break;
  case DFG_SETHEO: misc_UserErrorReport("SETHEO"); break;
  case DFG_SPASS: misc_UserErrorReport("SPASS"); break;
  case DFG_Ter: misc_UserErrorReport("Ter"); break;
  case DFG_Top: misc_UserErrorReport("Top"); break;
			
  case DFG_UnC: misc_UserErrorReport("UnC"); break;
  case DFG_URR: misc_UserErrorReport("URR"); break;
			
  case DFG_NUMBER: misc_UserErrorReport("Number"); break;
			
  case DFG_IDENTIFIER: misc_UserErrorReport("Identifier"); break;
  case DFG_WhiteSpace: misc_UserErrorReport(" "); break;
  case DFG_NextLine: misc_UserErrorReport("NextLine"); break;
  case DFG_FileEnd: misc_UserErrorReport("End of File"); break;
  default: misc_UserErrorReport("");
  }
}
