CROSS ?= i686-w64-mingw32
WINCXX ?= $(CROSS)-g++
WINDRES ?= $(CROSS)-windres
OUT ?= build
WINCPP := -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_WIN32_WINNT=0x0501 -DWINVER=0x0501
WINCXXFLAGS := -std=c++11 -march=pentium-m -Os -Wall -Wextra -ffunction-sections -fdata-sections -MMD -MP
WINLD := -static -static-libgcc -static-libstdc++ -municode -Wl,--gc-sections,--major-os-version,5,--minor-os-version,1,--major-subsystem-version,5,--minor-subsystem-version,1 -s
WINLIBS := -lcomdlg32 -lgdi32 -lshell32 -lwinmm -luser32
SOURCES := main app ui draw job player media win core
OBJECTS := $(addprefix $(OUT)/obj/,$(addsuffix .o,$(SOURCES)))

.PHONY: all codecs test smoke audit package clean
all: $(OUT)/NighEditor.exe

$(OUT)/obj/%.o: src/%.cpp
	@mkdir -p "$(@D)"
	$(WINCXX) $(WINCPP) $(WINCXXFLAGS) -c "$<" -o "$@"

$(OUT)/obj/nigh.res: src/nigh.rc src/resources/nigh.ico
	@mkdir -p "$(@D)"
	$(WINDRES) -Isrc -i "$<" -O coff -o "$@"

$(OUT)/NighEditor.exe: $(OBJECTS) $(OUT)/obj/nigh.res
	$(WINCXX) $(WINLD) -mwindows $^ $(WINLIBS) -o "$@"

codecs:
	CROSS="$(CROSS)" BIN_DIR="$(OUT)/codecs" tools/build-codecs.sh

$(OUT)/core-test: tests/core_test.cpp src/core.cpp src/core.hpp
	@mkdir -p "$(@D)"
	$(CXX) -std=c++11 -O2 -Wall -Wextra -pedantic -Isrc tests/core_test.cpp src/core.cpp -o "$@"

test: $(OUT)/core-test
	"$(OUT)/core-test"

$(OUT)/obj/smoke.o: tests/smoke_win.cpp
	@mkdir -p "$(@D)"
	$(WINCXX) $(WINCPP) $(WINCXXFLAGS) -Isrc -c "$<" -o "$@"

$(OUT)/smoke.exe: $(OUT)/obj/smoke.o $(OUT)/obj/media.o $(OUT)/obj/win.o $(OUT)/obj/core.o
	$(WINCXX) $(WINLD) -mconsole $^ -o "$@"

smoke: $(OUT)/smoke.exe

audit: all codecs
	tools/check-pe.sh "$(OUT)"

package: all codecs
	BIN_DIR="$(OUT)" tools/package.sh

clean:
	rm -rf -- "$(OUT)/obj" "$(OUT)/codecs"
	rm -f -- "$(OUT)/NighEditor.exe" "$(OUT)/core-test" "$(OUT)/smoke.exe"

-include $(OBJECTS:.o=.d) $(OUT)/obj/smoke.d
