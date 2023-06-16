# freesia-atlas-cell-counting

Freesia is a software specifically developed for the manual registration of histological slice images to the Allen Mouse Common Coordinate Framework ([CCF v3](https://scalablebrainatlas.incf.org/mouse/ABA_v3)). It can take cell identification results obtained from software such as [Imaris](https://imaris.oxinst.com/) or [ilastik](https://www.ilastik.org/), and generate a cell counting list for brain regions (refer to [PI C et al. Mol Brain. 2022](https://doi.org/10.1186/s13041-022-00985-w)).

![freesia](doc/images/freesia.png)

## Basic usage

The image registration process in freesia consists of three parts: 3D rigid transform on CCF, 2D rigid transform on images, and 2D deformation on images. It is important to note that all images in the same dataset should have the same size, and the brain slices in the images should be approximately centered and rotation-free.

The control for 3D/2D rigid transform is located in the right-top/middle panel, which includes options for 3D/2D rotation, translation, and scaling. On the other hand, 2D deformation is performed using a key point-based method, which requires manually labeled point pairs.

For more detailed information about the usage, please refer to [this document](doc/usage.md).

## Demo data

Within the "demo-data" folder of the "all-in-one" software release bundle, you will find an image along with a corresponding detected cells list. You can import the image or load the provided JSON project to see how the software works. It is worth mentioning that image sequences are also supported, so you can try it out on your own dataset by following the same process.

## Building the code

Currently, the software is only supported on Windows. To build the code, you need to install some dependencies first. The following instructions are tested on Windows 10 with Visual Studio 2019.

First, install the dependencies in [vcpkg](https://vcpkg.io):
    
```bash
./vcpkg install opencv:x64-windows vtk[qt,opengl]:x64-windows
```

And build the [nn-c-windows](https://github.com/dinglufe/nn-c-windows)

The released software is built using Visual Studio Code with `.vscode/settings.json` like this:

```json
{
    "cmake.cmakePath": "/path/to/cmake-3.21.1-windows-x86_64/bin/cmake.exe",
    "cmake.configureSettings": {
        "CMAKE_TOOLCHAIN_FILE": "/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake",
        "NN_DIR": "/path/to/nn-c-windows",
        "VCPKG_TARGET_TRIPLET": "x64-windows",
        "CMAKE_BUILD_TYPE": "Relwithdebinfo"
    },
    "cmake.buildDirectory": "${workspaceFolder}/build/2.1.0",
    "cmake.generator": "Ninja"
}
```


## License

Freesia is available for free use and modification, but it is copyrighted by the author and may not be sold or included in commercial products.

Feedback and feature requests are welcome :rocket:.