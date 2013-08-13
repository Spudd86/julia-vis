#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "glx_gen.h"

#if defined(__APPLE__)
#include <mach-o/dyld.h>

static void* AppleGLGetProcAddress (const GLubyte *name)
{
  static const struct mach_header* image = NULL;
  NSSymbol symbol;
  char* symbolName;
  if (NULL == image)
  {
    image = NSAddImage("/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL", NSADDIMAGE_OPTION_RETURN_ON_ERROR);
  }
  /* prepend a '_' for the Unix C symbol mangling convention */
  symbolName = malloc(strlen((const char*)name) + 2);
  strcpy(symbolName+1, (const char*)name);
  symbolName[0] = '_';
  symbol = NULL;
  /* if (NSIsSymbolNameDefined(symbolName))
	 symbol = NSLookupAndBindSymbol(symbolName); */
  symbol = image ? NSLookupSymbolInImage(image, symbolName, NSLOOKUPSYMBOLINIMAGE_OPTION_BIND | NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR) : NULL;
  free(symbolName);
  return symbol ? NSAddressOfSymbol(symbol) : NULL;
}
#endif /* __APPLE__ */

#if defined(__sgi) || defined (__sun)
#include <dlfcn.h>
#include <stdio.h>

static void* SunGetProcAddress (const GLubyte* name)
{
  static void* h = NULL;
  static void* gpa;

  if (h == NULL)
  {
    if ((h = dlopen(NULL, RTLD_LAZY | RTLD_LOCAL)) == NULL) return NULL;
    gpa = dlsym(h, "glXGetProcAddress");
  }

  if (gpa != NULL)
    return ((void*(*)(const GLubyte*))gpa)(name);
  else
    return dlsym(h, (const char*)name);
}
#endif /* __sgi || __sun */

#if defined(_WIN32)

#ifdef _MSC_VER
#pragma warning(disable: 4055)
#pragma warning(disable: 4054)
#endif

static int TestPointer(const PROC pTest)
{
	ptrdiff_t iTest;
	if(!pTest) return 0;
	iTest = (ptrdiff_t)pTest;
	
	if(iTest == 1 || iTest == 2 || iTest == 3 || iTest == -1) return 0;
	
	return 1;
}

static PROC WinGetProcAddress(const char *name)
{
	HMODULE glMod = NULL;
	PROC pFunc = wglGetProcAddress((LPCSTR)name);
	if(TestPointer(pFunc))
	{
		return pFunc;
	}
	glMod = GetModuleHandleA("OpenGL32.dll");
	return (PROC)GetProcAddress(glMod, (LPCSTR)name);
}
	
#define IntGetProcAddress(name) WinGetProcAddress(name)
#else
	#if defined(__APPLE__)
		#define IntGetProcAddress(name) AppleGLGetProcAddress(name)
	#else
		#if defined(__sgi) || defined(__sun)
			#define IntGetProcAddress(name) SunGetProcAddress(name)
		#else /* GLX */
		    #include <GL/glx.h>

			#define IntGetProcAddress(name) (*glXGetProcAddressARB)((const GLubyte*)name)
		#endif
	#endif
#endif

int glx_ext_OML_sync_control = glx_LOAD_FAILED;
int glx_ext_ARB_create_context = glx_LOAD_FAILED;

Bool (CODEGEN_FUNCPTR *_ptrc_glXGetMscRateOML)(Display *, GLXDrawable, int32_t *, int32_t *) = NULL;
Bool (CODEGEN_FUNCPTR *_ptrc_glXGetSyncValuesOML)(Display *, GLXDrawable, int64_t *, int64_t *, int64_t *) = NULL;
int64_t (CODEGEN_FUNCPTR *_ptrc_glXSwapBuffersMscOML)(Display *, GLXDrawable, int64_t, int64_t, int64_t) = NULL;
Bool (CODEGEN_FUNCPTR *_ptrc_glXWaitForMscOML)(Display *, GLXDrawable, int64_t, int64_t, int64_t, int64_t *, int64_t *, int64_t *) = NULL;
Bool (CODEGEN_FUNCPTR *_ptrc_glXWaitForSbcOML)(Display *, GLXDrawable, int64_t, int64_t *, int64_t *, int64_t *) = NULL;

static int Load_OML_sync_control()
{
	int numFailed = 0;
	_ptrc_glXGetMscRateOML = (Bool (CODEGEN_FUNCPTR *)(Display *, GLXDrawable, int32_t *, int32_t *))IntGetProcAddress("glXGetMscRateOML");
	if(!_ptrc_glXGetMscRateOML) numFailed++;
	_ptrc_glXGetSyncValuesOML = (Bool (CODEGEN_FUNCPTR *)(Display *, GLXDrawable, int64_t *, int64_t *, int64_t *))IntGetProcAddress("glXGetSyncValuesOML");
	if(!_ptrc_glXGetSyncValuesOML) numFailed++;
	_ptrc_glXSwapBuffersMscOML = (int64_t (CODEGEN_FUNCPTR *)(Display *, GLXDrawable, int64_t, int64_t, int64_t))IntGetProcAddress("glXSwapBuffersMscOML");
	if(!_ptrc_glXSwapBuffersMscOML) numFailed++;
	_ptrc_glXWaitForMscOML = (Bool (CODEGEN_FUNCPTR *)(Display *, GLXDrawable, int64_t, int64_t, int64_t, int64_t *, int64_t *, int64_t *))IntGetProcAddress("glXWaitForMscOML");
	if(!_ptrc_glXWaitForMscOML) numFailed++;
	_ptrc_glXWaitForSbcOML = (Bool (CODEGEN_FUNCPTR *)(Display *, GLXDrawable, int64_t, int64_t *, int64_t *, int64_t *))IntGetProcAddress("glXWaitForSbcOML");
	if(!_ptrc_glXWaitForSbcOML) numFailed++;
	return numFailed;
}

GLXContext (CODEGEN_FUNCPTR *_ptrc_glXCreateContextAttribsARB)(Display *, GLXFBConfig, GLXContext, Bool, const int *) = NULL;

static int Load_ARB_create_context()
{
	int numFailed = 0;
	_ptrc_glXCreateContextAttribsARB = (GLXContext (CODEGEN_FUNCPTR *)(Display *, GLXFBConfig, GLXContext, Bool, const int *))IntGetProcAddress("glXCreateContextAttribsARB");
	if(!_ptrc_glXCreateContextAttribsARB) numFailed++;
	return numFailed;
}

typedef int (*PFN_LOADFUNCPOINTERS)();
typedef struct glx_StrToExtMap_s
{
	char *extensionName;
	int *extensionVariable;
	PFN_LOADFUNCPOINTERS LoadExtension;
} glx_StrToExtMap;

static glx_StrToExtMap ExtensionMap[2] = {
	{"GLX_OML_sync_control", &glx_ext_OML_sync_control, Load_OML_sync_control},
	{"GLX_ARB_create_context", &glx_ext_ARB_create_context, Load_ARB_create_context},
};

static int g_extensionMapSize = 2;

static glx_StrToExtMap *FindExtEntry(const char *extensionName)
{
	int loop;
	glx_StrToExtMap *currLoc = ExtensionMap;
	for(loop = 0; loop < g_extensionMapSize; ++loop, ++currLoc)
	{
		if(strcmp(extensionName, currLoc->extensionName) == 0)
			return currLoc;
	}
	
	return NULL;
}

static void ClearExtensionVars()
{
	glx_ext_OML_sync_control = glx_LOAD_FAILED;
	glx_ext_ARB_create_context = glx_LOAD_FAILED;
}


static void LoadExtByName(const char *extensionName)
{
	glx_StrToExtMap *entry = NULL;
	entry = FindExtEntry(extensionName);
	if(entry)
	{
		if(entry->LoadExtension)
		{
			int numFailed = entry->LoadExtension();
			if(numFailed == 0)
			{
				*(entry->extensionVariable) = glx_LOAD_SUCCEEDED;
			}
			else
			{
				*(entry->extensionVariable) = glx_LOAD_SUCCEEDED + numFailed;
			}
		}
		else
		{
			*(entry->extensionVariable) = glx_LOAD_SUCCEEDED;
		}
	}
}


static void ProcExtsFromExtString(const char *strExtList)
{
	size_t iExtListLen = strlen(strExtList);
	const char *strExtListEnd = strExtList + iExtListLen;
	const char *strCurrPos = strExtList;
	char strWorkBuff[256];

	while(*strCurrPos)
	{
		/*Get the extension at our position.*/
		int iStrLen = 0;
		const char *strEndStr = strchr(strCurrPos, ' ');
		int iStop = 0;
		if(strEndStr == NULL)
		{
			strEndStr = strExtListEnd;
			iStop = 1;
		}

		iStrLen = (int)((ptrdiff_t)strEndStr - (ptrdiff_t)strCurrPos);

		if(iStrLen > 255)
			return;

		strncpy(strWorkBuff, strCurrPos, iStrLen);
		strWorkBuff[iStrLen] = '\0';

		LoadExtByName(strWorkBuff);

		strCurrPos = strEndStr + 1;
		if(iStop) break;
	}
}

int glx_LoadFunctions(Display *display, int screen)
{
	ClearExtensionVars();
	
	
	ProcExtsFromExtString((const char *)glXQueryExtensionsString(display, screen));
	return glx_LOAD_SUCCEEDED;
}

