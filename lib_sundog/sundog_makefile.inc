##
## Defined in the app MakeFile:
##   TARGET_OS
##   TARGET_ARCH - architecture name: arm, arm64, x86, x86_64, mips, mips64; NOT ABI NAME! ABI can be selected by some additional options;
##   ROOT
##   SUNDOG - base library (engine)
##   SUNDOG_DEFINES
##   APPNAME
##   LIBS - additional libraries; for example: -lGL
##

##
## Make options:
## (example: MAKE_WITHOUT_SIMD=true)
##   MAKE_DYNAMIC_LIB
##   MAKE_STATIC_LIB
##   MAKE_MODULE (vst3)
##   MAKE_WITH_OBJDUMP
##   MAKE_WITH_CONSOLE
##   MAKE_WITH_MULTITOUCH
##   MAKE_WITH_SDL (1 for SDL1.2 or 2 for SDL2)
##   MAKE_WITHOUT_MAIN
##   MAKE_WITHOUT_WM
##   MAKE_WITHOUT_SIMD
##   MAKE_FOR_SLOW_CPU
##   MAKE_FOR_NEWEST_CPU (for example, this option may enable SSSE3 instead of SSE3)
##

##
## Paths
##

ANDROID_NDK = /home/alex/soft/android-ndk-r22b
MACOS_TOOLCHAIN_BIN = /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin
MACOS_SDK = /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX13.1.sdk
VST3_SDK = /home/alex/soft/VST_SDK/vst3sdk
VST3_SDK_BASE = $(VST3_SDK)/base
VST3_SDK_IF = $(VST3_SDK)/pluginterfaces
VST3_SDK_HELPER = $(VST3_SDK)/public.sdk/source
IOS_TOOLCHAIN_BIN = /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin
ifneq (,$(findstring x86,$(TARGET_ARCH)))
    IOS_SDK = /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator16.2.sdk
else
    IOS_SDK = /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS16.2.sdk
endif
RPI_SDK = /opt/vc

##
## Target configuration
##

# LDFLAGS - must be set by user
# CFLAGS - must be set by user
CFLAGS2 = 
LDFLAGS2 = 
DEFAULT_LIBS = 

TARGET_ABI = $(TARGET_ARCH)

##
## Linux
##
ifneq (,$(findstring linux,$(TARGET_OS)))
    CC = gcc
    CXX = $(CC)
    LD = $(CC)
    OBJDUMP = objdump
    CFLAGS2 += -ffunction-sections -fdata-sections -Wall -Wno-unused-variable -Wno-multichar -pipe
#"-Wl,--gc-sections" works wrong when generating dynamic .so library? 
#./sunvox.so: undefined symbol: _Z23sunvox_record_write_intjP13sunvox_engine
#(при загрузке .so ошибка о ненайденной функции, которая не используется в коде и находится внутри другой неисп.функции psynth_handle_ctl_midi_in())
    LDFLAGS2 += -Wl,--gc-sections
    DEFAULT_LIBS = -lpthread -lstdc++ -lasound -lm -ldl
ifneq (,$(findstring x86,$(TARGET_ARCH)))
ifeq (,$(findstring android,$(TARGET_OS)))
#GCC produces PIE (Position Independent Executable) by default in Debian 9.
#14 jan 2018: It's ok, but ALL graphics file managers don't recognize such executables. So i disabled it temporarily:
    LDFLAGS2 += -no-pie
endif #not android
endif
##
## Linux: Raspberry PI (legacy)
##
ifneq (,$(findstring raspberry_pi,$(TARGET_OS)))
    CFLAGS2 += -DOS_RASPBERRY_PI -I$(RPI_SDK)/include -I$(RPI_SDK)/include/interface/vcos/pthreads -I$(RPI_SDK)/include/interface/vmcs_host/linux
    LDFLAGS2 += -L$(RPI_SDK)/lib
endif
##
## Linux: Maemo
##
ifneq (,$(findstring maemo,$(TARGET_OS)))
    CFLAGS2 += -DOS_MAEMO
endif
##
## Linux: Android
##
ifneq (,$(findstring android,$(TARGET_OS)))
    SYSROOT = $(ANDROID_NDK)/sysroot
    CFLAGS2 += \
	-DGL_GLEXT_PROTOTYPES=1 \
	-I$(SYSROOT)/usr/include \
	-I$(ANDROID_NDK)/sources/android/native_app_glue
    DEFAULT_LIBS = -llog -landroid -lOpenSLES -lm -ldl
ifeq ($(TARGET_ARCH),x86) 
    CC = $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/i686-linux-android16-clang
    CXX = $(CC)
    LD = $(CC)
    OBJDUMP = $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/i686-linux-android-objdump
    TARGET_ABI = x86
endif
ifeq ($(TARGET_ARCH),x86_64)
    CC = $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/x86_64-linux-android21-clang
    CXX = $(CC)
    LD = $(CC)
    OBJDUMP = $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/x86_64-linux-android-objdump
    TARGET_ABI = x86_64
endif
ifeq ($(TARGET_ARCH),arm)
    CC = $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/armv7a-linux-androideabi16-clang
    CXX = $(CC)
    LD = $(CC)
    OBJDUMP = $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/arm-linux-androideabi-objdump
    TARGET_ABI = armeabi-v7a
endif
ifeq ($(TARGET_ARCH),arm64)
    CC = $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android21-clang
    CXX = $(CC)
    LD = $(CC)
    OBJDUMP = $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android-objdump
    TARGET_ABI = arm64-v8a
endif
    MAKE_DYNAMIC_LIB = true
    MAKE_FOR_NEWEST_CPU = true
endif #android
endif #linux
##
## macOS
##
ifeq ($(TARGET_OS),macos)
    CC = $(MACOS_TOOLCHAIN_BIN)/clang
    CXX = $(CC) -x c++
    CMM = $(CC) -x objective-c++
    LD = $(CC)
    OBJDUMP = objdump
    CFLAGS2 += \
	-target $(TARGET_ARCH)-apple-macos10.12 \
	-Wno-multichar \
	-isysroot $(MACOS_SDK) \
	-pipe
    LDFLAGS2 += \
	-target $(TARGET_ARCH)-apple-macos10.12 \
	-isysroot $(MACOS_SDK)
    DEFAULT_LIBS = -lpthread -lstdc++ -framework Cocoa -framework Carbon -framework AudioUnit -framework CoreAudio
    MAKE_FOR_NEWEST_CPU = true
endif
##
## iOS
##
ifeq ($(TARGET_OS),ios)
    CC = $(IOS_TOOLCHAIN_BIN)/clang
    CXX = $(CC)
    CMM = $(CC) -x objective-c++
    LD = $(CC)
    OBJDUMP = $(IOS_TOOLCHAIN_BIN)/objdump
    LIBTOOL = $(IOS_TOOLCHAIN_BIN)/libtool -syslibroot $(IOS_SDK)
    CFLAGS2 += \
	-Wno-multichar \
	-isysroot $(IOS_SDK) \
	-miphoneos-version-min=11.0 \
	-fembed-bitcode \
	-pipe
    DEFAULT_LIBS = -framework Foundation -framework AudioToolbox -framework AVFoundation
    MAKE_FOR_NEWEST_CPU = true
endif
##
## Windows
##
ifneq (,$(findstring windows,$(TARGET_OS)))
ifneq ($(TARGET_OS),windowsce)
ifeq ($(TARGET_ARCH),x86_64) 
    CC = x86_64-w64-mingw32-gcc
    CXX = $(CC)
#   LD=g++ - this is required for option -static-libstdc++ to work!
    LD = x86_64-w64-mingw32-g++
    OBJDUMP = x86_64-w64-mingw32-objdump
    WINDRES = x86_64-w64-mingw32-windres
    FILE_EXT = .exe
    CFLAGS2 += \
	-ffunction-sections -fdata-sections \
	-fno-exceptions \
	-Wall -Wno-unused-variable -Wno-multichar \
	-pipe \
	-Iresources/
endif
ifeq ($(TARGET_ARCH),x86)
    CC = i686-w64-mingw32-gcc
    CXX = $(CC)
    LD = i686-w64-mingw32-g++
    OBJDUMP = i686-w64-mingw32-objdump
    WINDRES = i686-w64-mingw32-windres
    FILE_EXT = .exe
    CFLAGS2 += \
	-ffunction-sections -fdata-sections \
	-malign-double \
	-fno-exceptions \
	-Wall -Wno-unused-variable -Wno-multichar \
	-pipe \
	-Iresources/
endif
    LDFLAGS2 += -Wl,--gc-sections -static-libgcc -static-libstdc++
    DEFAULT_LIBS = -ldsound -ldxguid -lwinmm -lole32
#   -static-libgcc will remove the libgcc_s_sjlj-1 dependency (we don't use exceptions at all)
#   -static-libstdc++ will remove the libstdc++ dependency
endif
endif
##
## WindowsCE
##
ifeq ($(TARGET_OS),windowsce)
    CC = arm-mingw32ce-gcc
    CXX = $(CC)
    LD = $(CC)
    OBJDUMP = arm-mingw32ce-objdump
    WINDRES = arm-mingw32ce-windres
    FILE_EXT = .exe
    CFLAGS2 += \
	-ffunction-sections -fdata-sections \
	-fno-exceptions \
	-Wno-multichar \
	-pipe \
	-Iresources/ \
	-I$(SUNDOG)main/wince/
    LDFLAGS2 += -Wl,--gc-sections
    DEFAULT_LIBS = -laygshell -lceshell -lstdc++ -static
endif
##
## Emscripten
##
ifneq (,$(findstring emscripten,$(TARGET_OS)))
    CC = emcc
    CXX = $(CC) -std=c++11
    LD = $(CC)
    FILE_EXT = .html
    CFLAGS2 += \
	-fno-exceptions \
	-fno-rtti \
	-DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0 \
	-Wall -Wno-unused-variable -Wno-multichar -pipe
    LDFLAGS2 += -O3 -s ALLOW_MEMORY_GROWTH=1
    DEFAULT_LIBS = 
endif

##
## x86
##
ifeq ($(TARGET_ARCH),x86)
ifneq (,$(findstring android,$(TARGET_OS)))
    CFLAGS2 += -DCPUMARK=0 
else
ifeq ($(MAKE_FOR_SLOW_CPU),true)
    CFLAGS2 += -DCPUMARK=0 
else
    CFLAGS2 += -DCPUMARK=10 
endif
endif #not android
    CFLAGS2 += -m32 -ffast-math -march=i686
    LDFLAGS2 += -m32
    LIBTOOL_FLAGS = -static -arch_only i686
ifneq ($(MAKE_WITHOUT_SIMD),true)
ifneq (,$(findstring windows,$(TARGET_OS)))
#Filter Pro + ffast-math + march=pentium3 (min) crashes the Windows 10 machines; (i686-w64-mingw32-gcc 4.9.1)
#also it seems there is no big difference between the pure i686 and i686+sse+vectorize :)
#fix 15 jan 2018: works now with the i686-w64-mingw32-gcc 6.3.0
#                 but still there is no difference; furthermore, i686+sse+vectorize works slower sometimes (??)
else
ifeq ($(TARGET_OS),ios)
    CFLAGS2 += -mssse3 -ftree-vectorize
else
ifeq ($(MAKE_FOR_NEWEST_CPU),true)
    CFLAGS2 += -mfpmath=sse -mssse3 -ftree-vectorize
else
    CFLAGS2 += -mfpmath=sse -msse3 -ftree-vectorize
endif
endif #not ios
endif #not windows
endif #with SIMD
endif #x86
##
## x86_64
##
ifeq ($(TARGET_ARCH),x86_64) 
    CFLAGS2 += -m64 -ffast-math
ifneq ($(MAKE_WITHOUT_SIMD),true)
ifeq ($(MAKE_FOR_NEWEST_CPU),true)
    CFLAGS2 += -mssse3 -ftree-vectorize
else
    CFLAGS2 += -msse3 -ftree-vectorize
endif
endif
ifeq ($(MAKE_FOR_SLOW_CPU),true)
    CFLAGS2 += -DCPUMARK=0 
else
    CFLAGS2 += -DCPUMARK=10 
endif
    LDFLAGS2 += -m64
endif #x86_64
##
## ARM
##
## Some of these defines may be defined automatically below:
##   ARM_VFP - VFP used;
##   HARDFP - VFP + FPU-specific calling convention;
##
ifneq (,$(findstring arm,$(TARGET_ARCH)))
    CFLAGS2 += -DCPUMARK=0 -ffast-math
ifneq (,$(findstring android,$(TARGET_OS)))
ifeq ($(TARGET_ARCH),arm)
    ARM_DEFINES = -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16
    LDFLAGS2 += -march=armv7-a -Wl,--fix-cortex-a8
endif #arm
endif #android
ifneq (,$(findstring ios,$(TARGET_OS)))
    ARM_DEFINES = -arch $(TARGET_ARCH)
    LIBTOOL_FLAGS = -static -arch_only $(TARGET_ARCH)
endif #iOS
ifneq ($(MAKE_WITHOUT_SIMD),true)
    CFLAGS2 += -ftree-vectorize
endif
endif #arm
CFLAGS2 += $(ARM_DEFINES)
##
## Emscripten
##
ifeq ($(TARGET_ARCH),emscripten) 
    CFLAGS2 += -ffast-math
ifeq ($(MAKE_FOR_SLOW_CPU),true)
    CFLAGS2 += -DCPUMARK=0 
else
    CFLAGS2 += -DCPUMARK=10 
endif
endif

##
## Handling additional MAKE_xxx options
##

##
## All systems
##
ifeq ($(MAKE_MODULE),vst3)
    MAKE_DYNAMIC_LIB = true
    CFLAGS2 += \
	-I$(VST3_SDK) \
	-I$(VST3_SDK)/vstgui4 \
	-DRELEASE=1 \
	-DSUNDOG_MODULE \
	-DSUNDOG_MODULE_VST3
endif
ifeq ($(MAKE_WITH_MULTITOUCH),true)
    CFLAGS2 += -DMULTITOUCH
ifneq (,$(findstring linux,$(TARGET_OS)))
ifeq (,$(findstring sdl,$(TARGET_OS)))
    DEFAULT_LIBS += -lXi
endif
endif
endif
ifeq ($(MAKE_WITH_SDL),1)
    CFLAGS2 += -DSDL=1 -DSDL12
    DEFAULT_LIBS += -lSDL
endif
ifeq ($(MAKE_WITH_SDL),2)
    CFLAGS2 += -DSDL=2
    DEFAULT_LIBS += -lSDL2
endif
ifeq ($(MAKE_WITHOUT_MAIN),true)
    CFLAGS2 += -DNOMAIN
endif
ifeq ($(MAKE_WITHOUT_WM),true)
    CFLAGS2 += -DNOWM
endif
ifeq ($(MAKE_WITHOUT_SIMD),true)
    CFLAGS2 += -DNOSIMD
endif
ifeq ($(MAKE_WITH_OBJDUMP),true)
    CFLAGS2 += -g
    POSTLD = $(OBJDUMP) -S $(APPNAME)$(FILE_EXT) >$(APPNAME)$(FILE_EXT)_$(TARGET_OS)_$(TARGET_ARCH).objdump
endif
ifeq ($(CFLAGS),)
    CFLAGS = -O3
endif #empty CFLAGS
ifeq ($(LDFLAGS),)
ifeq (,$(findstring -g,$(CFLAGS)))
ifneq ($(MAKE_WITH_OBJDUMP),true)
    LDFLAGS = -s
endif #no OBJDUMP
endif #no -g (debug) in CFLAGS
endif #empty LDFLAGS
##
## Linux
##
ifneq (,$(findstring linux,$(TARGET_OS)))
ifeq ($(MAKE_DYNAMIC_LIB),true)
    CFLAGS2 += -fPIC
    LDFLAGS2 += -shared -Wl,-soname,$(APPNAME).so
    FILE_EXT = .so
endif
ifeq ($(MAKE_STATIC_LIB),true)
    LDFLAGS2 += -static
    FILE_EXT = .a
endif
endif
##
## MacOS
##
ifeq ($(TARGET_OS),macos)
ifeq ($(MAKE_DYNAMIC_LIB),true)
    CFLAGS2 += -fPIC
    LDFLAGS2 += -dynamiclib
    FILE_EXT = .dylib
endif
endif
##
## iOS
##
ifeq ($(TARGET_OS),ios)
ifeq ($(MAKE_STATIC_LIB),true)
    FILE_EXT = .a
endif
endif
##
## Windows
##
ifneq (,$(findstring windows,$(TARGET_OS)))
ifneq ($(TARGET_OS),windowsce)
ifneq ($(MAKE_WITH_CONSOLE),true)
    LDFLAGS2 += -mwindows
endif
ifeq ($(MAKE_DYNAMIC_LIB),true)
    FILE_EXT = .dll
    LDFLAGS2 += -shared -Wl,--kill-at
#-static-libgcc will remove the libgcc_s_sjlj-1 dependency (we don't use exceptions at all)
endif
endif
endif
##
## Emscripten
##
ifneq (,$(findstring emscripten,$(TARGET_OS)))
ifeq ($(MAKE_DYNAMIC_LIB),true)
    FILE_EXT = .js
    LDFLAGS2 += -s NO_EXIT_RUNTIME=1
endif
ifeq ($(MAKE_WITH_SDL),2)
    CFLAGS2 += -s USE_SDL=2
    LDFLAGS2 += -s USE_SDL=2
endif
endif

FINAL_CFLAGS = $(CFLAGS) $(CFLAGS2) $(SUNDOG_DEFINES) $(ADD_SUNDOG_DEFINES) -I./ -I$(SUNDOG) -DAPPNAME="\"$(APPNAME)\""
FINAL_LDFLAGS = $(LDFLAGS) $(LDFLAGS2)
FINAL_LIBS = $(LIBS) $(DEFAULT_LIBS)

##
## Object files
##

FILE = $(SUNDOG)file/
LOG = $(SUNDOG)log/
MAIN = $(SUNDOG)main/
MEM = $(SUNDOG)memory/
MISC = $(SUNDOG)misc/
NET = $(SUNDOG)net/
SOUND = $(SUNDOG)sound/
THREAD = $(SUNDOG)thread/
TIME = $(SUNDOG)time/
VIDEO = $(SUNDOG)video/
WM = $(SUNDOG)wm/

STD_INCS = $(SUNDOG)sundog.h $(LOG)log.h $(MAIN)main.h $(MEM)memory.h
FILE_OBJS = file.o file_format.o file_format_rw.o
LOG_OBJS = log.o
MAIN_OBJS = main.o
MEM_OBJS = memory.o
MISC_OBJS = smisc.o
NET_OBJS = net.o
SOUND_INCS = \
    $(SOUND)sound.h \
    $(SOUND)sound_common.hpp \
    $(SOUND)sound_common_jack.hpp \
    $(SOUND)sound_linux.hpp \
    $(SOUND)sound_linux_midi.hpp \
    $(SOUND)sound_win.hpp \
    $(SOUND)sound_win_midi.hpp \
    $(SOUND)sound_android.hpp \
    $(SOUND)sound_android_midi.hpp \
    $(SOUND)sound_macos.hpp \
    $(SOUND)sound_apple_midi.hpp \
    $(SOUND)sound_emscripten.hpp
SOUND_OBJS = sound.o
THREAD_OBJS = thread.o
TIME_OBJS = time.o
VIDEO_OBJS = video.o
WM_INCS = \
    $(WM)wm.h \
    $(WM)wm_struct.h \
    $(WM)wm_android.hpp \
    $(WM)wm_win.hpp \
    $(WM)wm_wince.hpp \
    $(WM)wm_opengl.hpp \
    $(WM)wm_win_ddraw.hpp \
    $(WM)wm_x11.hpp \
    $(WM)wm_sdl.hpp
ifneq ($(MAKE_WITHOUT_WM),true)
WM_OBJS = \
    wm.o \
    wm_fonts.o \
    wm_image.o \
    wm_keymap.o \
    wm_wbd.o \
    wm_framebuffer.o \
    wm_hnd.o \
    wm_hnd_color_theme.o \
    wm_hnd_dialog.o \
    wm_hnd_fdialog.o \
    wm_hnd_keymap.o \
    wm_hnd_list.o \
    wm_hnd_popup.o \
    wm_hnd_preferences.o \
    wm_hnd_scale.o \
    wm_hnd_virtual_keyboard.o \
    wm_hnd_webserver.o \
    devrgn.o
endif
OBJS = \
    $(FILE_OBJS) \
    $(LOG_OBJS) \
    $(MAIN_OBJS) \
    $(MEM_OBJS) \
    $(MISC_OBJS) \
    $(NET_OBJS) \
    $(SOUND_OBJS) \
    $(THREAD_OBJS) \
    $(TIME_OBJS) \
    $(VIDEO_OBJS) \
    $(WM_OBJS)

ifeq ($(TARGET_OS),ios)
    OBJS += sound_ios.o file_apple.o
endif

ifeq ($(TARGET_OS),macos)
    OBJS += file_apple.o
endif

ifneq (,$(findstring android,$(TARGET_OS)))
    OBJS += sundog_bridge.o
ifneq ($(MAKE_WITHOUT_MAIN),true)
    OBJS += android_native_app_glue.o
endif
endif

ifeq ($(MAKE_MODULE),vst3)
    OBJS += \
	main_vst3.o
    OBJS += \
	baseiids.o \
	fbuffer.o \
	fdebug.o \
	fdynlib.o \
	fobject.o \
	fstreamer.o \
	fstring.o \
	timer.o \
	updatehandler.o \
	fcondition.o \
	flock.o
    OBJS += \
	conststringtable.o \
	coreiids.o \
	funknown.o \
	ustring.o
    OBJS += \
	commoniids.o \
	memorystream.o \
	openurl.o \
	pluginview.o \
	systemclipboard_win32.o \
	threadchecker_win32.o \
	dllmain.o \
	pluginfactory.o \
	vstbus.o \
	vstcomponentbase.o \
	vstinitiids.o \
	vstparameters.o \
	vstsinglecomponenteffect.o
endif

##
## Rules
##

ifneq (,$(findstring linux,$(TARGET_OS)))
.SECONDEXPANSION:
all: $$(OBJS)
	$(LD) $(FINAL_LDFLAGS) -o $(APPNAME)$(FILE_EXT) $(OBJS) $(FINAL_LIBS)
	$(POSTLD)
endif

ifeq ($(TARGET_OS),macos)
.SECONDEXPANSION:
all: $$(OBJS)
	$(LD) $(FINAL_LDFLAGS) -o $(APPNAME)$(FILE_EXT) $(OBJS) $(FINAL_LIBS)
	$(POSTLD)
endif

ifeq ($(TARGET_OS),ios)
.SECONDEXPANSION:
ifeq ($(MAKE_STATIC_LIB),true)
all: $$(OBJS)
	$(LIBTOOL) $(LIBTOOL_FLAGS) -o $(APPNAME)$(FILE_EXT) $(OBJS)
	$(POSTLD)
endif
endif

ifneq (,$(findstring windows,$(TARGET_OS)))
ifneq ($(TARGET_OS),windowsce)
.SECONDEXPANSION:
all: $$(OBJS)
ifneq ($(MAKE_WITHOUT_WM),true)
	$(WINDRES) resources/win_res.rc win_res.o
	$(LD) $(FINAL_LDFLAGS) -o $(APPNAME)$(FILE_EXT) $(OBJS) win_res.o $(FINAL_LIBS)
else
	$(LD) $(FINAL_LDFLAGS) -o $(APPNAME)$(FILE_EXT) $(OBJS) $(FINAL_LIBS)
endif
	$(POSTLD)
endif
endif

ifeq ($(TARGET_OS),windowsce)
.SECONDEXPANSION:
all: $$(OBJS)
	$(WINDRES) resources/win_res.rc win_res.o
	$(LD) $(FINAL_LDFLAGS) -o $(APPNAME).exe $(OBJS) win_res.o $(FINAL_LIBS)
	$(POSTLD)
endif

ifneq (,$(findstring emscripten,$(TARGET_OS)))
.SECONDEXPANSION:
all: $$(OBJS)
	$(LD) $(FINAL_LDFLAGS) -o $(APPNAME)$(FILE_EXT) $(OBJS) $(FINAL_LIBS)
	$(POSTLD)
endif

sundog.h.gch: $(SUNDOG)sundog.h
	$(CXX) $(FINAL_CFLAGS) -x c++-header -o sundog.h.gch -c $(SUNDOG)sundog.h

file.o: $(FILE)file.cpp $(FILE)file.h $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(FILE)file.cpp

file_format.o: $(FILE)file_format.cpp $(FILE)file.h $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(FILE)file_format.cpp

file_format_rw.o: $(FILE)file_format_rw.cpp $(FILE)file.h $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(FILE)file_format_rw.cpp

log.o: $(LOG)log.cpp
	$(CXX) $(FINAL_CFLAGS) -c $(LOG)log.cpp

main.o: $(MAIN)main.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(MAIN)main.cpp

memory.o: $(MEM)memory.cpp $(MEM)memory.h $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(MEM)memory.cpp

smisc.o: $(MISC)misc.cpp $(MISC)misc.h $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(MISC)misc.cpp -o smisc.o

net.o: $(NET)net.cpp $(NET)net.h $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(NET)net.cpp

sound.o: $(SOUND)sound.cpp $(SOUND)sound.h $(SOUND_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(SOUND)sound.cpp

thread.o: $(THREAD)thread.cpp $(THREAD)thread.h $(SOUND_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(THREAD)thread.cpp

time.o: $(TIME)time.cpp $(TIME)time.h $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(TIME)time.cpp

video.o: $(VIDEO)video.cpp $(VIDEO)video_linux.hpp $(VIDEO)video_android.hpp $(VIDEO)video.h $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VIDEO)video.cpp

wm.o: $(WM)wm.cpp $(WM_INCS) $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(WM)wm.cpp

wm_fonts.o: $(WM)wm_fonts.cpp $(WM_INCS) $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(WM)wm_fonts.cpp

wm_framebuffer.o: $(WM)wm_framebuffer.cpp $(WM_INCS) $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(WM)wm_framebuffer.cpp

wm_image.o: $(WM)wm_image.cpp $(WM_INCS) $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(WM)wm_image.cpp

wm_keymap.o: $(WM)wm_keymap.cpp $(WM_INCS) $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(WM)wm_keymap.cpp

wm_wbd.o: $(WM)wm_wbd.cpp $(WM_INCS) $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(WM)wm_wbd.cpp

wm_hnd.o: $(WM)wm_hnd.cpp $(WM_INCS) $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(WM)wm_hnd.cpp

wm_hnd_color_theme.o: $(WM)wm_hnd_color_theme.cpp $(WM_INCS) $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(WM)wm_hnd_color_theme.cpp

wm_hnd_dialog.o: $(WM)wm_hnd_dialog.cpp $(WM_INCS) $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(WM)wm_hnd_dialog.cpp

wm_hnd_fdialog.o: $(WM)wm_hnd_fdialog.cpp $(WM_INCS) $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(WM)wm_hnd_fdialog.cpp

wm_hnd_keymap.o: $(WM)wm_hnd_keymap.cpp $(WM_INCS) $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(WM)wm_hnd_keymap.cpp

wm_hnd_list.o: $(WM)wm_hnd_list.cpp $(WM_INCS) $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(WM)wm_hnd_list.cpp

wm_hnd_popup.o: $(WM)wm_hnd_popup.cpp $(WM_INCS) $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(WM)wm_hnd_popup.cpp

wm_hnd_preferences.o: $(WM)wm_hnd_preferences.cpp $(WM_INCS) $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(WM)wm_hnd_preferences.cpp

wm_hnd_scale.o: $(WM)wm_hnd_scale.cpp $(WM_INCS) $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(WM)wm_hnd_scale.cpp

wm_hnd_virtual_keyboard.o: $(WM)wm_hnd_virtual_keyboard.cpp $(WM_INCS) $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(WM)wm_hnd_virtual_keyboard.cpp

wm_hnd_webserver.o: $(WM)wm_hnd_webserver.cpp $(WM_INCS) $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(WM)wm_hnd_webserver.cpp

devrgn.o: $(WM)regions/devrgn.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(WM)regions/devrgn.cpp

#ANDROID
android_native_app_glue.o: $(ANDROID_NDK)/sources/android/native_app_glue/android_native_app_glue.c
	$(CXX) $(FINAL_CFLAGS) -DNDEBUG -c $(ANDROID_NDK)/sources/android/native_app_glue/android_native_app_glue.c
sundog_bridge.o: $(SUNDOG)main/android/sundog_bridge.cpp
	$(CXX) $(FINAL_CFLAGS) -c $(SUNDOG)main/android/sundog_bridge.cpp

#IOS
sound_ios.o: $(SOUND)sound_ios.mm $(SOUND)sound.h $(SOUND_INCS)
	$(CMM) $(FINAL_CFLAGS) -c $(SOUND)sound_ios.mm
file_apple.o: $(FILE)file_apple.mm $(FILE)file.h $(STD_INCS)
	$(CMM) $(FINAL_CFLAGS) -c $(FILE)file_apple.mm

#VST3
main_vst3.o: $(MAIN)main_vst3.cpp $(MAIN)main_vst3.h $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(MAIN)main_vst3.cpp
#VST3 BASE
baseiids.o: $(VST3_SDK_BASE)/source/baseiids.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_BASE)/source/baseiids.cpp
fbuffer.o: $(VST3_SDK_BASE)/source/fbuffer.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_BASE)/source/fbuffer.cpp
fdebug.o: $(VST3_SDK_BASE)/source/fdebug.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_BASE)/source/fdebug.cpp
fdynlib.o: $(VST3_SDK_BASE)/source/fdynlib.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_BASE)/source/fdynlib.cpp
fobject.o: $(VST3_SDK_BASE)/source/fobject.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_BASE)/source/fobject.cpp
fstreamer.o: $(VST3_SDK_BASE)/source/fstreamer.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_BASE)/source/fstreamer.cpp
fstring.o: $(VST3_SDK_BASE)/source/fstring.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_BASE)/source/fstring.cpp
timer.o: $(VST3_SDK_BASE)/source/timer.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_BASE)/source/timer.cpp
updatehandler.o: $(VST3_SDK_BASE)/source/updatehandler.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_BASE)/source/updatehandler.cpp
fcondition.o: $(VST3_SDK_BASE)/thread/source/fcondition.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_BASE)/thread/source/fcondition.cpp
flock.o: $(VST3_SDK_BASE)/thread/source/flock.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_BASE)/thread/source/flock.cpp
#VST3 INTERFACES
conststringtable.o: $(VST3_SDK_IF)/base/conststringtable.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_IF)/base/conststringtable.cpp
coreiids.o: $(VST3_SDK_IF)/base/coreiids.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_IF)/base/coreiids.cpp
funknown.o: $(VST3_SDK_IF)/base/funknown.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_IF)/base/funknown.cpp
ustring.o: $(VST3_SDK_IF)/base/ustring.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_IF)/base/ustring.cpp
#VST3 HELPER CLASSES
commoniids.o: $(VST3_SDK_HELPER)/common/commoniids.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_HELPER)/common/commoniids.cpp
memorystream.o: $(VST3_SDK_HELPER)/common/memorystream.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_HELPER)/common/memorystream.cpp
openurl.o: $(VST3_SDK_HELPER)/common/openurl.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_HELPER)/common/openurl.cpp
pluginview.o: $(VST3_SDK_HELPER)/common/pluginview.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_HELPER)/common/pluginview.cpp
systemclipboard_win32.o: $(VST3_SDK_HELPER)/common/systemclipboard_win32.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_HELPER)/common/systemclipboard_win32.cpp
threadchecker_win32.o: $(VST3_SDK_HELPER)/common/threadchecker_win32.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_HELPER)/common/threadchecker_win32.cpp
dllmain.o: $(VST3_SDK_HELPER)/main/dllmain.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_HELPER)/main/dllmain.cpp
pluginfactory.o: $(VST3_SDK_HELPER)/main/pluginfactory.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_HELPER)/main/pluginfactory.cpp
vstbus.o: $(VST3_SDK_HELPER)/vst/vstbus.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_HELPER)/vst/vstbus.cpp
vstcomponentbase.o: $(VST3_SDK_HELPER)/vst/vstcomponentbase.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_HELPER)/vst/vstcomponentbase.cpp
vstinitiids.o: $(VST3_SDK_HELPER)/vst/vstinitiids.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_HELPER)/vst/vstinitiids.cpp
vstparameters.o: $(VST3_SDK_HELPER)/vst/vstparameters.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_HELPER)/vst/vstparameters.cpp
vstsinglecomponenteffect.o: $(VST3_SDK_HELPER)/vst/vstsinglecomponenteffect.cpp $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(VST3_SDK_HELPER)/vst/vstsinglecomponenteffect.cpp

#X.o: $()X.cpp $()X.h $(STD_INCS)
#	$(CXX) $(FINAL_CFLAGS) -c $()X.cpp

clean:
	rm -f *.o *.a *.optrpt
