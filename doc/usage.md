> This document provides a description of freesia data model and basic user operation.

There are 3 levels in freesia data model, the whole data set, image groups and images. All images in the same group undergo the same transformation. A image group can contain one or more images, which depends on the imaging strategy. For traditional thin slice 2D imaging, the number is one. For thick slice 3D imaging like [VISoR](https://doi.org/10.1093/nsr/nwz053), the number is 15 in our case (300 μm per slice). For whole brain 3D imaging like [fMost](https://www.nature.com/articles/ncomms12142) and [MouseLight](https://www.janelia.org/project-team/mouselight), it can be any number you want which depends on how precise you expect your results.

## Data import

Click menu *File ->* *Import images* and *Import spots* to import downsample coronal section images and detected cells list. The group size need to be set when you importing images, as well as image voxel size. All images will be loaded and transferred to graphics card memory, so the voxel size should not be large and 20 μm is a good option since the voxel size of the CCF v3 is 25 μm. Coordinates in spots file must be in micrometers.

Currently, only 16-bit and 8-bit grayscale image can be imported into freesia.

## Basic operation

There are 4 viewers in freesia, each with the following operations:
* double left mouse click: maximize or restore viewer
* Control(holding) + left mouse click: see which brain region here is
* Shift(holding) + left mouse drag or middle mouse drag: drag image
* Press R or double middle mouse click: reset viewer

For 3D viewer (right-top viewer):
* left mouse drag: rotate data
* right mouse drag: zoom in or out

For three view:
* left/right arrow key: step backward/forward

For coronal section viewer (left-top):
* pageup/pagedown: switch to previous/next group

## Rigid transform

3d rigid transform will be applied to CCF while 2d transform only apply to all images in corresponding group. 

## Warp transform

All operations of warp transform can only be done in coronal section viewer (left-top) and you can double click on it to hide other viewers. 

For coronal section viewer (left-top):
* Press Z(holding) + left mouse click: Add marker point for warp transform with 2 points a pair. Press space key to add point pair to system or press esc to cancel editing this pair. 
* Press X(holding) + left mouse move: Hover effect on nearest existing pairs
* Press X(holding) + left mouse click: Select a pair which is in hover state
* Press delete: Remove selected pair

Each warp marker consists of 2 points, from point on image to point on CCF. The first point will move along with image after being added to system.

Click menu *Edit* -> *Build warp field* or *Build all warp fields* to build deformation fields for current group or all groups. Click menu *View* -> *Toggle warp preview* to switch if show images after deformation. Markers can only be edited in non-preview mode.

## Export cell-counting results

Click menu *File* -> *Export cell counting results* to create cell counting results(.json) for every images. And the click menu *File* -> *Merge cell counting results* to merge cell counting results(.json) and a file *cell-counting.csv* will be generated in the same direcotry of input. Cells number and density(number/μm3).
