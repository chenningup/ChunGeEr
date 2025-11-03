#ifndef  TESSERACTTRAIN_H
#  define TESSERACTTRAIN_H

#include <string>
#define MYLIB_EXPORTS 1
#pragma once
#ifdef MYLIB_EXPORTS // 这个宏需要在编译动态库时通过CMake定义
#  define MYLIB_API __declspec(dllexport)
#else
#  define MYLIB_API __declspec(dllimport)
#endif

class MYLIB_API TesseractTrain 
{
public:
  TesseractTrain();
  ~TesseractTrain();
	
  void setLang(const std::string &value) {
    lang = value;
  };
  void setImage(const std::string &value) {
    image = value;
  };
  void setOutputbase(const std::string &value) {
    outputbase = value;
  };
    
  void setPageMode(int mode) {
    pageMode = mode;
  }

    void setConfig(const std::string &value) {
    config = value;
  }
  int doTask();

private:
  std::string lang;
  std::string image;
  std::string outputbase;
  int pageMode;
  std::string config;
};



#endif