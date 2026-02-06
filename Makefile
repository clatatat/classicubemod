SOURCE_DIR  = src
BUILD_DIR   = build
C_SOURCES   = $(wildcard $(SOURCE_DIR)/*.c)
OBJECTS   	= $(patsubst %.c, $(BUILD_DIR)/%.o, $(C_SOURCES))
BUILD_DIRS	= $(BUILD_DIR) $(BUILD_DIR)/src

##############################
# Configurable flags and names
##############################
# Flags passed to the C compiler
CFLAGS  = -pipe -fno-math-errno -Werror -Wno-error=missing-braces -Wno-error=strict-aliasing
# Flags passed to the linker
LDFLAGS = -g -rdynamic
# Name of the main executable
ENAME   = ClassiCube
# Name of the final target file
# (usually this is the executable, but e.g. is the app bundle on macOS)
TARGET  := $(ENAME)

# Enables dependency tracking (https://make.mad-scientist.net/papers/advanced-auto-dependency-generation/)
# This ensures that changing a .h file automatically results in the .c files using it being auto recompiled when next running make
# On older systems the required GCC options may not be supported - in which case just change TRACK_DEPENDENCIES to 0
TRACK_DEPENDENCIES=1
# link using C Compiler by default
LINK = $(CC)
# Whether to add BearSSL source files to list of files to compile
BEARSSL=1
# Optimization level in release builds
OPT_LEVEL=1


#################################################################
# Determine shell command used to remove files (for "make clean")
#################################################################
ifndef RM
	# No prefined RM variable, try to guess OS default
	ifeq ($(OS),Windows_NT)
		RM = del
	else
		RM = rm -f
	endif
endif


###########################################################
# If target platform isn't specified, default to current OS
###########################################################
ifndef $(PLAT)
	ifeq ($(OS),Windows_NT)
		PLAT = mingw
	else
		PLAT = $(shell uname -s | tr '[:upper:]' '[:lower:]')
	endif
endif


#########################################################
# Setup environment appropriate for the specific platform
#########################################################
ifeq ($(PLAT),mingw)
	CC      =  gcc
	OEXT    =  .exe
	CFLAGS  += -DUNICODE
	LDFLAGS =  -g
	LIBS    =  -mwindows -lwinmm
	BUILD_DIR = build/win
endif

ifeq ($(PLAT),linux)
	# -lm may be needed for __builtin_sqrtf (in cases where it isn't replaced by a CPU instruction intrinsic)
	LIBS    =  -lX11 -lXi -lpthread -lGL -ldl -lm
	BUILD_DIR = build/linux

	# Detect MCST LCC, where -O3 is about equivalent to -O1
	ifeq ($(shell $(CC) -dM -E -xc - < /dev/null | grep -o __MCST__),__MCST__)
		OPT_LEVEL=3
	endif
endif

ifeq ($(PLAT),web)
	CC      = emcc
	OEXT    = .html
	CFLAGS  = -g
	LDFLAGS = -g -s WASM=1 -s NO_EXIT_RUNTIME=1 -s ABORTING_MALLOC=0 -s ALLOW_MEMORY_GROWTH=1 -s TOTAL_STACK=256Kb --js-library $(SOURCE_DIR)/webclient/interop_web.js
	BUILD_DIR = build/web
	BEARSSL = 0

	BUILD_DIRS += $(BUILD_DIR)/src/webclient
	C_SOURCES  += $(wildcard src/webclient/*.c)
endif



ifdef BUILD_SDL2
	CFLAGS += -DCC_WIN_BACKEND=CC_WIN_BACKEND_SDL2
	LIBS += -lSDL2
endif
ifdef BUILD_SDL3
	CFLAGS += -DCC_WIN_BACKEND=CC_WIN_BACKEND_SDL3
	LIBS += -lSDL3
endif
ifdef BUILD_TERMINAL
	CFLAGS += -DCC_WIN_BACKEND=CC_WIN_BACKEND_TERMINAL -DCC_GFX_BACKEND=CC_GFX_BACKEND_SOFTGPU
	LIBS := $(subst mwindows,mconsole,$(LIBS))
endif

ifeq ($(BEARSSL),1)
	BUILD_DIRS += $(BUILD_DIR)/third_party/bearssl
	C_SOURCES  += $(wildcard third_party/bearssl/*.c)
endif

ifdef RELEASE
	CFLAGS += -O$(OPT_LEVEL)
else
	CFLAGS += -g
endif

default: $(PLAT)

# Build for the specified platform
mingw:
	$(MAKE) $(TARGET) PLAT=mingw
linux:
	$(MAKE) $(TARGET) PLAT=linux
# Default overrides
sdl2:
	$(MAKE) $(TARGET) BUILD_SDL2=1
sdl3:
	$(MAKE) $(TARGET) BUILD_SDL3=1
terminal:
	$(MAKE) $(TARGET) BUILD_TERMINAL=1
release:
	$(MAKE) $(TARGET) RELEASE=1

# Cleans up all build .o files
clean:
	$(RM) $(OBJECTS)


#################################################
# Source files and executable compilation section
#################################################
# Auto creates directories for build files (.o and .d files)
$(BUILD_DIRS):
	mkdir -p $@

# Main executable (typically just 'ClassiCube' or 'ClassiCube.exe')
$(ENAME): $(BUILD_DIRS) $(OBJECTS)
	$(LINK) $(LDFLAGS) -o $@$(OEXT) $(OBJECTS) $(EXTRA_LIBS) $(LIBS)
	@echo "----------------------------------------------------"
	@echo "Successfully compiled executable file: $(ENAME)"
	@echo "----------------------------------------------------"

# macOS app bundle
$(ENAME).app : $(ENAME)
	mkdir -p $(TARGET)/Contents/MacOS
	mkdir -p $(TARGET)/Contents/Resources
	cp $(ENAME) $(TARGET)/Contents/MacOS/$(ENAME)
	cp misc/macOS/Info.plist   $(TARGET)/Contents/Info.plist
	cp misc/macOS/appicon.icns $(TARGET)/Contents/Resources/appicon.icns


# === Compiling with dependency tracking ===
# NOTE: Tracking dependencies might not work on older systems - disable this if so
ifeq ($(TRACK_DEPENDENCIES), 1)

DEPFLAGS = -MT $@ -MMD -MP -MF $(BUILD_DIR)/$*.d
DEPFILES := $(patsubst %.o, %.d, $(OBJECTS))
$(DEPFILES):

$(BUILD_DIR)/%.o : %.c $(BUILD_DIR)/%.d
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(DEPFLAGS) -c $< -o $@
$(BUILD_DIR)/%.o : %.cpp $(BUILD_DIR)/%.d
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(DEPFLAGS) -c $< -o $@
$(BUILD_DIR)/%.o : %.m $(BUILD_DIR)/%.d
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(DEPFLAGS) -c $< -o $@

include $(wildcard $(DEPFILES))
# === Compiling WITHOUT dependency tracking ===
else

$(BUILD_DIR)/%.o : %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c $< -o $@
$(BUILD_DIR)/%.o : %.cpp
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c $< -o $@
endif

# EXTRA_CFLAGS and EXTRA_LIBS are not defined in the makefile intentionally -
# define them on the command line as a simple way of adding CFLAGS/LIBS
