CC      = g++
CFLAGS  = -O3 -mavx -std=c++14 -w
XFLAGS  = -O3 -mavx -std=c++17 -w -DWORKLOAD_COUNT -I.

SOURCES = indexes/containers/relation.cpp indexes/containers/buffer.cpp indexes/LIT/hierarchicalindex.cpp indexes/LIT/hint_m_dynamic.cpp indexes/LIT/live_index.cpp
OBJECTS = $(SOURCES:.cpp=.o)

all: competitors_all static_delta_all adaptive_all learned_all

# HINT (as used inside LIT) and R*-tree
LIT: $(OBJECTS)
	$(CC) $(XFLAGS) $(OBJECTS) experiments/competitors/main_LIT.cpp -o lit.exe

rtree: $(OBJECTS)
	$(CC) $(XFLAGS) $(OBJECTS) experiments/competitors/main_rtree.cpp -o rtree.exe

competitors_all: LIT rtree

# TIDY variants: TIDY-delta (oracle), TIDY-Loc (adaptive), TIDY-Lrn (learned)
static_delta_all: $(OBJECTS)
	$(CC) $(XFLAGS) $(OBJECTS) experiments/tidy_variants/main_static_delta.cpp -o static_delta.exe

adaptive_all: $(OBJECTS)
	$(CC) $(XFLAGS) $(OBJECTS) experiments/tidy_variants/main_adaptive.cpp -o adaptive.exe

learned_all: $(OBJECTS)
	$(CC) $(XFLAGS) $(OBJECTS) experiments/learned/main.cpp -o learned.exe

.cpp.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f indexes/containers/*.o indexes/LIT/*.o *.exe
	find experiments -name '*.exe' -delete

.PHONY: all LIT rtree competitors_all static_delta_all adaptive_all learned_all clean
