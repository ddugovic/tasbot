# Makefile made by tom7.
default: playfun learnfun
# tasbot
# emu_test

all: playfun tasbot emu_test objective_test learnfun weighted-objectives_test

# GPP=

# mlton executes this:
# x86_64-w64-mingw32-gcc -std=gnu99 -c -Ic:\program files (x86)\mlton\lib\mlton\targets\x86_64-w64-mingw32\include -IC:/Program Files (x86)/MLton/lib/mlton/include -O1 -fno-common -fno-strict-aliasing -fomit-frame-pointer -w -m64 -o C:\Users\Tom\AppData\Local\Temp\file17jU3c.o x6502.c

CXXFLAGS=-Wall -Wno-deprecated -Wno-sign-compare -I/usr/local/include -fno-strict-aliasing

# for 64 bits on windows
CXX=g++
CC=g++
SDLARCH=x64

# not using the one in protobuf/src because it doesn't work?
PROTOC=protoc

# -Wl,--subsystem,console

PROTO_HEADERS=marionet.pb.h
PROTO_OBJECTS=marionet.pb.o


## If you don't have SDL, you can leave these out, and maybe it still works.
CCNETWORKING= -DMARIONET=1 -I /usr/include/SDL
LINKSDL=  -lSDL -lSDL_net
LINKNETWORKING= $(LINKSDL) -lprotobuf -lpthread
SDLOPATH=SDL/build
#SDLOBJECTS=$(SDLOPATH)/SDL.o $(SDLOPATH)/SDL_error.o $(SDLOPATH)/SDL_fatal.o $(SDLOPATH)/SDL_audio.o $(SDLOPATH)/SDL_audiocvt.o $(SDLOPATH)/SDL_audiodev.o $(SDLOPATH)/SDL_mixer.o $(SDLOPATH)/SDL_mixer_MMX.o $(SDLOPATH)/SDL_mixer_MMX_VC.o $(SDLOPATH)/SDL_mixer_m68k.o $(SDLOPATH)/SDL_wave.o $(SDLOPATH)/SDL_cdrom.o $(SDLOPATH)/SDL_cpuinfo.o $(SDLOPATH)/SDL_active.o $(SDLOPATH)/SDL_events.o $(SDLOPATH)/SDL_expose.o $(SDLOPATH)/SDL_keyboard.o $(SDLOPATH)/SDL_mouse.o $(SDLOPATH)/SDL_quit.o $(SDLOPATH)/SDL_resize.o $(SDLOPATH)/SDL_rwops.o $(SDLOPATH)/SDL_getenv.o $(SDLOPATH)/SDL_iconv.o $(SDLOPATH)/SDL_malloc.o $(SDLOPATH)/SDL_qsort.o $(SDLOPATH)/SDL_stdlib.o $(SDLOPATH)/SDL_string.o $(SDLOPATH)/SDL_thread.o $(SDLOPATH)/SDL_timer.o $(SDLOPATH)/SDL_RLEaccel.o $(SDLOPATH)/SDL_blit.o $(SDLOPATH)/SDL_blit_0.o $(SDLOPATH)/SDL_blit_1.o $(SDLOPATH)/SDL_blit_A.o $(SDLOPATH)/SDL_blit_N.o $(SDLOPATH)/SDL_bmp.o $(SDLOPATH)/SDL_cursor.o $(SDLOPATH)/SDL_gamma.o $(SDLOPATH)/SDL_pixels.o $(SDLOPATH)/SDL_stretch.o $(SDLOPATH)/SDL_surface.o $(SDLOPATH)/SDL_video.o $(SDLOPATH)/SDL_yuv.o $(SDLOPATH)/SDL_yuv_mmx.o $(SDLOPATH)/SDL_yuv_sw.o $(SDLOPATH)/SDL_joystick.o $(SDLOPATH)/SDL_nullevents.o $(SDLOPATH)/SDL_nullmouse.o $(SDLOPATH)/SDL_nullvideo.o $(SDLOPATH)/SDL_diskaudio.o $(SDLOPATH)/SDL_dummyaudio.o $(SDLOPATH)/SDL_sysevents.o $(SDLOPATH)/SDL_sysmouse.o $(SDLOPATH)/SDL_syswm.o $(SDLOPATH)/SDL_wingl.o $(SDLOPATH)/SDL_dibevents.o $(SDLOPATH)/SDL_dibvideo.o $(SDLOPATH)/SDL_dx5events.o $(SDLOPATH)/SDL_dx5video.o $(SDLOPATH)/SDL_dx5yuv.o $(SDLOPATH)/SDL_dibaudio.o $(SDLOPATH)/SDL_dx5audio.o $(SDLOPATH)/SDL_mmjoystick.o $(SDLOPATH)/SDL_syscdrom.o $(SDLOPATH)/SDL_sysmutex.o $(SDLOPATH)/SDL_syssem.o $(SDLOPATH)/SDL_systhread.o $(SDLOPATH)/SDL_syscond.o $(SDLOPATH)/SDL_systimer.o $(SDLOPATH)/SDL_sysloadso.o
# For some reason this compiles as 32-bit? But it's unused.
# $(SDLOPATH)/version.o
NETWORKINGOBJECTS= $(PROTO_OBJECTS) netutil.o

#PROTOBUFOBJECTS=protobuf/src/code_generator.o protobuf/src/coded_stream.o protobuf/src/common.o protobuf/src/cpp_enum.o protobuf/src/cpp_enum_field.o protobuf/src/cpp_extension.o protobuf/src/cpp_field.o protobuf/src/cpp_file.o protobuf/src/cpp_generator.o protobuf/src/cpp_helpers.o protobuf/src/cpp_message.o protobuf/src/cpp_message_field.o protobuf/src/cpp_primitive_field.o protobuf/src/cpp_service.o protobuf/src/cpp_string_field.o protobuf/src/descriptor.o protobuf/src/descriptor.pb.o protobuf/src/descriptor_database.o protobuf/src/dynamic_message.o protobuf/src/extension_set.o protobuf/src/extension_set_heavy.o protobuf/src/generated_message_reflection.o protobuf/src/generated_message_util.o protobuf/src/gzip_stream.o protobuf/src/importer.o protobuf/src/java_enum.o protobuf/src/java_enum_field.o protobuf/src/java_extension.o protobuf/src/java_field.o protobuf/src/java_file.o protobuf/src/java_generator.o protobuf/src/java_helpers.o protobuf/src/java_message.o protobuf/src/java_message_field.o protobuf/src/java_primitive_field.o protobuf/src/java_service.o protobuf/src/java_string_field.o protobuf/src/message.o protobuf/src/message_lite.o protobuf/src/once.o protobuf/src/parser.o protobuf/src/plugin.o protobuf/src/plugin.pb.o protobuf/src/printer.o protobuf/src/python_generator.o protobuf/src/reflection_ops.o protobuf/src/repeated_field.o protobuf/src/service.o protobuf/src/structurally_valid.o protobuf/src/strutil.o protobuf/src/subprocess.o protobuf/src/substitute.o protobuf/src/text_format.o protobuf/src/tokenizer.o protobuf/src/unknown_field_set.o protobuf/src/wire_format.o protobuf/src/wire_format_lite.o protobuf/src/zero_copy_stream.o protobuf/src/zero_copy_stream_impl.o protobuf/src/zero_copy_stream_impl_lite.o protobuf/src/zip_writer.o

# PROFILE=-pg
PROFILE=

OPT=-O2

# enable link time optimizations?
# FLTO=-flto
# FLTO=

INCLUDES=-I "../cc-lib" -I "../cc-lib/city"

#  -DNOUNZIP
CPPFLAGS= $(CCNETWORKING) -DPSS_STYLE=1 -DDUMMY_UI -DHAVE_ASPRINTF -Wno-write-strings -m64 $(OPT) -DHAVE_ALLOCA -DNOWINSTUFF $(INCLUDES) $(PROFILE) $(FLTO)
#  CPPFLAGS=-DPSS_STYLE=1 -DDUMMY_UI -DHAVE_ASPRINTF -Wno-write-strings -m64 -O -DHAVE_ALLOCA -DNOWINSTUFF $(PROFILE) -g

CCLIBOBJECTS=../cc-lib/util.o ../cc-lib/arcfour.o ../cc-lib/base/stringprintf.o ../cc-lib/city/city.o ../cc-lib/textsvg.o

MAPPEROBJECTS=fceu/mappers/24and26.o fceu/mappers/51.o fceu/mappers/69.o fceu/mappers/77.o fceu/mappers/40.o fceu/mappers/6.o fceu/mappers/71.o fceu/mappers/79.o fceu/mappers/41.o fceu/mappers/61.o fceu/mappers/72.o fceu/mappers/80.o fceu/mappers/42.o fceu/mappers/62.o fceu/mappers/73.o fceu/mappers/85.o fceu/mappers/46.o fceu/mappers/65.o fceu/mappers/75.o fceu/mappers/emu2413.o fceu/mappers/50.o fceu/mappers/67.o fceu/mappers/76.o fceu/mappers/mmc2and4.o

# utils/unzip.o removed -- needs lz
UTILSOBJECTS=fceu/utils/ConvertUTF.o fceu/utils/general.o fceu/utils/memory.o fceu/utils/crc32.o fceu/utils/guid.o fceu/utils/endian.o fceu/utils/md5.o fceu/utils/xstring.o fceu/utils/unzip.o

# main binary
# PALETTESOBJECTS=palettes/conv.o

BOARDSOBJECTS=fceu/boards/01-222.o fceu/boards/32.o fceu/boards/gs-2013.o fceu/boards/103.o fceu/boards/33.o fceu/boards/h2288.o fceu/boards/106.o fceu/boards/34.o fceu/boards/karaoke.o fceu/boards/108.o fceu/boards/3d-block.o fceu/boards/kof97.o fceu/boards/112.o fceu/boards/411120-c.o fceu/boards/konami-qtai.o fceu/boards/116.o fceu/boards/43.o fceu/boards/ks7012.o fceu/boards/117.o fceu/boards/57.o fceu/boards/ks7013.o fceu/boards/120.o fceu/boards/603-5052.o fceu/boards/ks7017.o fceu/boards/121.o fceu/boards/68.o fceu/boards/ks7030.o fceu/boards/12in1.o fceu/boards/8157.o fceu/boards/ks7031.o fceu/boards/15.o fceu/boards/82.o fceu/boards/ks7032.o fceu/boards/151.o fceu/boards/8237.o fceu/boards/ks7037.o fceu/boards/156.o fceu/boards/830118C.o fceu/boards/ks7057.o fceu/boards/164.o fceu/boards/88.o fceu/boards/le05.o fceu/boards/168.o fceu/boards/90.o fceu/boards/lh32.o fceu/boards/17.o fceu/boards/91.o fceu/boards/lh53.o fceu/boards/170.o fceu/boards/95.o fceu/boards/malee.o fceu/boards/175.o fceu/boards/96.o fceu/boards/mmc1.o fceu/boards/176.o fceu/boards/99.o fceu/boards/mmc3.o fceu/boards/177.o fceu/boards/__dummy_mapper.o fceu/boards/mmc5.o fceu/boards/178.o fceu/boards/a9711.o fceu/boards/n-c22m.o fceu/boards/179.o fceu/boards/a9746.o fceu/boards/n106.o fceu/boards/18.o fceu/boards/ac-08.o fceu/boards/n625092.o fceu/boards/183.o fceu/boards/addrlatch.o fceu/boards/novel.o fceu/boards/185.o fceu/boards/ax5705.o fceu/boards/onebus.o fceu/boards/186.o fceu/boards/bandai.o fceu/boards/pec-586.o fceu/boards/187.o fceu/boards/bb.o fceu/boards/sa-9602b.o fceu/boards/189.o fceu/boards/bmc13in1jy110.o fceu/boards/sachen.o fceu/boards/193.o fceu/boards/bmc42in1r.o fceu/boards/sc-127.o fceu/boards/199.o fceu/boards/bmc64in1nr.o fceu/boards/sheroes.o fceu/boards/208.o fceu/boards/bmc70in1.o fceu/boards/sl1632.o fceu/boards/222.o fceu/boards/bonza.o fceu/boards/smb2j.o fceu/boards/225.o fceu/boards/bs-5.o fceu/boards/subor.o fceu/boards/228.o fceu/boards/cityfighter.o fceu/boards/super24.o fceu/boards/230.o fceu/boards/dance2000.o fceu/boards/supervision.o fceu/boards/232.o fceu/boards/datalatch.o fceu/boards/t-227-1.o fceu/boards/234.o fceu/boards/deirom.o fceu/boards/t-262.o fceu/boards/235.o fceu/boards/dream.o fceu/boards/tengen.o fceu/boards/244.o fceu/boards/edu2000.o fceu/boards/tf-1201.o fceu/boards/246.o fceu/boards/famicombox.o fceu/boards/transformer.o fceu/boards/252.o fceu/boards/fk23c.o fceu/boards/vrc2and4.o fceu/boards/253.o fceu/boards/ghostbusters63in1.o fceu/boards/vrc7.o fceu/boards/28.o fceu/boards/gs-2004.o fceu/boards/yoko.o

INPUTOBJECTS=fceu/input/arkanoid.o fceu/input/ftrainer.o fceu/input/oekakids.o fceu/input/suborkb.o fceu/input/bworld.o fceu/input/hypershot.o fceu/input/powerpad.o fceu/input/toprider.o fceu/input/cursor.o fceu/input/mahjong.o fceu/input/quiz.o fceu/input/zapper.o fceu/input/fkb.o fceu/input/mouse.o fceu/input/shadow.o

FCEUOBJECTS=fceu/asm.o fceu/cart.o fceu/cheat.o fceu/conddebug.o fceu/config.o fceu/debug.o fceu/drawing.o fceu/emufile.o fceu/fceu.o fceu/fds.o fceu/file.o fceu/filter.o fceu/ines.o fceu/input.o fceu/movie.o fceu/netplay.o fceu/nsf.o fceu/oldmovie.o fceu/palette.o fceu/ppu.o fceu/sound.o fceu/state.o fceu/unif.o fceu/video.o fceu/vsuni.o fceu/wave.o fceu/x6502.o

# fceu/drivers/common/config.o fceu/drivers/common/configSys.o
DRIVERS_COMMON_OBJECTS=fceu/drivers/common/args.o fceu/drivers/common/nes_ntsc.o fceu/drivers/common/cheat.o fceu/drivers/common/scale2x.o  fceu/drivers/common/scale3x.o fceu/drivers/common/scalebit.o fceu/drivers/common/hq2x.o fceu/drivers/common/vidblit.o fceu/drivers/common/hq3x.o

EMUOBJECTS=$(FCEUOBJECTS) $(MAPPEROBJECTS) $(UTILSOBJECTS) $(PALLETESOBJECTS) $(BOARDSOBJECTS) $(INPUTOBJECTS) $(DRIVERS_COMMON_OBJECTS)

#included in all tests, etc.
BASEOBJECTS=$(CCLIBOBJECTS) $(NETWORKINGOBJECTS) $(PROTOBUFOBJECTS)

TASBOT_OBJECTS=headless-driver.o config.o simplefm2.o emulator.o basis-util.o objective.o weighted-objectives.o motifs.o util.o

OBJECTS=$(BASEOBJECTS) $(EMUOBJECTS) $(TASBOT_OBJECTS)

%.pb.cc: %.proto
	$(PROTOC) $< --cpp_out=.

%.pb.h: %.proto
	$(PROTOC) $< --cpp_out=.

# without static, can't find lz or lstdcxx maybe?
LFLAGS =  -m64 $(LINKNETWORKING) -lz $(OPT) $(FLTO) $(PROFILE)
# -Wl,--subsystem,console
# -static -fwhole-program
# -static

learnfun : $(OBJECTS) learnfun.o
	$(CXX) $^ -o $@ $(LFLAGS)

# XXX never implemented this.
showfun : $(OBJECTS) showfun.o
	$(CXX) $^ -o $@ $(LFLAGS)

tasbot : $(OBJECTS) tasbot.o
	$(CXX) $^ -o $@ $(LFLAGS)

playfun : $(OBJECTS) playfun.o
	$(CXX) $^ -o $@ $(LFLAGS)

emu_test : $(OBJECTS) emu_test.o
	$(CXX) $^ -o $@ $(LFLAGS)

objective_test : $(BASEOBJECTS) objective.o objective_test.o
	$(CXX) $^ -o $@ $(LFLAGS)

weighted-objectives_test : $(BASEOBJECTS) weighted-objectives.o weighted-objectives_test.o util.o
	$(CXX) $^ -o $@ $(LFLAGS)

test : emu_test objective_test weighted-objectives_test
	time ./emu_test
	time ./objective_test
	time ./weighted-objectives_test

clean :
	rm -f learnfun playfun showfun *_test *.o $(EMUOBJECTS) $(CCLIBOBJECTS) gmon.out

veryclean : clean cleantas

cleantas :
	rm -f prog*.fm2 deepest.fm2 heuristicest.fm2

