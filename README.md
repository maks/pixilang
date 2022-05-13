Pixilang Programming Language
https://warmplace.ru/soft/pixilang

pixilang3
  /bin - compiled versions;
  /docs - documentation (license, changelog, etc.);
  /examples - *.pixi examples;
  /lib - *.pixi libraries (required for the examples);
  /main - main app cpp file;
  /make - build scripts; use it to build the Pixilang for different platforms with different options;
lib_* - auxiliary libraries for building the Pixilang from source;


---

## Building for Opendingux (MIPS)

You need to have the Opendignux toolchain installed (http://od.abstraction.se/opendingux/toolchain/)
and the `build_opk` script that comes with the toolchain from WerWolv: https://werwolv.net/projects/opendingux_toolchain.

```
cd pixilang3/make
./MAKE_LINUX_OPENDINGUX
cd ..
build_opk pixi.opk bin/linux_mipsel/pixilang.dge  make/resources/pixi.gcw0.desktop
```

then make sure you are connected to the Opendingus device via ssh and to install:

```
scp pixi.opk od@10.1.1.2:/media/data/apps/
```

Note the desktop file needs to define a mimitype and there is none for pixilang's pixi files, so we just need to usue plain text and you need to give your pixilang files a `.txt` extension, which Pixilang also supports.

Then on your Opendingux device (eg. via ssh or file manager on device) create a new directory, eg. `/media/sdcard/roms/PIXI` and then put your pixi source files (remember with `.txt1 extension!) in that directory. Then when you run Pixilang via the devices menu (in applications section) it will show a file browser. Browse to the directory with your pixi source files and choose the one you want to run.


### LIMITATIONS

This has only been tested on a Anbernic RG280v running the latest "beta" version of Opendingux [see her for installation instructions](https://retrogamecorps.com/2020/12/05/opendingux-beta-firmware-for-rg350-and-rg280-devices/#Install).

Midi is currently disabled, though the RG280v probably can't support USB OTG anyway.