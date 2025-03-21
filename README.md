									
# PatchMatch: Hole Filling

![frontispiece.png](frontispiece.png)
This application lets you mark an input image (left) with a hole (blue) and apply the PatchMatch algorithm to
fill that hole from other parts of the image (right). In effect, 

by Greg Philbrick (Copyright 2024)
													
## Description	

The [PatchMatch](https://gfx.cs.princeton.edu/pubs/Barnes_2009_PAR/index.php) algorithm, presented by Barnes et al.,
is about finding pixel-to-pixel correspondences between two images A and B. More specifically, it is about finding,
for each NxN patch of image A, the best corresponding NxN patch of image B. Finding this patch-to-patch correspondence
allows for some interesting image editing operations, in particular "hole filling," the focus of this repository.

In a hole-filling operation, a user marks some pixels in an input image and requests that these pixels be 
intelligently "deleted," e.g., that these pixels be filled with image content taken from unmarked pixels--that is,
from outside the hole. (Note that this is how Photoshop's "Content-Aware Delete" tool works.) The PatchMatch algorithm
can achieve this effect by treating the hole (the marked pixels) as one image and the rest of the input image as another
image.

I present here my two implementations of PatchMatch-based hole filling: one built in OpenCL to run on the GPU, the
other purely in C++. You can run both of these implementations from my application to compare their results. (The OpenCL 
implementation not only runs faster--no surprise there--but produces more seamless fills,suggesting an undiagnosed issue 
in the C++-only version. Note that this code comes from my research years as a master's student.)

If you are not interested in running the code and just want to view it for insights into the PatchMatch algorithm,
start out at PatchMatch/holefillpatchmatchopencl.h and at PatchMatch/holefillpatchmatch.h.

## How to Build / 3rd-Party Dependencies

To configure this project, CMake needs to know how to find these on your computer:

* [Boost](https://www.boost.org/)		
* [Qt 5.15+ or 6](https://doc.qt.io/qt-6/get-and-install-qt.html)
* [OpenMP](https://www.openmp.org/)
* [OpenCL]

Note that just having an OpenCL-capable video card is not enough; OpenCL itself must be installed. I most recently
accomplished this by installing the NVIDIA CUDA SDK, which is overkill but simple.

## How to Run

Once you have configured and built the project, run the HoleFillApplication target. The first time you run,
you will be asked to pick where OpenCL device you want to use (there may be more than one if you have multiple
GPUs). 

To mark a hole to fill, click and drag on the image. Once your hole is marked, you can either (1) complete
the hole-fill operation using the OpenCL implementation, (2) complete the hole-fill operation using the 
non-OpenCL implementation, or (3) begin a "manual" application of the non-OpenCL implementation. The third
option lets you move through the steps of the algorithm and observe the progress. Note that this involves
moving through a sequence of pyramid levels, starting out at a very low-resolution version of the problem
and finishing at a full-resolution version, i.e., pyramid level 0. At each pyramid level you can "refine"
the NNF as many times as you like. After each "Refine NNF" click, a new "blend" will be performed, allowing
you to see how the NNF has changed. 

## License
					
My code is subject to the Boost Software License v1.0. See LICENSE.txt.