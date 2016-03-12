# Makefile

include config.mak

vpath %.c $(SRCPATH)
vpath %.h $(SRCPATH)
vpath %.S $(SRCPATH)
vpath %.asm $(SRCPATH)
vpath %.rc $(SRCPATH)

all: default
default:

SRCS = bavs_cabac.c \
       bavs_cavlc.c \
       bavs_clearbit.c \
       bavs_cpu.c \
       bavs_decoder.c \
       bavs_frame.c threadpool.c \
       bavs_globe.c               
	       
SRCCLI = bavs.c
SRCSO =
OBJS =
OBJSO =
OBJCLI =

OBJCHK =

CONFIG: $(shell cat config.h)


# MMX/SSE optims
ifneq ($(AS),)
X86SRC = common/x86/bavs_const-a.asm \
	 common/x86/bavs_cpu-a.asm \
         common/x86/bavs_idct.asm \
	 common/x86/bavs_deblock.asm \
         common/x86/bavs_deblock_inter.asm \
         common/x86/bavs_deblock_intra.asm \
         common/x86/bavs_mc.asm \
         common/x86/bavs_mc_chroma.asm \
         common/x86/bavs_predict.asm \
	 common/x86/bavs_avg.asm 
ifeq ($(ARCH),X86)
ARCH_X86 = yes
ASMSRC   = $(X86SRC) 
endif

ifeq ($(ARCH),X86_64)
ARCH_X86 = yes
ASMSRC   = $(X86SRC)
ASFLAGS += -DARCH_X86_64=1
ASFLAGS += -DNON_MOD16_STACK
endif

ifneq ($(ARCH),X86_64)
ARCH_X86 = yes
ASMSRC   = $(X86SRC)
ASFLAGS += -DARCH_X86_64=0
ASFLAGS += -DNON_MOD16_STACK
endif

ifdef ARCH_X86
ASFLAGS += -I$(SRCPATH)common/x86/
SRCS    += common/x86/bavs_mc-c.c
OBJASM  = $(ASMSRC:%.asm=%.o)
$(OBJASM): common/x86/bavs_x86inc.asm common/x86/bavs_x86util.asm
endif
endif




ifeq ($(SYS), WINDOWS)
OBJCLI += $(if $(RC), bavsres.o)
endif

OBJS += $(SRCS:%.c=%.o)
OBJCLI += $(SRCCLI:%.c=%.o)
OBJSO += $(SRCSO:%.c=%.o)

.PHONY: all default fprofiled clean distclean install uninstall lib-static lib-shared cli install-lib-dev install-lib-static install-lib-shared install-cli

cli: bavs$(EXE)
lib-static: $(LIBBAVS)
lib-shared: $(SONAME)

$(LIBBAVS): .depend $(OBJS) $(OBJASM)
	rm -f $(LIBBAVS)
	$(AR)$@ $(OBJS) $(OBJASM)
	$(if $(RANLIB), $(RANLIB) $@)

$(SONAME): .depend $(OBJS) $(OBJASM) $(OBJSO)
	$(LD) $@ $(OBJS) $(OBJASM) $(OBJSO) $(SOFLAGS) $(LDFLAGS)

ifneq ($(EXE),)
.PHONY: bavs
bavs: bavs$(EXE)
endif

bavs$(EXE): .depend $(OBJCLI) $(CLI_LIBBAVS) 
	$(LD) $@ $(OBJCLI) $(CLI_LIBBAVS) $(LDFLAGSCLI) $(LDFLAGS)

$(OBJS) $(OBJASM) $(OBJSO) $(OBJCLI) $(OBJCHK): .depend

%.o: %.asm
	$(AS) $(ASFLAGS) -o $@ $<
#delete local/anonymous symbols, so they don't show up in oprofile
	-@ $(if $(STRIP), $(STRIP) -x $@)

%.o: %.S
	$(AS) $(ASFLAG) -o $@ $<
	-@ $(if $(STRIP), $(STRIP) -x $@)

%.dll.o: %.rc bavs.h
	$(RC) -DDLL -o $@ $<

%.o: %.rc bavs.h
	$(RC) -o $@ $<

.depend: config.mak
	rm -f .depend
	@$(foreach SRC, $(addprefix $(SRCPATH)/, $(SRCS) $(SRCCLI) $(SRCSO)), $(CC) $(CFLAGS) $(SRC) $(DEPMT) $(SRC:$(SRCPATH)/%.c=%.o) $(DEPMM) 1>> .depend;)

config.mak:
	./configure

depend: .depend
ifneq ($(wildcard .depend),)
include .depend
endif

clean:
	rm -f $(OBJS) $(OBJASM) $(OBJCLI) $(OBJSO) $(SONAME) *.a *.lib *.exp *.pdb bavs bavs.exe .depend TAGS
	rm -f $(SRC2:%.c=%.gcda) $(SRC2:%.c=%.gcno) *.dyn pgopti.dpi pgopti.dpi.lock

distclean: clean
	rm -f config.mak bavs_config.h config.h config.log bavs.pc bavs.def

install-cli: cli
	install -d $(DESTDIR)$(bindir)
	install bavs$(EXE) $(DESTDIR)$(bindir)

install-lib-dev:
	install -d $(DESTDIR)$(includedir)
	install -d $(DESTDIR)$(libdir)
	install -d $(DESTDIR)$(libdir)/pkgconfig
	install -m 644 $(SRCPATH)/bavs.h $(DESTDIR)$(includedir)
	install -m 644 bavs_config.h $(DESTDIR)$(includedir)
	install -m 644 bavs.pc $(DESTDIR)$(libdir)/pkgconfig

install-lib-static: lib-static install-lib-dev
	install -m 644 $(LIBBAVS) $(DESTDIR)$(libdir)
	$(if $(RANLIB), $(RANLIB) $(DESTDIR)$(libdir)/$(LIBBAVS))

install-lib-shared: lib-shared install-lib-dev
ifneq ($(IMPLIBNAME),)
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(SONAME) $(DESTDIR)$(bindir)
	install -m 644 $(IMPLIBNAME) $(DESTDIR)$(libdir)
else ifneq ($(SONAME),)
	ln -f -s $(SONAME) $(DESTDIR)$(libdir)/libbavs.$(SOSUFFIX)
	install -m 755 $(SONAME) $(DESTDIR)$(libdir)
endif

uninstall:
	rm -f $(DESTDIR)$(includedir)/bavs.h $(DESTDIR)$(includedir)/bavs_config.h $(DESTDIR)$(libdir)/libbavs.a
	rm -f $(DESTDIR)$(bindir)/bavs$(EXE) $(DESTDIR)$(libdir)/pkgconfig/bavs.pc
ifneq ($(IMPLIBNAME),)
	rm -f $(DESTDIR)$(bindir)/$(SONAME) $(DESTDIR)$(libdir)/$(IMPLIBNAME)
else ifneq ($(SONAME),)
	rm -f $(DESTDIR)$(libdir)/$(SONAME) $(DESTDIR)$(libdir)/libbavs.$(SOSUFFIX)
endif

etags: TAGS

TAGS:
	etags $(SRCS)
