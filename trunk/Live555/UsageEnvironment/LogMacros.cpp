#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
//#include <sys/time.h>
#include "GroupsockHelper.hh"
#include <errno.h>

#include "LogMacros.hh"

extern int errno;

//void DEBUG_LOG(char *fmt,...)__attribute__((format(printf,1,2)));

FILE *g_fout = NULL;
const char* LOG_FILE_PREFIX = "debug";
const char* LOG_FILE_POSTFIX = "log";
const char* LOG_FILE_NAME = "debug.log";

void initDebugLog(const char* filename)
{
      if(NULL == (g_fout = fopen(filename, "w+")))
      {
          fprintf(stderr, "Open log file error: %s", strerror(errno));
      }
}

bool isWriteLog()
{
  return NULL != g_fout;
}

void writeLog(int type, const char *fmt,...)
{
    char szTimeString[128] = {0};
    struct timeval tv;
    struct tm *today;
    time_t tmp_time;
    int off;
    
    char buf[4096] = {0};
    va_list ap;// typedef char *  va_list;

    gettimeofday(&tv, NULL);

    tmp_time = tv.tv_sec;
    today = localtime(&tmp_time);
    off = strftime( szTimeString, 128, "%y-%m-%d %H:%M:%S", today );
    sprintf(szTimeString+off, ".%03ld", tv.tv_usec/1000);

    
    va_start(ap, fmt);//#define va_start(ap,v)  ( ap = (va_list)&v + _INTSIZEOF(v) )
    vsprintf( buf, fmt, ap );
    va_end( ap);   //#define va_end(ap)      ( ap = (va_list)0 )

    if(ERR == type)
    {
        fprintf(stderr, "[%s][%s]%s\n", szTimeString, "Err", buf);
        fprintf(g_fout, "[%s][%s]%s\n", szTimeString, "Err", buf);
    }
    else
    {
        fprintf(g_fout, "[%s][%s]%s\n", szTimeString, "Inf", buf);
    }
    
    fflush(g_fout);
}

const char* getName(const char* path)
{
  int i;
  if(NULL == path)
  {
      return "null";
  }
  for(i= strlen(path)-1; i >= 0; i--)
  {
      if(*(path+i) == '\\' || *(path+i) == '/')
      {
          return path+i+1;
      }
  }
  return path;
}

void printHex(const unsigned char* start, int len, const char* filename, int linenumber)
{
  char szTimeString[128] = {0};
  struct timeval tv;
  struct tm *today;
  time_t tmp_time;
  int off;
  
  static const int BUF_SIZE = 78;
  static const int OFFSET_HEAD = 4;
  static const int OFFSET_HEX = 10;
  static const int OFFSET_CHAR = 62;
  int iAllLine = len/16;
  char szLineBuf[BUF_SIZE+1] = {0};
  int iCurLine;
  int iCurMaxCharNum;
  int i, j, left;

  gettimeofday(&tv, NULL);
  
  tmp_time = tv.tv_sec;
  today = localtime(&tmp_time);
  off = strftime( szTimeString, 128, "%y-%m-%d %H:%M:%S", today );
  sprintf(szTimeString+off, ".%03ld", tv.tv_usec/1000);
  
  fprintf(g_fout, "[%s][Hex][%s:%d]>> start = %p, lenght = %d\n", 
    szTimeString, filename, linenumber, start, len);

  //    [0001] 01 20 32 23 43 23 35 54 23 12 43 12 23 53 23 12     0ksdnf..sdfl

  

  for(iCurLine = 0; iCurLine <= iAllLine; iCurLine++)
  {
    memset(szLineBuf, ' ', BUF_SIZE);
    szLineBuf[BUF_SIZE] = '\0';
    
    sprintf(szLineBuf+OFFSET_HEAD, "[%04d]", iCurLine+1);

    iCurMaxCharNum = (iCurLine == iAllLine ? (len%16) : 16);
    for(i = 0; i < iCurMaxCharNum; i++)
    {
      sprintf(szLineBuf+OFFSET_HEX+i*3, " %02X", start[iCurLine*16+i]);
    }
    for(left = iCurMaxCharNum; left < 16; left++)
    {
      sprintf(szLineBuf+OFFSET_HEX+left*3, "   ");
    }

    sprintf(szLineBuf+OFFSET_HEX+16*3, "    ");
    

    for(j = 0; j < iCurMaxCharNum; j++)
    {
      if(start[iCurLine*16+j] >=33 && start[iCurLine*16+j] <=128)
      {
        szLineBuf[OFFSET_CHAR+j] = start[iCurLine*16+j];
      }
      else
      {
        szLineBuf[OFFSET_CHAR+j] = '.';
      }
    }
    fprintf(g_fout, "%s\n", szLineBuf);
  }
  
  fflush(g_fout);
  
}

const char* binToHex(const unsigned char* start, int len)
{
    static char buf[10240] = {0};
    int off = 0;

    for(int i = 0; i < len; i++)
    {
        off += sprintf(buf+off, "%02x ", start[i]);
    }

    return buf;
}

const char* timeToStr(const time_t& time)
{
  static char szTimeString[20] = {0};
  tm* today = localtime(&time);
  strftime( szTimeString, 20, "%y-%m-%d %H:%M:%S", today );

  return szTimeString;
}

