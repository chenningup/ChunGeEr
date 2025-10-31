#include <string>
#define MYLIB_EXPORTS 1
#pragma once
#ifdef MYLIB_EXPORTS // 这个宏需要在编译动态库时通过CMake定义
#  define MYLIB_API __declspec(dllexport)
#else
#  define MYLIB_API __declspec(dllimport)
#endif
#include <cerrno>
#include <locale> // for std::locale::classic
#if defined(__USE_GNU)
#  include <cfenv> // for feenableexcept
#endif
#include "commontraining.h"
#include "fileio.h" // for LoadFileLinesToStrings
#include "lstmtester.h"
#include "lstmtrainer.h"
#include "params.h"
#include "tprintf.h"
#include "unicharset_training_utils.h"
#include <cstdlib>



using namespace tesseract;
class MYLIB_API lstmtraining {
public:
	lstmtraining(){};
	~lstmtraining(){};
	
		void set_model_output(const std::string&value);
        void set_continue_from(const std::string &value);
		void set_traineddata(const std::string &value);
        void set_max_iterations(int value);
        void set_target_error_rate(double value);
        void set_stop_training(bool stop);

		int startTrain();
};
