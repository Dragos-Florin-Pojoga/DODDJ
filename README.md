
# Building the latest version locally

### Install the SDL3 [dependencies](https://wiki.libsdl.org/SDL3/README-linux):

<details>

<summary>Fedora</summary>

```sh
sudo dnf install gcc git-core make cmake ninja-build \
    alsa-lib-devel fribidi-devel pulseaudio-libs-devel pipewire-devel \
    libX11-devel libXext-devel libXrandr-devel libXcursor-devel libXfixes-devel \
    libXi-devel libXScrnSaver-devel dbus-devel ibus-devel \
    systemd-devel mesa-libGL-devel libxkbcommon-devel mesa-libGLES-devel \
    mesa-libEGL-devel vulkan-devel wayland-devel wayland-protocols-devel \
    libdrm-devel mesa-libgbm-devel libusb1-devel libdecor-devel \
    pipewire-jack-audio-connection-kit-devel liburing-devel
```

</details>

<details>

<summary>Ubuntu/Debian</summary>

```sh
sudo apt-get install build-essential git make \
    pkg-config cmake ninja-build gnome-desktop-testing libasound2-dev libpulse-dev \
    libaudio-dev libfribidi-dev libjack-dev libsndio-dev libx11-dev libxext-dev \
    libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
    libxkbcommon-dev libdrm-dev libgbm-dev libgl1-mesa-dev libgles2-mesa-dev \
    libegl1-mesa-dev libdbus-1-dev libibus-1.0-dev libudev-dev libpipewire-0.3-dev \
    libwayland-dev libdecor-0-dev liburing-dev
```

</details>

### Clone the repo [^full_clone]

[^full_clone]: for development omit the `--shallow-submodules --depth 1` flags

```sh
git clone --recurse-submodules --shallow-submodules --depth 1 https://github.com/Dragos-Florin-Pojoga/DODDJ.git
cd DODDJ
```

<details open>

<summary>Linux</summary>

### Configure, Compile & Run the app

```sh
make run_dist
```

<details>

<summary>Or, manually</summary>

### Configure & Compile

simply run `make` or use `cmake` directly

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Dist # = make config_dist
cmake --build build                                  # = make dist
```

### Run the app

```sh
cd build/Dist
./DODDJ
```

</details>

</details>


<details>

<summary>Windows</summary>

### Configure

Open the `Developer Command Prompt for VS 2022` in the repo dir

```sh
cmake -B build -G "Visual Studio 17 2022" -A x64
```

<details open>

<summary>CMD</summary>

### Compile

```sh
cmake --build build --config Dist
```

### Run the app

```sh
cd build\Dist
DODDJ.exe
```

</details>

<details>

<summary>Visual Studio</summary>

```sh
cd build
DODDJ.sln
```

In the `Solution Explorer` right click the `DODDJ` project and select `Set as Startup Project`. At the top, select the configuration `Dist` and press `CTRL+F5`

</details>

</details>
