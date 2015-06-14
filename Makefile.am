include $(top_srcdir)/Makefile.all.am
#----------------------------------------------------------------------------
# libllvmcodegen-<platform>.a
#----------------------------------------------------------------------------

BUILT_SOURCES = $(mach_user_srcs)
CLEANFILES    = $(mach_user_srcs) $(mach_server_srcs) $(mach_hdrs)

pkglib_LIBRARIES  = libllvmcodegen-@VGCONF_ARCH_PRI@-@VGCONF_OS@.a

LLVMCODEGEN_SOURCES_COMMON = \
	llvm/InitializeLLVM.cpp \
	llvm/LLVMAPI.cpp \
	llvm/log.cpp \
	llvm/CompilerState.cpp \
	llvm/IntrinsicRepository.cpp \
	llvm/CommonValues.cpp \
	llvm/Output.cpp \
	llvm/Compile.cpp \
	llvm/StackMaps.cpp \
	llvm/VexTranslator.cpp \
	llvm/Link.cpp \
	llvm/log.cpp \
	isel_amd64.cpp

AM_CPPFLAGS_@VGCONF_PLATFORM_PRI_CAPS@ += \
	-I$(top_srcdir)/llvm-codegen/llvm

AM_CFLAGS_@VGCONF_PLATFORM_PRI_CAPS@ = \
	-DLLVMLOG_LEVEL=0 \
	-g

libllvmcodegen_@VGCONF_ARCH_PRI@_@VGCONF_OS@_a_SOURCES = \
    $(LLVMCODEGEN_SOURCES_COMMON)
nodist_libllvmcodegen_@VGCONF_ARCH_PRI@_@VGCONF_OS@_a_SOURCES = \
    $(BUILT_SOURCES)
libllvmcodegen_@VGCONF_ARCH_PRI@_@VGCONF_OS@_a_CPPFLAGS = \
    $(AM_CPPFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
libllvmcodegen_@VGCONF_ARCH_PRI@_@VGCONF_OS@_a_CFLAGS = \
    $(AM_CFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
libllvmcodegen_@VGCONF_ARCH_PRI@_@VGCONF_OS@_a_CCASFLAGS = \
    $(AM_CCASFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
libllvmcodegen_@VGCONF_ARCH_PRI@_@VGCONF_OS@_a_CXXFLAGS = \
    $(AM_CFLAGS_@VGCONF_PLATFORM_PRI_CAPS@) \
	-std=c++11

bin_PROGRAMS = \
	testIR

testIR_SOURCES = \
	test/IRContext.cpp \
	test/main.cpp \
	test/Helpers.cpp \
	test/RegisterAssign.cpp \
	test/RegisterInit.cpp \
	test/RegisterOperation.cpp \
	test/Check.cpp \
	test/IRParser.y \
	test/IRScanner.l \
	test/IRContextInternal.cpp \
	test/dispatch_vex.S

testIR_CPPFLAGS = \
				 $(AM_CPPFLAGS_@VGCONF_PLATFORM_PRI_CAPS@) \
				 -I$(top_srcdir)/llvm-codegen/test

testIR_CXXFLAGS = \
    $(AM_CFLAGS_@VGCONF_PLATFORM_PRI_CAPS@) \
	-std=c++11

testIR_LDADD = \
	$(top_builddir)/VEX/libvex-@VGCONF_ARCH_PRI@-@VGCONF_OS@.a \
	libllvmcodegen-@VGCONF_ARCH_PRI@-@VGCONF_OS@.a \
	$(LLVM_LIBS) -lz -lpthread -ldl -lcurses
