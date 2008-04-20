# This file is part of Pazpar2.
# Copyright (C) 2006-2008 Index Data

DEBUG=1   # 0 for release, 1 for debug
USE_MANIFEST = 0 # Can be enabled Visual Studio 2005
PACKAGE_NAME=pazpar2
PACKAGE_VERSION=1.0.7

# YAZ
YAZ_DIR=..\..\yaz

# Libxml2 + libxslt + associates
ZLIB_DIR=\zlib-1.2.3.win32
ICONV_DIR=\iconv-1.9.2.win32
LIBXML2_DIR=\libxml2-2.6.28.win32
LIBXSLT_DIR=\libxslt-1.1.19.win32

# ICU
HAVE_ICU=1
ICU_DIR=c:\icu

default: all

all: dirs yaz libxslt pazpar2

NSIS="c:\program files\nsis\makensis-bz2.exe"

nsis: all
	$(NSIS) yazpp.nsi

nsishelp:
	$(NSIS)

# Directories
ROOTDIR=..

BINDIR=$(ROOTDIR)\bin       # We produce exes and dlls there
WINDIR=$(ROOTDIR)\win       # all these Win make things
SRCDIR=$(ROOTDIR)\src       # for the case we move them under src

!if $(DEBUG)
OBJDIR=$(WINDIR)\dobj       # debug objects
YAZD=yaz3d
!else
OBJDIR=$(WINDIR)\obj        # non-debug objects
YAZD=yaz3
!endif

TMPDIR=$(ROOTDIR)\win\tmp
TMP=$(TMPDIR)

PAZPAR2_EXE=$(BINDIR)\pazpar2.exe

pazpar2: $(PAZPAR2_EXE)

# YAZ includes & libs
YAZ_LIB="$(YAZ_DIR)\lib\$(YAZD).lib"
YAZ_DEF=/DYAZ_HAVE_XML2=2 /DYAZ_HAVE_XSLT=1 /I"$(YAZ_DIR)\include"
yaz: $(BINDIR)\$(YAZD).dll 

$(BINDIR)\$(YAZD).dll: "$(YAZ_DIR)\bin\$(YAZD).dll"
  copy "$(YAZ_DIR)\bin\icu*.dll" $(BINDIR)
  copy "$(YAZ_DIR)\bin\$(YAZD).dll" $(BINDIR)

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
ICU_DEF=/DHAVE_ICU=1 /D HAVE_ICU_H=1 /I"$(ICU_DIR)\include"
ICU_LIB= $(ICU_DIR)\lib\icudt.lib \
	 $(ICU_DIR)\lib\icuin.lib \
	 $(ICU_DIR)\lib\icuuc.lib
!else
ICU_DEF=/DHAVE_ICU=0
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

LINK_LIBS= kernel32.lib user32.lib gdi32.lib advapi32.lib uuid.lib \
           wsock32.lib  advapi32.lib \
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
   "$(OBJDIR)\pazpar2.obj" \
   "$(OBJDIR)\pazpar2_config.obj" \
   "$(OBJDIR)\http.obj" \
   "$(OBJDIR)\eventl.obj" \
   "$(OBJDIR)\http_command.obj" \
   "$(OBJDIR)\icu_I18N.obj" \
   "$(OBJDIR)\logic.obj" \
   "$(OBJDIR)\record.obj" \
   "$(OBJDIR)\reclists.obj" \
   "$(OBJDIR)\relevance.obj" \
   "$(OBJDIR)\termlists.obj" \
   "$(OBJDIR)\normalize7bit.obj" \
   "$(OBJDIR)\util.obj" \
   "$(OBJDIR)\zeerex.obj" \
   "$(OBJDIR)\database.obj" \
   "$(OBJDIR)\settings.obj" \
   "$(OBJDIR)\getaddrinfo.obj" \
   "$(OBJDIR)\charsets.obj" \
   "$(OBJDIR)\client.obj" \
   "$(OBJDIR)\dirent.obj" \
   "$(OBJDIR)\connection.obj" 


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

