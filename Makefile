MKDIR = mkdir -p
CXX = g++
RM = rm -f
RMDIR = rm -rf
INC = -I src
LDFLAGS = -lstdc++
CPPFLAGS = -g -std=c++17 $(INC) -Wall
STRIP = strip
 
ifdef CONFIG_W32
    CXX = i686-w64-mingw32-g++
    LDFLAGS = -mconsole -static-libgcc -static-libstdc++ -lwsock32 -lwinspool -lws2_32 -lgdi32 -lwinmm -lsetupapi -lole32 -loleaut32
    INC = -I src -I /opt/local/mingw-std-threads-1.0.0
    CPPFLAGS = -O3 -std=c++17  -pthread $(INC) -D_WIN32 -Wall
    WINDRES = i686-w64-mingw32-windres
    STRIP = i686-w64-mingw32-strip
    WINDRESARGS = 
endif
 
ifdef CONFIG_W64
    CXX = x86_64-w64-mingw32-g++
    LDFLAGS = -mconsole -static-libgcc -static-libstdc++ -lwsock32 -lwinspool -lws2_32 -lgdi32 -lwinmm -lsetupapi -lole32 -loleaut32
    INC = -I src -I /opt/local/mingw-std-threads-1.0.0
    CPPFLAGS = -O3 -std=c++17  -pthread $(INC) -D_WIN64 -Wall
    WINDRES = x86_64-w64-mingw32-windres
    STRIP = x86_64-w64-mingw32-strip
    WINDRESARGS = 
endif

VERSION = $(shell cat VERSION.txt)
CPPFLAGS := $(CPPFLAGS) -DVERSION="\"$(VERSION)\""
LDFLAGS := $(LDFLAGS)

# Temporary build directories
ifdef CONFIG_W32
    BUILD := .win32
else ifdef CONFIG_W64
    BUILD := .win64
else
    BUILD := .nix
endif
 
# Define V=1 to show command line.
ifdef V
    Q :=
    E := @true
else
    Q := @
    E := @echo
endif
 
ifdef CONFIG_W32
    TARG := soda.exe
else ifdef CONFIG_W64
    TARG := soda64.exe
else
    TARG := soda
endif
 
all: $(TARG)
 
default: all
 
.PHONY: all default clean strip
 
COMMON_OBJS := \
        src/Assembly.o \
        src/Compiler.o \
        src/Environment.o \
        src/Parser.o \
        src/System.o \
	src/main.o 

ifdef CONFIG_W32
OBJS := \
        $(COMMON_OBJS) \
        soda.res
else ifdef CONFIG_W64
OBJS := \
        $(COMMON_OBJS) \
        soda.res
else
OBJS := \
        $(COMMON_OBJS)
endif


# Rewrite paths to build directories
OBJS := $(patsubst %,$(BUILD)/%,$(OBJS))

$(TARG): $(OBJS)
	$(E) [LD] $@    
	$(Q)$(MKDIR) $(@D)
	$(Q)$(CXX) -o $@ $(OBJS) $(LDFLAGS)

clean:
	$(E) [CLEAN]
	$(Q)$(RM) $(TARG)
	$(Q)$(RMDIR) $(BUILD)

strip: $(TARG)
	$(E) [STRIP]
	$(Q)$(STRIP) $(TARG)

$(BUILD)/%.o: %.cpp
	$(E) [CXX] $@
	$(Q)$(MKDIR) $(@D)
	$(Q)$(CXX) -c $(CPPFLAGS) -o $@ $<

$(BUILD)/%.res: %.rc
	$(E) [RES] $@
	$(Q)$(WINDRES) $< -O coff -o $@ $(WINDRESARGS)
