# manga-reader

A lightweight manga reader built in C with GTK2, designed Kindle but also runs natively on macOS and Linux.

## How to Install

Download `manga-reader-kindle.zip` from the [latest release](../../releases/latest).

Install on a jailbroken Kindle with KUAL: unzip into the Kindle USB root and launch from the KUAL menu.

## Build

**Mac**

Install dependencies:

```sh
brew install meson ninja gtk+ curl libxml2
```

Build and run:

```sh
./scripts/build-mac.sh
```

**Linux**

Install dependencies:

```sh
apt install build-essential meson ninja-build libgtk2.0-dev libcurl4-openssl-dev libxml2-dev
```

Build and run:

```sh
meson setup builddir && ninja -C builddir
./builddir/manga-reader
```
