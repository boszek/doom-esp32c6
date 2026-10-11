# Build silnika na PC bez SDL/dzwieku. Uzycie: make -C host
ENGINE = ../third_party/doomgeneric/doomgeneric
OBJDIR ?= build
OUTPUT ?= doomhost
CC     = gcc
CFLAGS += -MMD -MP -O2 -g -w -I$(ENGINE) -DNORMALUNIX -DLINUX -D_DEFAULT_SOURCE $(EXTRA_CFLAGS)
LIBS   = -lm

SRC_DOOM = dummy am_map doomdef doomstat dstrings d_event d_items d_iwad d_loop d_main d_mode d_net f_finale f_wipe g_game hu_lib hu_stuff info i_cdmus i_endoom i_joystick i_scale i_sound i_system i_timer memio m_argv m_bbox m_cheat m_config m_controls m_fixed m_menu m_misc m_random p_ceilng p_doors p_enemy p_floor p_inter p_lights p_map p_maputl p_mobj p_plats p_pspr p_saveg p_setup p_sight p_spec p_switch p_telept p_tick p_user r_bsp r_data r_draw r_main r_plane r_segs r_sky r_things sha1 sounds statdump st_lib st_stuff s_sound_stub tables v_video wi_stuff w_checksum w_file w_main w_wad z_zone w_file_stdc w_file_mem i_input i_video doomgeneric dg_mono
OBJS = $(addprefix $(OBJDIR)/, $(addsuffix .o, $(SRC_DOOM))) $(OBJDIR)/doomgeneric_pbm.o

all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LIBS)

$(OBJDIR)/doomgeneric_pbm.o: doomgeneric_pbm.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(ENGINE)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf build build_mono build_mono32 doomhost doommono doommono32

-include $(OBJS:.o=.d)

# wersja 128x64 1-bit: make -C host mono
MONO_FLAGS = -DDG_MONO -DSCREENWIDTH=128 -DSCREENHEIGHT=64
mono:
	$(MAKE) OBJDIR=build_mono OUTPUT=doommono EXTRA_CFLAGS="$(MONO_FLAGS) $(EXTRA_CFLAGS)" all

# jak mono, ale 32-bitowo (ILP32 jak RV32) przez zig cc: realistyczny pomiar pamieci
# wymaga: python3 -m venv <dir> && <dir>/bin/pip install ziglang ; ZIG=<dir>/bin/python
ZIG ?= python3
mono32:
	$(MAKE) OBJDIR=build_mono32 OUTPUT=doommono32 CC="$(ZIG) -m ziglang cc -target x86-linux-musl" EXTRA_CFLAGS="$(MONO_FLAGS) $(EXTRA_CFLAGS)" all
