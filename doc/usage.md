> This document provides an overview of the freesia data model and basic user operations.

The freesia data model consists of three levels: the whole dataset, image groups, and individual images. All images within the same group undergo the same transformation process. An image group can contain one or more images, depending on the imaging strategy employed. For traditional thin slice 2D imaging, there is typically only one image per group. For 3D imaging, there are multiple images per group. 

## Data import

To import downsampled coronal section images and a detected cell list, follow these steps:

1. Click on the File menu.
2. Select Import images to import the coronal section images.
3. Select Import spots to import the detected cells list.

When importing images, you need to specify the group size and image voxel size. It is important to note that all images will be loaded and transferred to the graphics card memory. Therefore, it is recommended to use a voxel size that is not too large. A voxel size of 20 μm is a good option, considering that the voxel size of the CCF v3 is 25 μm.

Please ensure that the coordinates in the spots file are specified in micrometers.

Currently, freesia supports the import of 16-bit and 8-bit grayscale images.

## Basic operation

freesia provides four viewers, each with its own set of operations:

### General Viewer Operations (Applicable to all viewers)
- **Double-click the left mouse button.**: Maximize or restore viewer.
- **Hold down the Control key and left-click the mouse**: Identify the brain region corresponding to the clicked location.
- **Press the Shift key and drag the left mouse button**: Drag the image within the viewer.
- **Press the R key or double-click the middle mouse button**: Reset the viewer.

### 3D Viewer Operations (Top-right viewer)
- **Press and move the left mouse button.**: Rotate the data.
- **Press and move the right mouse button.**: Zoom in or out.

### Three View Operations (Three-view display)
- **Press the left/right arrow key**: Step backward/forward through the views.

### Coronal Section Viewer Operations (Top-left viewer)
- **Press the Page Up/Page Down key**: Switch to the previous/next image group.

Please note that each viewer has specific operations that provide different functionalities to enhance the viewing experience in freesia.

## Rigid transform

The CCF undergoes a 3D rigid transform, while the 2D transform is exclusively applied to all images within the corresponding group.

## Warp transform

In freesia, the operations related to the warp transform are specifically performed in the coronal section viewer (left-top), where you can double-click to hide other viewers.

For the coronal section viewer (left-top), the following operations are available:

- **Hold down the Z key and left-click the mouse**: Add marker points for the warp transform, using pairs of two points. Press the space key to add the point pair to the system, or press esc to cancel editing this pair.
- **Hold down the X key and move the mouse**: Activate the hover effect on the nearest existing point pairs.
- **Hold down the X key and left-click the mouse**: Select a pair that is in the hover state.
- **Press the delete key**: Remove the selected point pair.

Each warp marker consists of two points: one on the image and one on the CCF. The first point will move along with the image once it is added to the system.

To build deformation fields, you can click on the *Edit* menu and select either *Build warp field* for the current group or *Build all warp fields* for all groups. Additionally, you can toggle the warp preview by clicking on the *View* menu and selecting *Toggle warp preview*. Please note that markers can only be edited in the non-preview mode.

These operations allow for precise editing of the warp markers and the generation of deformation fields for the desired groups in freesia.

## Export cell-counting results

To export cell counting results in freesia, you can follow these steps:

1. Click on the *File* menu.
2. Select *Export cell counting results* option.
3. This action will generate cell counting results in the form of a .json file for each image.

Additionally, if you want to merge the cell counting results, you can perform the following steps:

1. Click on the *File* menu.
2. Select *Merge cell counting results* option.
3. This will merge the individual .json files into a single file named *cell-counting.csv*.
4. The *cell-counting.csv* file will be generated in the same directory as the input files.
5. The file will contain information such as the number of cells and cell density ($number/μm^3$).

These export and merge operations allow you to obtain comprehensive cell counting results for your images in freesia.
