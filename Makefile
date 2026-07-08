# toolchain
CC           = sdcc
CP           = sdobjcopy
AS           = sdas8051
LD			= sdld
HEX          = packihx
BIN          = $(CP) -O binary -S

# define mcu, specify the target processor
F_CPU   ?= 16000000
MCU          = mcs51
ARCH = mcs51

# ------------------------------------------------------
# Usually SDCC's small memory model is the best choice.  If
# you run out of internal RAM, you will need to declare
# variables as "xdata", or switch to largeer model

# Memory Model (small, medium, large, huge)
MODEL  = large
# ------------------------------------------------------
# Memory Layout
# PRG Size = 32K Bytes
CODE_SIZE = --code-loc 0x0000 --code-size 65536
# INT-MEM Size = 256 Bytes
IRAM_SIZE = --idata-loc 0x0000  --iram-size 256
# EXT-MEM Size = 1K Bytes
XRAM_SIZE = --xram-loc 0x0000 --xram-size 3072

# all the files will be generated with this name (main.elf, main.bin, main.hex, etc)
PROJECT_NAME=music-box-8051

# Storage backend: runtime-selectable via function pointer dispatcher
# Both backends are always compiled; backend chosen by storage_auto_detect() at boot

# specify define
DEFS       = NO_RUN_TEST
DEFS += STC8

# define root dir
ROOT_DIR     = .

# define include dir
INCLUDE_DIRS = Player Synthesizer

# define lib dir
LIBDIR   = 

# user specific

SRC 	+= main.c
SRC 	+= Synthesizer/AlgorithmTest.c
SRC 	+= Synthesizer/SynthCore.c
SRC 	+= Player/Player.c
SRC 	+= UartRedirect.c
SRC 	+= Bsp.c
SRC 	+= Protocol.c
SRC 	+= Synthesizer/WaveTable.c

# Storage backends (both always compiled)
SRC 	+= Storage.c
SRC 	+= Storage_Internal.c
SRC 	+= Storage_SPI.c
SRC 	+= SpiFlash.c
SRC 	+= scoreList.c

ASM_SRC =
ASM_SRC   += Synthesizer/PeriodTimer.s
ASM_SRC   += Synthesizer/SynthCoreAsm.s

# ASM_SRC   += Synthesizer/Synth_testbench.s
# ASM_SRC   += Synthesizer/UpdateTick_testbench.s

INC_DIR  = $(patsubst %, -I%, $(INCLUDE_DIRS))
AS_INC   = $(INC_DIR)

# run from Flash
DDEFS	 = $(patsubst %, -D%, $(DEFS))
DEPS  = $(SRC:.c=.d)
OBJECTS  = $(SRC:.c=.rel) $(ASM_SRC:.s=.rel)
OTHER_OUTPUTS += $(ASM_SRC:.s=.asm) $(SRC:.c=.asm)
OTHER_OUTPUTS += $(ASM_SRC:.s=.lst) $(SRC:.c=.lst)
OTHER_OUTPUTS += $(ASM_SRC:.s=.rst) $(SRC:.c=.rst)
OTHER_OUTPUTS += $(ASM_SRC:.s=.sym) $(SRC:.c=.sym)


CFLAGS  = -m$(ARCH) -p$(MCU) --model-$(MODEL) --std-sdcc11
CFLAGS += -DF_CPU=$(F_CPU)UL -I. -I$(LIBDIR) $(DDEFS) --stack-auto
ASFLAGS  = $(AS_INC) -plosgff -l -s
LD_FLAGS = -m$(ARCH) -l$(ARCH) --out-fmt-ihx -m$(MCU_MODEL) --model-$(MODEL) $(CODE_SIZE) $(IRAM_SIZE) $(XRAM_SIZE) --stack-auto

#
# makefile rules
#
all: $(OBJECTS) $(PROJECT_NAME).ihx $(PROJECT_NAME).hex $(PROJECT_NAME).bin

# Don't delete dependency files
.PRECIOUS: %.d

# Don't rebuild deps if cleaning
ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
# Beacuse SDCC's assembler has no way to auto output dependency info,
# the dependency is manually written here.	
Synthesizer/PeriodTimer.rel: Synthesizer/SynthCore.inc Synthesizer/8051.inc Synthesizer/Synth.inc Synthesizer/UpdateTick.inc Synthesizer/WaveTable.inc
Synthesizer/Synth_testbench.rel: Synthesizer/SynthCore.inc Synthesizer/8051.inc Synthesizer/Synth.inc Synthesizer/UpdateTick.inc
Synthesizer/UpdateTick_testbench.rel: Synthesizer/SynthCore.inc Synthesizer/8051.inc Synthesizer/Synth.inc Synthesizer/UpdateTick.inc
Synthesizer/SynthCoreAsm.rel: Synthesizer/SynthCore.inc Synthesizer/WaveTable.inc
endif




%.rel: %.c Makefile
	@echo [CC] $(notdir $<)
# Output dependency
	@$(CC) $(CFLAGS) $(INC_DIR) -MM -c $< | sed 's|^[^:]*:|$@:|' > $(patsubst %.c,%.d,$<)
# Do compiling
	@$(CC) $(CFLAGS) $(INC_DIR) -c $< -o $@
	


%.rel: %.s
	@echo [AS] $(notdir $<)
	@$(AS) $(ASFLAGS) $<

%.ihx: $(OBJECTS)
	@echo [LD] $(PROJECT_NAME).ihx
	@$(CC) $(LD_FLAGS) $(OBJECTS) -o $@
	
%.hex: %.ihx
	@echo [HEX] $(PROJECT_NAME).hex
	@$(HEX) $< > $@	
	
%.bin: %.ihx
	@echo [BIN] $(PROJECT_NAME).bin
	@$(CP) -I ihex -O binary $< $@

# stcgal settings (open-source STC MCU ISP tool for Linux)
# install: pip3 install stcgal
# https://github.com/grigorig/stcgal
STCGAL_PORT   ?= /dev/ttyUSB0
STCGAL_BAUD   ?= 115200
STCGAL_PROTO  ?= stc8d
STCGAL_BOOT_CMD ?= auto

BOOT_SCRIPT = python3 tools/boot.py $(STCGAL_PORT) $(STCGAL_BAUD)

boot:
	@echo [BOOT] sending RESET frame to $(STCGAL_PORT)
	$(BOOT_SCRIPT)

flash: $(PROJECT_NAME).ihx
	@echo [BOOT] sending RESET frame to $(STCGAL_PORT)
	@$(BOOT_SCRIPT)
	@echo [FLASH] $(PROJECT_NAME).ihx via stcgal
	stcgal -P $(STCGAL_PROTO) -p $(STCGAL_PORT) -b $(STCGAL_BAUD) $<
	
clean:
	@echo [RM] OBJ
	@-rm -rf $(OBJECTS)
	@echo [RM] HEX
	@-rm -rf $(PROJECT_NAME).ihx
	@echo [RM] Intermediate outputs
	@-rm -rf $(OTHER_OUTPUTS)
	@-rm -rf $(PROJECT_NAME).lk
	@-rm -rf $(PROJECT_NAME).map	
	@-rm -rf $(PROJECT_NAME).cdb	
	@-rm -rf $(PROJECT_NAME).hex
	@-rm -rf $(PROJECT_NAME).bin
	@-rm -rf $(PROJECT_NAME).mem
	@-rm -rf $(DEPS)