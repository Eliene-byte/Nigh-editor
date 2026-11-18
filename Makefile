CROSS ?= i686-w64-mingw32
WINCXX ?= $(CROSS)-g++
WINCC ?= $(CROSS)-gcc
WINDRES ?= $(CROSS)-windres
OUT ?= build
WINCPP := -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_WIN32_WINNT=0x0501 -DWINVER=0x0501
WINCXXFLAGS := -std=c++11 -march=pentium-m -Os -Wall -Wextra -ffunction-sections -fdata-sections -MMD -MP
WINLD := -static -static-libgcc -static-libstdc++ -municode -Wl,--gc-sections,--major-os-version,5,--minor-os-version,1,--major-subsystem-version,5,--minor-subsystem-version,1 -s
WINLIBS := -lcomdlg32 -lgdi32 -lshell32 -lwinmm -luser32
SOURCES := main app ui dialogs draw job player media comp assist local_models win core
OBJECTS := $(addprefix $(OUT)/obj/,$(addsuffix .o,$(SOURCES)))
VAD := src/vendor/vad
VADS := $(wildcard $(VAD)/webrtc/common_audio/vad/*.c $(VAD)/webrtc/common_audio/signal_processing/*.c)
VADWIN := $(patsubst $(VAD)/%.c,$(OUT)/obj/vad/%.o,$(VADS))
VADHOST := $(patsubst $(VAD)/%.c,$(OUT)/vad-host/%.o,$(VADS))

.PHONY: all codecs test smoke audit package clean
all: $(OUT)/NighEditor.exe

$(OUT)/obj/%.o: src/%.cpp
	@mkdir -p "$(@D)"
	$(WINCXX) $(WINCPP) $(WINCXXFLAGS) -I$(VAD) -c "$<" -o "$@"

$(OUT)/obj/vad/%.o: $(VAD)/%.c
	@mkdir -p "$(@D)"
	$(WINCC) $(WINCPP) -std=gnu99 -march=pentium-m -O2 -fwrapv -ffunction-sections -fdata-sections -MMD -MP -I$(VAD) -c "$<" -o "$@"

$(OUT)/vad-host/%.o: $(VAD)/%.c
	@mkdir -p "$(@D)"
	$(CC) -DWEBRTC_POSIX -std=gnu99 -O2 -fwrapv -MMD -MP -I$(VAD) -c "$<" -o "$@"

$(OUT)/obj/nigh.res: src/nigh.rc src/resources/nigh.ico
	@mkdir -p "$(@D)"
	$(WINDRES) -Isrc -i "$<" -O coff -o "$@"

$(OUT)/NighEditor.exe: $(OBJECTS) $(VADWIN) $(OUT)/obj/nigh.res
	$(WINCXX) $(WINLD) -mwindows $^ $(WINLIBS) -o "$@"

codecs:
	CROSS="$(CROSS)" BIN_DIR="$(OUT)/codecs" tools/build-codecs.sh

$(OUT)/core-test: tests/core_test.cpp src/core.cpp src/core.hpp
	@mkdir -p "$(@D)"
	$(CXX) -std=c++11 -O2 -Wall -Wextra -pedantic -Isrc tests/core_test.cpp src/core.cpp -o "$@"

$(OUT)/models-test: tests/models_test.cpp src/local_models.cpp src/local_models.hpp src/cut_weights.hpp src/core.hpp $(VADHOST)
	$(CXX) -std=c++11 -O2 -Wall -Wextra -pedantic -Isrc -I$(VAD) tests/models_test.cpp src/local_models.cpp $(VADHOST) -pthread -o "$@"

test: $(OUT)/core-test $(OUT)/models-test
	"$(OUT)/core-test"
	"$(OUT)/models-test"

$(OUT)/obj/smoke.o: tests/smoke_win.cpp
	@mkdir -p "$(@D)"
	$(WINCXX) $(WINCPP) $(WINCXXFLAGS) -Isrc -c "$<" -o "$@"

$(OUT)/smoke.exe: $(OUT)/obj/smoke.o $(OUT)/obj/media.o $(OUT)/obj/comp.o $(OUT)/obj/assist.o $(OUT)/obj/local_models.o $(OUT)/obj/win.o $(OUT)/obj/core.o $(VADWIN)
	$(WINCXX) $(WINLD) -mconsole $^ -lgdi32 -luser32 -o "$@"

$(OUT)/obj/gui-smoke.o: tests/gui_win.cpp
	@mkdir -p "$(@D)"
	$(WINCXX) $(WINCPP) $(WINCXXFLAGS) -Isrc -c "$<" -o "$@"

$(OUT)/gui-smoke.exe: $(OUT)/obj/gui-smoke.o $(OUT)/obj/win.o $(OUT)/obj/core.o
	$(WINCXX) $(WINLD) -mconsole $^ -lgdi32 -luser32 -o "$@"

smoke: $(OUT)/smoke.exe $(OUT)/gui-smoke.exe

audit: all codecs
	tools/check-pe.sh "$(OUT)"

package: all codecs
	BIN_DIR="$(OUT)" tools/package.sh

clean:
	rm -rf -- "$(OUT)/obj" "$(OUT)/codecs" "$(OUT)/vad-host"
	rm -f -- "$(OUT)/NighEditor.exe" "$(OUT)/core-test" "$(OUT)/models-test" "$(OUT)/smoke.exe" "$(OUT)/gui-smoke.exe"

-include $(OBJECTS:.o=.d) $(VADWIN:.o=.d) $(VADHOST:.o=.d) $(OUT)/obj/smoke.d $(OUT)/obj/gui-smoke.d
