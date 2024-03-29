# This file is part of Pazpar2.
# Copyright (C) Index Data
# See the file LICENSE for details.

DEBUG=0   # 0 for release, 1 for debug
BARCH = 32

PACKAGE_NAME=pazpar2
PACKAGE_VERSION=1.14.1

# YAZ
YAZ_DIR=..\..\yaz

# Libxml2 + libxslt + associates
LIBXML2_DIR=\libxml2-2.9.2.win$(BARCH)
LIBXSLT_DIR=\libxslt-1.1.28.win$(BARCH)

# ICU
HAVE_ICU=1
ICU_VER=68
ICU_DIR=c:\icu4c-$(ICU_VER)_2-Win$(BARCH)-MSVC2019
ICU_INCLUDE=$(ICU_DIR)\include
!if $(BARCH) == 32
ICU_LIB=$(ICU_DIR)\lib
ICN_BIN=$(ICU_DIR)\bin
!elseif $(BARCH) == 64
ICU_LIB=$(ICU_DIR)\lib64
ICN_BIN=$(ICU_DIR)\bin64
!else
!error BARCH must be 32 or 64
!endif

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
YAZ_LIB="$(YAZ_DIR)\lib\yaz5d.lib" "$(YAZ_DIR)\lib\yaz_cond5d.lib" 
!if $(HAVE_ICU)
YAZ_LIB=$(YAZ_LIB) "$(YAZ_DIR)\lib\yaz_icu5d.lib"
!endif
YAZ_DLL_SOURCE="$(YAZBINDIR)\yaz5d.dll"
YAZ_DLL_TARGET="$(BINDIR)\yaz5d.dll"
!else
YAZ_LIB="$(YAZ_DIR)\lib\yaz5.lib" "$(YAZ_DIR)\lib\yaz_cond5.lib"
!if $(HAVE_ICU)
YAZ_LIB=$(YAZ_LIB) "$(YAZ_DIR)\lib\yaz_icu5.lib"
!endif
YAZ_DLL_SOURCE="$(YAZBINDIR)\yaz5.dll"
YAZ_DLL_TARGET="$(BINDIR)\yaz5.dll"
!endif

YAZ_DEF=/DYAZ_HAVE_XML2=2 /DYAZ_HAVE_XSLT=1 /I"$(YAZ_DIR)\include"

# Copy libxslt and associates to our bin directory
libxslt: $(BINDIR)\libxslt.dll

$(BINDIR)\libxslt.dll: "$(LIBXSLT_DIR)\lib\libxslt.dll"
	copy "$(LIBXML2_DIR)\bin\libxml2.dll" $(BINDIR)
	copy "$(LIBXSLT_DIR)\lib\libxslt.dll" $(BINDIR)


LIBXSLT_DEF= \
	    /I"$(LIBXML2_DIR)\include\libxml2" \
	    /I"$(LIBXSLT_DIR)\include"

LIBXSLT_LIB = \
	   "$(LIBXML2_DIR)\lib\libxml2.lib" \
	   "$(LIBXSLT_DIR)\lib\libxslt.lib"

!if $(HAVE_ICU)
ICU_DEF=/DYAZ_HAVE_ICU=1 /I"$(ICU_INCLUDE)"
ICU_LIBS= $(ICU_LIB)\icudt.lib \
	 $(ICU_LIB)\icuin.lib \
	 $(ICU_LIB)\icuuc.lib
!else
ICU_DEF=/DYAZ_HAVE_ICU=0
ICU_LIBS=
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
           $(YAZ_LIB) $(ICU_LIBS) $(LIBXSLT_LIB)

COMMON_LNK_OPTIONS= /nologo /subsystem:windows /incremental:no

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


LINK_PROGRAM = $(LINK) $(LNKOPT) $(CLIENT_LINK_OPTIONS) $(LINK_LIBS)


$(PAZPAR2_EXE) : "$(BINDIR)" $(PAZPAR2_OBJS) $(YAZ_LIB)
	$(LINK_PROGRAM) $(PAZPAR2_OBJS) /out:$@

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

