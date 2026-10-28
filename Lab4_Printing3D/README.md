> \[!IMPORTANT]
> The majority of the code for this project was generated using ChatGPT, with clear implementation instructions provided by myself.

[Lab 4 Demo](https://youtu.be/W4u0QnQKcpg)

# Printable Visual Cryptography

This app automatically designs a printable grid using the concepts of [Visual Cryptography](https://en.wikipedia.org/wiki/Visual_cryptography) described by [Moni Naor and Adi Shamir](https://www.wisdom.weizmann.ac.il/~naor/PAPERS/vis.pdf).  The GUI allows the user to input text, control the grid resolution, and adjust the desired print dimensions.  Additionally, the user may use the same key to generate multiple cypher designs such that the a single printed key plate can be used on more than one cypher plates.

## Setup Instructions

Install the Python packages listed in requirements.txt, then run the command line: "\~\\Lab4\_Printing3D> python .\\gui.py".

## Usage

Input text, and adjust resolution for legibility.  This is constrained by the physical print dimensions and 3D printer resolution.
Press "Preview" to visualize the cipher and key plate, as well as their resulting overlay.  Once satisfied with the result, press "Generate STL" to output the '.stl' mesh files for the cipher and key.  Additional outputs include '.png' images for the three preview images, and a '.json' file with the parameters used to generate the mesh files.
Print using your favorite slicer!

> \[!NOTE]
> The hardcoded defaults assume a 0.4 mm nozzle and 0.2 mm layer height.

## Files

|File|Responsibility|
|-|-|
|`grid_generation.py`|Constructs the secret image, then generates the cipher and key grids.|
|`stl_mesh.py`|Generates contiguous STL meshes from two-dimensional Boolean grids.|
|`config.py`|Hardcoded grid, mesh, and printer defaults.|
|`gui.py`|Tkinter interface that coordinates the other three modules.|
