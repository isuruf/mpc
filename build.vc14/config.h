
#if defined ( _WINDLL ) || defined( __GMP_LIBGMP_DLL )
#  include "..\..\mpir\dll\config.h"
#else
#  include "..\..\mpir\lib\config.h"
#endif

#define PACKAGE_STRING "mpc" 
#define PACKAGE_VERSION "0.9-rc1" 
#define STDC_HEADERS 1 
#define MPC_CC MSC_VERSION
#define dlsym(handle, name) GetProcAddress(GetModuleHandle(handle), name) 
#include "windows.h" 
