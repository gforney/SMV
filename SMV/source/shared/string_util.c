// $Date$ 
// $Revision$
// $Author$

// svn revision character string
char string_util_revision[]="$Revision$";

#define IN_STRING_UTIL
#include "options.h"
#include <stdio.h>  
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#ifdef WIN32
#include <dirent_win.h>
#else
#include <dirent.h>
#endif
#include "MALLOC.h"
#include "datadefs.h"
#include "string_util.h"

/* ----------------------- fparsecsv ----------------------------- */

void fparsecsv(char *buffer, float *vals, int *valids, int ncols, int *ntokens){
  /*! \fn void fparsecsv(char *buffer, float *vals, int ncols, int *ntokens)
      \brief copy comma delimited values from buffer into floating point array vals
       returning number of values found in ntokens
  */
  int nt=0;
  char *token;

  token=strtok(buffer,",");
  while(token!=NULL&&nt<ncols){
    if(STRCMP(token,"NULL")==0){
      valids[nt]=0;
      vals[nt]=0.0;
    }
    else{
      valids[nt]=1;
      sscanf(token,"%f",&vals[nt]);
    }
    nt++;
    token=strtok(NULL,",");
    if(token!=NULL)trim(token);
  }
  *ntokens=nt;
}

/* ----------------------- parsecsv ----------------------------- */

void parsecsv(char *buffer, char **tokens, int ncols, int *ntokens){
  /*! \fn void parsecsv(char *buffer, char **tokens, int ncols, int *ntokens)
      \brief copy comma delimited values from buffer into character array tokens
       returning number of values found in ntokens
  */
  int nt=0;
  int i;
  int lenbuffer;

  lenbuffer=strlen(buffer);
  for(i=0;i<lenbuffer;i++){
    if(buffer[i]==',')buffer[i]=0;
  }
  tokens[nt++]=buffer;
  for(i=1;i<lenbuffer;i++){
    if(buffer[i]==0){
      tokens[nt++]=buffer+i+1;
    }
  }
  *ntokens=nt;
}

/* ------------------ getrowcols ------------------------ */

int getrowcols(FILE *stream, int *nrows, int *ncols){
  /*! \fn int getrowcols(FILE *stream, int *nrows, int *ncols)
      \brief find number of rows (nrows) and number of columns (ncols) in
       comma delimited file pointed to by stream and returns the length
       of the longest line
  */
  int nnrows=0,nncols=1,maxcols=0,linelength=0,maxlinelength=0;

  while(!feof(stream)){
    char ch;

    ch = getc(stream);
    linelength++;
    if(ch == ',')nncols++;
    if(ch=='\n'){
      if(linelength>maxlinelength)maxlinelength=linelength;
      if(nncols>maxcols)maxcols=nncols;
      linelength=0;
      nncols=1;
      nnrows++;
    }
  }
  *nrows=nnrows;
  *ncols=maxcols;
  rewind(stream);
  return maxlinelength;
}

/* ------------------ getrevision ------------------------ */

int getrevision(char *svn){
  char svn_string[256];
  char *svn_ptr;
  int return_val;

  svn_ptr=svn_string;
  svn=strchr(svn,':');
  if(svn==NULL||strlen(svn)<=4)return 0;
  
  svn++;
  strcpy(svn_ptr,svn);
  svn_ptr=trim_front(svn_ptr);
  svn_ptr[strlen(svn_ptr)-1]=0;
  trim(svn_ptr);
  sscanf(svn_ptr,"%i",&return_val);
  return return_val;
}

/* ------------------ stripquotes ------------------------ */

void stripquotes(char *buffer){
  /*! \fn void stripquotes(char *buffer)
      \brief replaces quotes (") with blanks in the character string buffer
  */
  char *c;

  for(c=buffer;c<buffer+strlen(buffer);c++){
    if(*c=='"')*c=' ';
  }
}
/* ------------------ stripcommas ------------------------ */

void stripcommas(char *buffer){
  /*! \fn void stripcommas(char *buffer)
      \brief replaces commas (,) with blanks in the character string buffer
  */
  char *c;

  for(c=buffer;c<buffer+strlen(buffer);c++){
    if(*c==',')*c=' ';
  }
}

/* ------------------ randint ------------------------ */

int randint(int min, int max){
  /*! \fn int randint(int min, int max)
      \brief returns a random integer inclusively between min and max 
  */
  int return_val;

  if (min>max){
    return_val = max+((min-max+1)*(float)rand()/(float)(RAND_MAX+1));
  }
  else{
    return_val = min+((max-min+1)*(float)rand()/(float)(RAND_MAX+1));
  }
  return return_val;
}

/* ------------------ randstr ------------------------ */

char *randstr(char* str, int length){
  /*! \fn char *randstr(char* str, int length)
      \brief returns a random character string of length length
  */
    int i;

    if (str==NULL||length<=0)return NULL;

    for (i=0;i<length;i++){
      str[i]=(char)randint(65,90);
    }
    str[length]=0;
    return str;
}

/* ------------------ trim_commas ------------------------ */

void trim_commas(char *line){
  char *c;
  
  for(c = line + strlen(line) - 1;c>=line;c--){
    if(isspace(*c))continue;
    if(strncmp(c,",",1)!=0)break;
    *c=' ';
  }
}

/* ------------------ trim ------------------------ */

void trim(char *line){
  /*! \fn void trim(char *line)
      \brief removes trailing white space from the character string line
  */
  char *c;
  size_t len;

  if(line==NULL)return;
  len = strlen(line);
  if(len==0)return;
  for(c=line+len-1; c>=line; c--){
    if(isspace(*c))continue;
    *(c+1)='\0';
    return;
  }
  *line='\0';
}

/* ------------------ trim_front ------------------------ */

char *trim_front(char *line){
  /*! \fn char *trim_front(char *line)
      \brief returns a pointer to the first non-blank character in the character string line
  */
  char *c;

  for(c=line;c<=line+strlen(line)-1;c++){
    if(!isspace(*c))return c;
  }
  return line;
}


/* ------------------ trimzeros ------------------------ */

void trimzeros(char *line){
  /*! \fn void trimzeros(char *line)
      \brief removes trailing zeros in the floating point number found in line
  */
  size_t i,len;
  char *c;

  len = strlen(line);
  c = line + len-1;
  for(i=len-1;i>0;i--){
    if(*c=='0'){
      c--;
      if(*c=='.'){
        line[i+1]='\0';
        return;
      }
      continue;
    }
    line[i+1]='\0';
    return;
  }
  line[0]='\0';
}

/* ------------------ trimmzeros ------------------------ */

void trimmzeros(char *line){
  /*! \fn void trimmzeros(char *line)
      \brief removes trailing zeros in each floating point number found in line
  */
  char linecopy[1024];
  char *token;

  trim(line);
  strcpy(linecopy,line);
  token=strtok(linecopy," ");
  strcpy(line,"");
  while(token!=NULL){
    trimzeros(token);
    strcat(line,token);
    strcat(line," ");
    token=strtok(NULL," ");
  }
}

/* ------------------ STRCMP ------------------------ */

int STRCMP(const char *s1, const char *s2){
  /*! \fn int STRCMP(const char *s1, const char *s2)
      \brief same as the standard function, strcmp, but ignores case
  */
  while (toupper(*s1) == toupper(*s2++)){
		if (*s1++ == 0)return (0);
  }
  return (toupper(*(const unsigned char *)s1) - toupper(*(const unsigned char *)(s2 - 1)));
}

/* ------------------ STRSTR ------------------------ */

char *STRSTR(char *string, const char *key){
  char *k,*s,*ss;

  if(string==NULL||key==NULL)return NULL;

  for(s=string;*s!=0;s++){
    for(k=(char *)key;*k!=0;k++){
      ss = s + (k-key);
      if(*ss==0)return NULL;
      if(toupper(*ss)!=toupper(*k))break;
    }
    if(*k==0)return s;
  }
  return NULL;
}

/* ------------------ scalestring ------------------------ */

void scalestring(const char *stringfrom, char *stringto, const float *scale, float range){
  float val;
  sscanf(stringfrom,"%f",&val);
  val = scale[0]*val+scale[1];
  num2string(stringto,val,scale[0]*range);
}

/* ------------------ scalefloat2string ------------------------ */

void scalefloat2string(float floatfrom, char *stringto, const float *scale, float range){
  float val;
  val = scale[0]*floatfrom+scale[1];
  num2string(stringto,val,scale[0]*range);
}

/* ------------------ num2string ------------------------ */

void num2string(char *string, float tval,float range){
  float tval2,mant10;
  int exp10;

  //if(ABS((double)tval)<ABS((double)range)/100.0f)tval=0.0f;
  tval2=tval; 
  if(tval2<0.0)tval2=-tval2;
  if(0.01<=tval2&&tval2<0.1){
    sprintf(string,"%3.2f",tval);
  }
  else if(0.1<=tval2&&tval2<1.0){
    sprintf(string,"%3.2f",tval);
  }
  else if(1.0<=tval2&&tval2<10.0){
    sprintf(string,"%3.2f",tval);
  }
  else if(10.0<=tval2&&tval2<100.0){
    sprintf(string,"%3.1f",tval);
  }
  else if(100.0<=tval2&&tval2<1000.0){
    sprintf(string,"%3.0f",tval);
  }
  else if(1000.0<=tval2&&tval2<10000.0){
    sprintf(string,"%4.0f",tval);
  }
  else if(10000.0<=tval2&&tval2<100000.0){
    sprintf(string,"%5.0f",tval);
    }
  else if(tval2==0.0){STRCPY(string,"0.00");}
  else{
    mant10 = frexp10(tval,&exp10);
    mant10 = (float)((int)(10.0f*mant10+0.5f))/10.0f;
    if(mant10>=10.0f){
      mant10/=10.0f;
      exp10++;
    }
    if(exp10<-99)STRCPY(string,"0.00");
    else if(exp10>=-99&&exp10<-9){sprintf(string,"%2.1f%i",mant10,exp10);}
    else if(exp10>99)STRCPY(string,"***");
    else{
      if(exp10==0){sprintf(string,"%2.1f",mant10);}
      else{sprintf(string,"%2.1fE%i",mant10,exp10);}
    }

    /*sprintf(string,"%1.1e",tval); */
  }
  if(strlen(string)>9)printf("ut oh - overwriting string\n");


}

/* ------------------ trim_string ------------------------ */

char *trim_string(char *buffer){
  /*! \fn char *trim_string(char *buffer)
      \brief removes trailing blanks from buffer and returns a pointer to the first non-blank character
  */
  int len;
  char *bufptr;

  if(buffer==NULL)return NULL;
  len=strlen(buffer);
  buffer[len-1]='\0';
  bufptr=trim_front(buffer);
  trim(bufptr);
  return bufptr;
}

/* ------------------ get_chid ------------------------ */

char *get_chid(char *file, char *buffer){
  FILE *stream;
  char *chidptr,*c;
  unsigned int i;
  int found1st, found2nd;

  if(file==NULL)return NULL;
  stream=fopen(file,"r");
  if(stream==NULL)return NULL;

  found1st=0;
  found2nd=0;
  chidptr=NULL;
  while(!feof(stream)){
    found1st=0;
    found2nd=0;

    if(fgets(buffer,255,stream)==NULL)break;
    chidptr=strstr(buffer,"CHID");
    if(chidptr==NULL)continue;

    chidptr+=5;
    for(i=0;i<strlen(chidptr);i++){
      c=chidptr+i;
      if(*c=='\''){
        found1st=1;
        chidptr=c+1;
        break;
      }
    }
    if(found1st==0)break;

    for(i=0;i<strlen(chidptr);i++){
      c=chidptr+i;
      if(*c=='\''){
        found2nd=1;
        *c=0;
        break;
      }
    }
    break;
  }
  fclose(stream);
  if(found1st==0||found2nd==0)chidptr=NULL;
  return chidptr;
}
#ifdef pp_GPU

/* ------------------ log_base2 ------------------------ */

int log_base2(float xx){
  /*! \fn int log_base2(float xx)
      \brief returns the log base 2 of the floating point number xx
  */
  int r = 0;
  unsigned int x;

  x=xx;
  while( (x >> r) != 0){
    r++;
  }
  return r-1; // returns -1 for x==0, floor(log2(x)) otherwise
}
#endif

/* ------------------ array2string ------------------------ */

void array2string(float *vals, int nvals, char *string){
  /*! \fn void array2string(float *vals, int nvals, char *string)
      \brief convert an array of floating point numbers to a character string
  */
  char cval[30];
  int i;

  strcpy(string,"");
  if(nvals==0)return;
  for(i=0;i<nvals-1;i++){
    sprintf(cval,"%f",vals[i]);
    trimzeros(cval);
    strcat(string,cval);
    strcat(string,", ");
  }
  sprintf(cval,"%f",vals[nvals-1]);
  trimzeros(cval);
  strcat(string,cval);
}

/* ------------------ frexp10 ------------------------ */

float frexp10(float x, int *exp10){
  float xabs, mantissa;

  xabs = ABS((double)x);
  if(x==0.0f){
    *exp10=0;
    return 0.0f;
  }
  mantissa = log10((double)xabs);
  *exp10 = (int)floor((double)mantissa);
      
  mantissa = pow((double)10.0f,(double)mantissa-(double)*exp10);
  if(x<0)mantissa = -mantissa;
  return mantissa;
}

/* ------------------ getstring ------------------------ */

char *getstring(char *buffer){
  /*! \fn *getstring(char *buffer)
      \brief return pointer to string contained between a pair of double quotes
  */
  char *begin,*end;

  // if buffer contains msgid "string"
  // return a pointer to s in string

  begin=strchr(buffer,'"');
  if(begin==NULL)return NULL;
  begin++;
  end=strrchr(begin,'"');
  if(end==NULL)return NULL;
  end[0]=0;
  trim(begin);
  begin = trim_front(begin);
  if(strlen(begin)>0)return begin;
  return NULL;
}

  /* ------------------ time2timelabel ------------------------ */

char *time2timelabel(float sv_time, float dt, char *timelabel){
  char *timelabelptr;

  if(dt<0.001){
    sprintf(timelabel,"%4.4f",sv_time);
  }
  else if(dt>=0.001&&dt<0.01){
    sprintf(timelabel,"%4.3f",sv_time);
  }
  else if(dt>=0.01&&dt<0.1){
    sprintf(timelabel,"%4.2f",sv_time);
  }
  else{
    sprintf(timelabel,"%4.1f",sv_time);
  }
  trimzeros(timelabel);
  trim(timelabel);
  timelabelptr=trim_front(timelabel);
  return timelabelptr;
}

/* ------------------ match ------------------------ */

int match(char *buffer, const char *key){
  size_t lenbuffer;
  size_t lenkey;

  lenkey=strlen(key);
  lenbuffer=strlen(buffer);
  if(lenbuffer<lenkey)return 0;
  if(strncmp(buffer,key,lenkey) != 0)return 0;
  if(lenbuffer>lenkey&&!isspace(buffer[lenkey]))return 0;
  return 1;
}

/* ------------------ match_upper ------------------------ */

int match_upper(char *buffer, const char *key){
  size_t lenbuffer;
  size_t lenkey;
  size_t i;

  lenkey=strlen(key);
  trim(buffer);
  lenbuffer=strlen(buffer);

  if(lenbuffer<lenkey)return 0;
  for(i=0;i<lenkey;i++){
    if(toupper(buffer[i])!=toupper(key[i]))return 0;
  }
  if(lenbuffer>lenkey&&buffer[lenkey]==':')return 2;
  if(lenbuffer>lenkey&&!isspace(buffer[lenkey]))return 0;
  return 1;
}

/* ----------------------- match_wild ----------------------------- */

int match_wild(char *pTameText, char *pWildText){
// This function compares text strings, the second of which can have wildcards ('*').
//
//Matching Wildcards: An Algorithm
//by Kirk J. Krauss
// http://drdobbs.com/windows/210200888
// (modified from original by setting bCaseSensitive and cAltTerminator in the 
//  body of the routine and changing routine name to match_wild, also changed
//  formatting to be consistent with smokeview coding style)

  char cAltTerminator='\0';
#ifdef WIN32
  int bCaseSensitive=0;
#else
  int bCaseSensitive=1;
#endif
  int bMatch = 1;
  char *pAfterLastWild = NULL; // The location after the last '*', if we've encountered one
  char *pAfterLastTame = NULL; // The location in the tame string, from which we started after last wildcard
  char t, w;

  if(*pWildText==cAltTerminator)return 1;

        // Walk the text strings one character at a time.
  for(;;){
    t = *pTameText;
    w = *pWildText;

    if (!t || t == cAltTerminator){
      if (!w || w == cAltTerminator)break;    // "x" matches "x"
      else if (w == '*'){
        pWildText++;
        continue;                             // "x*" matches "x" or "xy"
      }
      else if (pAfterLastTame){
        if (!(*pAfterLastTame) || *pAfterLastTame == cAltTerminator){
          bMatch = 0;
          break;
        }
        pTameText = pAfterLastTame++;
        pWildText = pAfterLastWild;
        continue;
      }
      bMatch = 0;
      break;                                  // "x" doesn't match "xy"
    }
    else{
      if (!bCaseSensitive){
  //   convert characters to lowercase
        if (t >= 'A' && t <= 'Z')t += ('a' - 'A');
        if (w >= 'A' && w <= 'Z')w += ('a' - 'A');
      }
      if (t != w){
        if (w == '*'){
          pAfterLastWild = ++pWildText;
          pAfterLastTame = pTameText;
          continue;                           // "*y" matches "xy"
        }
        else if (pAfterLastWild){
          pWildText = pAfterLastWild;
          w = *pWildText;
          if (!w || w == cAltTerminator)break;// "*" matches "x"
          else{
            if (!bCaseSensitive && w >= 'A' && w <= 'Z')w += ('a' - 'A');
            if (t == w)pWildText++;
          }
          pTameText++;
          continue;                           // "*sip*" matches "mississippi"
        }
        else{
          bMatch = 0;
          break;                              // "x" doesn't match "y"
        }
      }
    }
    pTameText++;
    pWildText++; 
  }
  return bMatch;
}

/* ----------------------- remove_comment ----------------------------- */

void remove_comment(char *buffer){
  char *comment;
  comment = strstr(buffer,"//");
  if(comment!=NULL)comment[0]=0;
  return;
}

/* ------------------ getPROGversion ------------------------ */

void getPROGversion(char *PROGversion){
  strcpy(PROGversion,PROGVERSION);
}


/* ------------------ readlabels ------------------------ */

int readlabels(flowlabels *flowlabel, FILE *stream){
  char buffer2[255], *buffer;
  size_t len;

  if(fgets(buffer2,255,stream)==NULL){
    strcpy(buffer2,"*");
  }

  len=strlen(buffer2);
  buffer2[len-1]='\0';
  buffer=trim_front(buffer2);
  trim(buffer);
  len=strlen(buffer);
  if(NewMemory((void **)&flowlabel->longlabel,(unsigned int)(len+1))==0)return 2;
  STRCPY(flowlabel->longlabel,buffer);


  if(fgets(buffer2,255,stream)==NULL){
    strcpy(buffer2,"**");
  }

  len=strlen(buffer2);
  buffer2[len-1]='\0';
  buffer=trim_front(buffer2);
  trim(buffer);
  len=strlen(buffer);
  if(NewMemory((void **)&flowlabel->shortlabel,(unsigned int)(len+1))==0)return 2;
  STRCPY(flowlabel->shortlabel,buffer);

  if(fgets(buffer2,255,stream)==NULL){
    strcpy(buffer2,"***");
  }

  len=strlen(buffer2);
  buffer2[len-1]='\0';
  buffer=trim_front(buffer2);
  trim(buffer);
  len=strlen(buffer);
  if(NewMemory((void *)&flowlabel->unit,(unsigned int)(len+1))==0)return 2;
  STRCPY(flowlabel->unit,buffer);
  return 0;
}

/* ------------------ readlabels_cellcenter ------------------------ */

int readlabels_cellcenter(flowlabels *flowlabel, FILE *stream){
  char buffer2[255], *buffer;
  size_t len;

  if(fgets(buffer2,255,stream)==NULL){
    strcpy(buffer2,"*");
  }

  len=strlen(buffer2);
  buffer2[len-1]='\0';
  buffer=trim_front(buffer2);
  trim(buffer);
  len=strlen(buffer);
  if(NewMemory((void **)&flowlabel->longlabel,(unsigned int)(len+1+15))==0)return 2;
  STRCPY(flowlabel->longlabel,buffer);
  STRCAT(flowlabel->longlabel,"(cell centered)");

  if(fgets(buffer2,255,stream)==NULL){
    strcpy(buffer2,"**");
  }

  len=strlen(buffer2);
  buffer2[len-1]='\0';
  buffer=trim_front(buffer2);
  trim(buffer);
  len=strlen(buffer);
  if(NewMemory((void **)&flowlabel->shortlabel,(unsigned int)(len+1))==0)return 2;
  STRCPY(flowlabel->shortlabel,buffer);

  if(fgets(buffer2,255,stream)==NULL){
    strcpy(buffer2,"***");
  }

  len=strlen(buffer2);
  buffer2[len-1]='\0';
  buffer=trim_front(buffer2);
  trim(buffer);
  len=strlen(buffer);
  if(NewMemory((void *)&flowlabel->unit,(unsigned int)(len+1))==0)return 2;
  STRCPY(flowlabel->unit,buffer);
  return 0;
}

/* ------------------ readlabels_terrain ------------------------ */

int readlabels_terrain(flowlabels *flowlabel, FILE *stream){
  char buffer2[255],*buffer;
  size_t len;

  if(fgets(buffer2,255,stream)==NULL){
    strcpy(buffer2,"*");
  }

  len=strlen(buffer2);
  buffer2[len-1]='\0';
  buffer=trim_front(buffer2);
  trim(buffer);
  len=strlen(buffer);
  if(NewMemory((void **)&flowlabel->longlabel,(unsigned int)(len+1+9))==0)return 2;
  STRCPY(flowlabel->longlabel,buffer);
  STRCAT(flowlabel->longlabel,"(terrain)");

  if(fgets(buffer2,255,stream)==NULL){
    strcpy(buffer2,"**");
  }

  len=strlen(buffer2);
  buffer2[len-1]='\0';
  buffer=trim_front(buffer2);
  trim(buffer);
  len=strlen(buffer);
  if(NewMemory((void **)&flowlabel->shortlabel,(unsigned int)(len+1))==0)return 2;
  STRCPY(flowlabel->shortlabel,buffer);

  if(fgets(buffer2,255,stream)==NULL){
    strcpy(buffer2,"***");
  }

  len=strlen(buffer2);
  buffer2[len-1]='\0';
  buffer=trim_front(buffer2);
  trim(buffer);
  len=strlen(buffer);
  if(NewMemory((void *)&flowlabel->unit,(unsigned int)(len+1))==0)return 2;
  STRCPY(flowlabel->unit,buffer);
  return 0;
}


