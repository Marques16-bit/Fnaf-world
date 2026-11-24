# Makefile do FNaF World PS4 (OpenOrbis PS4 Toolchain, LLVM 18).
#   make -C ps4 pkg        -> compila e gera o .pkg
#   make -C ps4 eboot.bin  -> so compila e gera o eboot.bin
#   make -C ps4 clean
#
# Usa ".RECIPEPREFIX" para as receitas comecarem com ">" (evita problemas de TAB ao copiar/colar no celular).
.RECIPEPREFIX = >

TOOLCHAIN ?= $(OO_PS4_TOOLCHAIN)
ifeq ($(TOOLCHAIN),)
$(error Defina OO_PS4_TOOLCHAIN com a pasta do toolchain OpenOrbis)
endif

BIN          := $(TOOLCHAIN)/bin/linux
PKGTOOL      ?= $(BIN)/PkgTool.Core
CREATE_FSELF ?= $(BIN)/create-fself
CREATE_GP4   ?= $(BIN)/create-gp4

TITLE      := FNaF World PS4
TITLE_ID   := FNAF00001
CONTENT_ID := IV0000-FNAF00001_00-FNAFWORLDPS4BETA
VERSION    := 01.00

CXX := clang++
LD  := ld.lld

SRCS := $(wildcard src/*.cpp) $(wildcard src/core/*.cpp) $(wildcard src/platform/ps4/*.cpp)
OBJS := $(patsubst src/%.cpp,build/%.o,$(SRCS))

CXXFLAGS := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -isysroot $(TOOLCHAIN) \
            -isystem $(TOOLCHAIN)/include -isystem $(TOOLCHAIN)/include/c++/v1 \
            -std=c++17 -O2 -fno-exceptions -fno-rtti -fno-math-errno -Wall -Wno-unused-function \
            -D__PS4__ -MMD -MP

# Bibliotecas: libc/libkernel/libc++ + as stubs de sistema que o app usa (so as que existirem no toolchain).
CORE_LIBS  := -lc -lkernel -lc++
WANT_STUBS := SceLibcInternal SceVideoOut ScePad SceUserService SceSysmodule SceSystemService
STUB_LIBS  := $(foreach s,$(WANT_STUBS),$(if $(wildcard $(TOOLCHAIN)/lib/lib$(s).so),-l$(s)))
LDFLAGS    := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib \
              $(CORE_LIBS) --as-needed $(STUB_LIBS) --no-as-needed $(TOOLCHAIN)/lib/crt1.o

# Arquivos que vao dentro do PKG
ASSET_FILES  := $(shell find assets -type f ! -name '.gitkeep' 2>/dev/null | sort)
MODULE_FILES := $(wildcard sce_module/*.prx)
ABOUT_FILES  := $(wildcard sce_sys/about/right.sprx)
PKG_FILES    := eboot.bin sce_sys/param.sfo sce_sys/icon0.png $(ABOUT_FILES) $(MODULE_FILES) $(ASSET_FILES)

.PHONY: all pkg clean
all: pkg
pkg: $(CONTENT_ID).pkg

build/%.o: src/%.cpp
> @mkdir -p $(dir $@)
> $(CXX) $(CXXFLAGS) -c $< -o $@

build/fnafworld.elf: $(OBJS)
> $(LD) $(OBJS) -o $@ $(LDFLAGS)

eboot.bin: build/fnafworld.elf
> $(CREATE_FSELF) -in=build/fnafworld.elf -out=build/fnafworld.oelf --eboot "eboot.bin" --paid 0x3800000000000011

sce_sys/param.sfo: Makefile
> $(PKGTOOL) sfo_new $@
> $(PKGTOOL) sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 0
> $(PKGTOOL) sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
> $(PKGTOOL) sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
> $(PKGTOOL) sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
> $(PKGTOOL) sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
> $(PKGTOOL) sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
> $(PKGTOOL) sfo_setentry $@ FORMAT --type Utf8 --maxsize 4 --value 'obs'
> $(PKGTOOL) sfo_setentry $@ PARENTAL_LEVEL --type Integer --maxsize 4 --value 0
> $(PKGTOOL) sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
> $(PKGTOOL) sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
> $(PKGTOOL) sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
> $(PKGTOOL) sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

pkg.gp4: $(PKG_FILES)
> $(CREATE_GP4) -out $@ --content-id=$(CONTENT_ID) --files "$(PKG_FILES)"

$(CONTENT_ID).pkg: pkg.gp4
> $(PKGTOOL) pkg_build pkg.gp4 .

clean:
> rm -rf build eboot.bin pkg.gp4 sce_sys/param.sfo *.pkg

-include $(OBJS:.o=.d)
