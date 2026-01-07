# Linux only

.PHONY: build run_debug run_dist clean run_bench run_perft

BUILD_CONFIG_FILES := CMakeLists.txt src/CMakeLists.txt vendor/CMakeLists.txt .gitmodules

dist: config_dist build

debug: config_debug build

build/Debug: $(BUILD_CONFIG_FILES)
	make clean
	cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

config_debug: build/Debug

build/Dist: $(BUILD_CONFIG_FILES)
	make clean
	cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Dist

config_dist: build/Dist

build:
	cmake --build build

run_dist: dist
	cd ./build/Dist && ./DODDJ

run_debug: debug
	cd ./build/Debug && ./DODDJ

run_bench: dist
	cd ./build/Dist && nix-shell -p poop --run "poop './DODDJ --benchmark 5000'"

run_perft:
	make clean
	cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
	cmake --build build

	cd ./build/RelWithDebInfo && perf record -F 99 -g ./DODDJ -- --benchmark 5000
	cd ./build/RelWithDebInfo && perf report

clean:
	rm -rf build .cache