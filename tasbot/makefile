# Makefile made by tom7.
default: playfun learnfun showfun
# tasbot

all: playfun objective_test learnfun weighted-objectives_test showfun tasbot

#CXXFLAGS=-Wall -Wno-sign-compare -I/usr/local/include -fno-strict-aliasing
CXXFLAGS=-Wall -Wno-sign-compare -I/usr/local/include
CXX=g++
CC=g++

# If you don't have protobuf, SDL and SDL_net, you can comment these out.
CCNETWORKING=-DMARIONET -I /usr/include/SDL
LINKNETWORKING=-lSDL -lSDL_net -lprotobuf -lpthread
PROTOC=protoc
PROTO_OBJECTS=marionet.pb.o
MARIONET_OBJECTS=$(PROTO_OBJECTS) netutil.o
SDLARCH=x64

#PROFILE=-pg -g
#PROFILE=-g
PROFILE=

# compile and link time optimization
# -flto is not compatible with -fwhole-program
#OPT=-O0
OPT=-O2
#OPT=-O2 -flto
#LOPT=-O0
LOPT=-O2 -fwhole-program
#LOPT=-O2 -flto

INCLUDES=-I "../cc-lib" -I "../cc-lib/city"
#DEFINES=-DPSS_STYLE=1 -DDUMMY_UI -DHAVE_ALLOCA -DNOEMUCACHE -DNOWINSTUFF -DNOUNZIP
DEFINES=-DPSS_STYLE=1 -DDUMMY_UI -DNOEMUCACHE
#DEFINES=-DPSS_STYLE=1 -DDUMMY_UI

CPPFLAGS= $(CCNETWORKING) $(DEFINES) -m64 $(INCLUDES) $(PROFILE) $(OPT)

#LFLAGS=$(LINKNETWORKING) -lz $(PROFILE) $(LOPT) -Wl,--subsystem,console
LFLAGS=$(LINKNETWORKING) -lz $(PROFILE) $(LOPT)

CCLIBOBJECTS=../cc-lib/util.o ../cc-lib/arcfour.o ../cc-lib/base/stringprintf.o ../cc-lib/city/city.o ../cc-lib/textsvg.o

FCEUOBJECTS=fceu/asm.o fceu/cart.o fceu/cheat.o fceu/conddebug.o fceu/config.o fceu/debug.o fceu/drawing.o fceu/emufile.o fceu/fceu.o fceu/fds.o fceu/file.o fceu/filter.o fceu/ines.o fceu/input.o fceu/movie.o fceu/netplay.o fceu/nsf.o fceu/oldmovie.o fceu/palette.o fceu/ppu.o fceu/sound.o fceu/state.o fceu/unif.o fceu/video.o fceu/vsuni.o fceu/wave.o fceu/x6502.o

BOARDSOBJECTS=fceu/boards/01-222.o fceu/boards/32.o fceu/boards/gs-2013.o fceu/boards/103.o fceu/boards/33.o fceu/boards/h2288.o fceu/boards/106.o fceu/boards/34.o fceu/boards/karaoke.o fceu/boards/108.o fceu/boards/3d-block.o fceu/boards/kof97.o fceu/boards/112.o fceu/boards/411120-c.o fceu/boards/konami-qtai.o fceu/boards/116.o fceu/boards/43.o fceu/boards/ks7012.o fceu/boards/117.o fceu/boards/57.o fceu/boards/ks7013.o fceu/boards/120.o fceu/boards/603-5052.o fceu/boards/ks7017.o fceu/boards/121.o fceu/boards/68.o fceu/boards/ks7030.o fceu/boards/12in1.o fceu/boards/8157.o fceu/boards/ks7031.o fceu/boards/15.o fceu/boards/82.o fceu/boards/ks7032.o fceu/boards/151.o fceu/boards/8237.o fceu/boards/ks7037.o fceu/boards/156.o fceu/boards/830118C.o fceu/boards/ks7057.o fceu/boards/164.o fceu/boards/88.o fceu/boards/le05.o fceu/boards/168.o fceu/boards/90.o fceu/boards/lh32.o fceu/boards/17.o fceu/boards/91.o fceu/boards/lh53.o fceu/boards/170.o fceu/boards/95.o fceu/boards/malee.o fceu/boards/175.o fceu/boards/96.o fceu/boards/mmc1.o fceu/boards/176.o fceu/boards/99.o fceu/boards/mmc3.o fceu/boards/177.o fceu/boards/__dummy_mapper.o fceu/boards/mmc5.o fceu/boards/178.o fceu/boards/a9711.o fceu/boards/n-c22m.o fceu/boards/179.o fceu/boards/a9746.o fceu/boards/n106.o fceu/boards/18.o fceu/boards/ac-08.o fceu/boards/n625092.o fceu/boards/183.o fceu/boards/addrlatch.o fceu/boards/novel.o fceu/boards/185.o fceu/boards/ax5705.o fceu/boards/onebus.o fceu/boards/186.o fceu/boards/bandai.o fceu/boards/pec-586.o fceu/boards/187.o fceu/boards/bb.o fceu/boards/sa-9602b.o fceu/boards/189.o fceu/boards/bmc13in1jy110.o fceu/boards/sachen.o fceu/boards/193.o fceu/boards/bmc42in1r.o fceu/boards/sc-127.o fceu/boards/199.o fceu/boards/bmc64in1nr.o fceu/boards/sheroes.o fceu/boards/208.o fceu/boards/bmc70in1.o fceu/boards/sl1632.o fceu/boards/222.o fceu/boards/bonza.o fceu/boards/smb2j.o fceu/boards/225.o fceu/boards/bs-5.o fceu/boards/subor.o fceu/boards/228.o fceu/boards/cityfighter.o fceu/boards/super24.o fceu/boards/230.o fceu/boards/dance2000.o fceu/boards/supervision.o fceu/boards/232.o fceu/boards/datalatch.o fceu/boards/t-227-1.o fceu/boards/234.o fceu/boards/deirom.o fceu/boards/t-262.o fceu/boards/235.o fceu/boards/dream.o fceu/boards/tengen.o fceu/boards/244.o fceu/boards/edu2000.o fceu/boards/tf-1201.o fceu/boards/246.o fceu/boards/famicombox.o fceu/boards/transformer.o fceu/boards/252.o fceu/boards/fk23c.o fceu/boards/vrc2and4.o fceu/boards/253.o fceu/boards/ghostbusters63in1.o fceu/boards/vrc7.o fceu/boards/28.o fceu/boards/gs-2004.o fceu/boards/yoko.o

# fceu/drivers/common/config.o fceu/drivers/common/configSys.o
DRIVERS_COMMON_OBJECTS=fceu/drivers/common/args.o fceu/drivers/common/nes_ntsc.o fceu/drivers/common/cheat.o fceu/drivers/common/scale2x.o  fceu/drivers/common/scale3x.o fceu/drivers/common/scalebit.o fceu/drivers/common/hq2x.o fceu/drivers/common/vidblit.o fceu/drivers/common/hq3x.o

INPUTOBJECTS=fceu/input/arkanoid.o fceu/input/ftrainer.o fceu/input/oekakids.o fceu/input/suborkb.o fceu/input/bworld.o fceu/input/hypershot.o fceu/input/powerpad.o fceu/input/toprider.o fceu/input/cursor.o fceu/input/mahjong.o fceu/input/quiz.o fceu/input/zapper.o fceu/input/fkb.o fceu/input/mouse.o fceu/input/shadow.o

MAPPEROBJECTS=fceu/mappers/24and26.o fceu/mappers/51.o fceu/mappers/69.o fceu/mappers/77.o fceu/mappers/40.o fceu/mappers/6.o fceu/mappers/71.o fceu/mappers/79.o fceu/mappers/41.o fceu/mappers/61.o fceu/mappers/72.o fceu/mappers/80.o fceu/mappers/42.o fceu/mappers/62.o fceu/mappers/73.o fceu/mappers/85.o fceu/mappers/46.o fceu/mappers/65.o fceu/mappers/75.o fceu/mappers/emu2413.o fceu/mappers/50.o fceu/mappers/67.o fceu/mappers/76.o fceu/mappers/mmc2and4.o

PALETTESOBJECTS=fceu/palettes/conv.o

UTILSOBJECTS=fceu/utils/ConvertUTF.o fceu/utils/general.o fceu/utils/memory.o fceu/utils/crc32.o fceu/utils/guid.o fceu/utils/endian.o fceu/utils/md5.o fceu/utils/xstring.o fceu/utils/unzip.o

EMUOBJECTS=$(FCEUOBJECTS) $(MAPPEROBJECTS) $(UTILSOBJECTS) $(PALLETESOBJECTS) $(BOARDSOBJECTS) $(INPUTOBJECTS) $(DRIVERS_COMMON_OBJECTS)

TASBOT_OBJECTS=$(MARIONET_OBJECTS) headless-driver.o config.o simplefm2.o emulator.o basis-util.o objective.o weighted-objectives.o motifs.o util.o

%.o: %.cc %.cpp %.h
	$(CXX) -o $@ -c $< $(CPPFLAGS)

#included in all tests, etc.
OBJECTS=$(CCLIBOBJECTS) $(EMUOBJECTS) $(TASBOT_OBJECTS)

%.pb.cc: %.proto
	$(PROTOC) $< --cpp_out=.

tasbot : $(OBJECTS) tasbot.o
	$(CXX) $^ -o $@ $(LFLAGS)

learnfun : $(OBJECTS) learnfun.o
	$(CXX) $^ -o $@ $(LFLAGS)

playfun : $(OBJECTS) playfun.o
	$(CXX) $^ -o $@ $(LFLAGS)

showfun : $(OBJECTS) showfun.o
	$(CXX) $^ -o $@ $(LFLAGS)

objective_test : $(OBJECTS) objective_test.o
	$(CXX) $^ -o $@ $(LFLAGS)

weighted-objectives_test : $(OBJECTS) weighted-objectives_test.o
	$(CXX) $^ -o $@ $(LFLAGS)

test : objective_test weighted-objectives_test
	time ./objective_test
	time ./weighted-objectives_test

clean :
	rm -f learnfun playfun showfun tasbot *_test $(OBJECTS) tasbot.o learnfun.o playfun.o showfun.o objective.o objective_test.o weighted-objectives.o weighted-objectives_test.o gmon.out

veryclean : clean cleantas

cleantas :
	rm -f prog*.fm2 deepest.fm2 heuristicest.fm2

