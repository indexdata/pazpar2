# This file is part of Pazpar2.
# Copyright (C) 2006-2012 Index Data

DEBUG=0   # 0 for release, 1 for debug
USE_MANIFEST = 1 # Can be enabled Visual Studio 2005/2008
PACKAGE_NAME=pazpar2
PACKAGE_VERSION=1.6.7

# YAZ
YAZ_DIR=..\..\yaz

# Libxml2 + libxslt + associates
ZLIB_DIR=\zlib-1.2.3.win32
ICONV_DIR=\iconv-1.9.2.win32
LIBXML2_DIR=\libxml2-2.7.6.win32
LIBXSLT_DIR=\libxslt-1.1.26.win32

# ICU
HAVE_ICU=1
ICU_DIR=c:\icu

default: all

all: dirs yaz libxslt pazpar2

NSIS="c:\program files\nsis\makensis.exe"

dist: pazpar2.nsi
	nmake DEBUG=1 clean
	nmake DEBUG=0 clean
	nmake DEBUG=0
	$(NSIS) pazpar2.nsi

nsis: all
	$(NSIS) pazpar2.nsi

nsishelp:
	$(NSIS)

# Directories
ROOTDIR=..

BINDIR=$(ROOTDIR)\bin       # We produce exes and dlls there
WINDIR=$(ROOTDIR)\win       # all these Win make things
SRCDIR=$(ROOTDIR)\src       # for the case we move them under src

!if $(DEBUG)
OBJDIR=$(WINDIR)\dobj
!else
OBJDIR=$(WINDIR)\obj
!endif

TMPDIR=$(ROOTDIR)\win\tmp
TMP=$(TMPDIR)

PAZPAR2_EXE=$(BINDIR)\pazpar2.exe

pazpar2: $(PAZPAR2_EXE)

YAZBINDIR=$(YAZ_DIR)\bin
!if $(DEBUG)
YAZ_LIB="$(YAZ_DIR)\lib\yaz4d.lib" "$(YAZ_DIR)\lib\yaz_cond4d.lib" 
!if $(HAVE_ICU)
YAZ_LIB=$(YAZ_LIB) "$(YAZ_DIR)\lib\yaz_icu4d.lib"
!endif
YAZ_DLL_SOURCE="$(YAZBINDIR)\yaz4d.dll"
YAZ_DLL_TARGET="$(BINDIR)\yaz4d.dll"
!else
YAZ_LIB="$(YAZ_DIR)\lib\yaz4.lib" "$(YAZ_DIR)\lib\yaz_cond4.lib"
!if $(HAVE_ICU)
YAZ_LIB=$(YAZ_LIB) "$(YAZ_DIR)\lib\yaz_icu4.lib"
!endif
YAZ_DLL_SOURCE="$(YAZBINDIR)\yaz4.dll"
YAZ_DLL_TARGET="$(BINDIR)\yaz4.dll"
!endif

YAZ_DEF=/DYAZ_HAVE_XML2=2 /DYAZ_HAVE_XSLT=1 /I"$(YAZ_DIR)\include"

# Copy libxslt and associates to our bin directory
libxslt: $(BINDIR)\libxslt.dll

$(BINDIR)\libxslt.dll: "$(LIBXSLT_DIR)\bin\libxslt.dll"
	copy "$(ICONV_DIR)\bin\iconv.dll" $(BINDIR)
	copy "$(ZLIB_DIR)\bin\zlib1.dll" $(BINDIR)
	copy "$(LIBXML2_DIR)\bin\libxml2.dll" $(BINDIR)
	copy "$(LIBXSLT_DIR)\bin\libxslt.dll" $(BINDIR)


LIBXSLT_DEF= \
	    /I"$(ICONV_DIR)\include" \
	    /I"$(LIBXML2_DIR)\include" \
	    /I"$(LIBXSLT_DIR)\include"

LIBXSLT_LIB = \
	   "$(ICONV_DIR)\lib\iconv.lib" \
	   "$(LIBXML2_DIR)\lib\libxml2.lib" \
	   "$(LIBXSLT_DIR)\lib\libxslt.lib"

!if $(HAVE_ICU)
ICU_DEF=/DYAZ_HAVE_ICU=1 /I"$(ICU_DIR)\include"
ICU_LIB= $(ICU_DIR)\lib\icudt.lib \
	 $(ICU_DIR)\lib\icuin.lib \
	 $(ICU_DIR)\lib\icuuc.lib
!else
ICU_DEF=/DYAZ_HAVE_ICU=0
ICU_LIB=
!endif

COMMON_C_OPTIONS=          \
  /nologo /W3 /EHsc /FD /c \
  /D "_CRT_SECURE_NO_DEPRECATE" \
  /D "PACKAGE_VERSION=\"$(PACKAGE_VERSION)\"" \
  /D "PACKAGE_NAME=\"$(PACKAGE_NAME)\"" \
  /D "VERSION=\"$(PACKAGE_VERSION)\"" \
  $(YAZ_DEF)               \
  $(ICU_DEF)               \
  $(LIBXSLT_DEF)           \
  /D "_WINDOWS"            \
  /D "WIN32"               \
  /D "HAVE_WCHAR_H=1"      \
  /FR"$(OBJDIR)\\"         \
  /Fo"$(OBJDIR)\\"         \
  /Fd"$(OBJDIR)\\" 

COMMON_C_INCLUDES= \
  /I"$(ROOTDIR)\include"

DEBUG_C_OPTIONS=  \
  /D "_DEBUG"      \
  /MDd  /Od /Zi /Gm

RELEASE_C_OPTIONS=  \
  /D "NDEBUG"        \
  /MD /O2

# /W3  = warning level
# /GX  = Enable exception handling
# /FD  = Generate file dependencies (what ever they are)
# /c   = compile without linking
# /FR  = Generate browse info (.sbr file that gets combined into .bsc)
# /Fo  = object file name (or at least path)
# /Fd  = debug database name (or path)
# /MD  = Runtime library: Multithread DLL
# /MDd = Runtime library: Multithread DLL (debug)
# /Od  = Disable optimising (debug)
# /O2  = Optimize for speed
# /Gm  = Minimal rebuild (some cpp class stuff)
# /Zi  = Program database for debuggers
# /ZI  = Pgm database with special "edit&continue" stuff - not available in C5


### Linker options
LINK=link.exe

LINK_LIBS= kernel32.lib ws2_32.lib advapi32.lib \
           $(YAZ_LIB) $(ICU_LIB) $(LIBXSLT_LIB)

COMMON_LNK_OPTIONS= /nologo /subsystem:windows /machine:i386 /incremental:no

DEBUG_LNK_OPTIONS= /debug 

RELEASE_LNK_OPTIONS=

CLIENT_LINK_OPTIONS = /subsystem:console  

# Final opt variables
!if $(DEBUG)
COPT=   $(COMMON_C_OPTIONS)   $(DEBUG_C_OPTIONS)     $(COMMON_C_INCLUDES)
RCOPT=  $(COMMON_RC_OPTIONS)  $(DEBUG_RC_OPTIONS)
LNKOPT= $(COMMON_LNK_OPTIONS) $(DEBUG_LNK_OPTIONS)   $(LNK_LIBS)

!else
COPT=   $(COMMON_C_OPTIONS)   $(RELEASE_C_OPTIONS)   $(COMMON_C_INCLUDES) 
RCOPT=  $(COMMON_RC_OPTIONS)  $(RELEASE_RC_OPTIONS)
LNKOPT= $(COMMON_LNK_OPTIONS) $(RELEASE_LNK_OPTIONS) $(LNK_LIBS)
!endif

# Source and object modules

PAZPAR2_OBJS = \
   "$(OBJDIR)\getaddrinfo.obj" \
   "$(OBJDIR)\host.obj" \
   "$(OBJDIR)\pazpar2.obj" \
   "$(OBJDIR)\pazpar2_config.obj" \
   "$(OBJDIR)\http.obj" \
   "$(OBJDIR)\eventl.obj" \
   "$(OBJDIR)\http_command.obj" \
   "$(OBJDIR)\session.obj" \
   "$(OBJDIR)\record.obj" \
   "$(OBJDIR)\reclists.obj" \
   "$(OBJDIR)\relevance.obj" \
   "$(OBJDIR)\termlists.obj" \
   "$(OBJDIR)\normalize7bit.obj" \
   "$(OBJDIR)\database.obj" \
   "$(OBJDIR)\settings.obj" \
   "$(OBJDIR)\charsets.obj" \
   "$(OBJDIR)\client.obj" \
   "$(OBJDIR)\jenkins_hash.obj" \
   "$(OBJDIR)\marcmap.obj" \
   "$(OBJDIR)\marchash.obj" \
   "$(OBJDIR)\normalize_record.obj" \
   "$(OBJDIR)\normalize_cache.obj" \
   "$(OBJDIR)\ppmutex.obj" \
   "$(OBJDIR)\incref.obj" \
   "$(OBJDIR)\sel_thread.obj" \
   "$(OBJDIR)\service_xslt.obj" \
   "$(OBJDIR)\connection.obj"  \
   "$(OBJDIR)\facet_limit.obj" 


{$(SRCDIR)}.c{$(OBJDIR)}.obj:
	$(CPP) $(COPT) $< 


!if $(USE_MANIFEST)
MT=mt.exe /nologo
!else
MT=echo
!endif


LINK_PROGRAM = $(LINK) $(LNKOPT) $(CLIENT_LINK_OPTIONS) $(LINK_LIBS)


$(PAZPAR2_EXE) : "$(BINDIR)" $(PAZPAR2_OBJS) $(YAZ_LIB)
	$(LINK_PROGRAM) $(PAZPAR2_OBJS) /out:$@
	$(MT) -manifest $@.manifest -outputresource:$@;1

clean:
	-del $(BINDIR)\*.exe
	-del $(TMPDIR)\*.
	-del $(OBJDIR)\*.OBJ

dirs: $(OBJDIR) $(WINDIR) $(BINDIR) $(TMPDIR)

$(OBJDIR) $(WINDIR) $(BINDIR) $(TMPDIR):
	if not exist "$@/$(NUL)" mkdir "$@"

yaz: $(YAZ_DLL_TARGET)

$(YAZ_DLL_TARGET) : $(YAZ_DLL_SOURCE)
#	copy "$(YAZBINDIR)\*.dll.manifest" $(BINDIR)
	copy "$(YAZBINDIR)\*.dll" $(BINDIR)

