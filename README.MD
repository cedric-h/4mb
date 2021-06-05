# Intro
This is a voxel game. Currently, in the most size-optimized build without controller support and error messages, it occupies 10kb.

You can walk around, placing and destroying blocks.

# Nuts 'n Bolts
### build.bat
This is the entry-point of the application; run it to get something you can play.

It converts the games source into an executable, dumping the resulting files into a "build" folder in the current directory. It builds the shaders, compiles a trimmed down version of microsoft's C runtime, and runs a compression pass on the resulting executable using [upx](upx.github.io).

There are two sets of compiler flags in use, you can select which set you want to use by configuring this variable:
```bat
set smol=no
```

If `smol` is set to `yes`, much more aggressive compilation flags are used, and the script looks for a copy of `upx` in the parent directory (so if you have the `build.bat` in a folder in your home directory, `C:/Users/cedhu/4mb/build.bat`, it will look for the executable in your home directory, `C:/Users/cedhu/upx`.)

If you don't have the UPX executable, just use `smol=no`, and the compression pass will be skipped. This is often desirable, because it's slow as hell.

A potential point of failure here is if the script cannot find `vcvars64.bat`, more popularly invoked as `vcvarsall.bat` (which may in turn call the 64 bit version). This bat script is needed to put essential Microsoft dev tools (like the shader compiler, C/C++ compiler, etc.) into scope so that our build script can use them. Unfortunately, `vcvars64.bat` is often in different locations depending on the version of Windows you are using. If the script is unable to find it on your machine, poke around the internet and your file system and try to find the exact path. We'll update the `.bat` script so that it uses it as another place to look.


### main.c
The second "entry point" of the application, this file is used to house all of the global state -- in a `static` struct named `state` -- as well as manage the window creation and handle input using the typical win32 "window procedure" (aka `winproc`).

I also use this file as temporary housing for "protofiles," things I'm working on but can't quite justify bringing out into their own file just yet. At the time of writing, the player and camera abstractions are two "protofiles" hiding in the top of `main.c`.

Another landmark in main.c is `init_world`, which should create the tree and dirt block the player starts out with, and do other gameplay-oriented initialization.


### render.h
This file reads from the `state` variable defined in `main.c`, and puts a representation of that state on the screen, so the player can see what's going on.

It exposes a few key functions, `render_create`, `render_frame`, `render_resize`, and `render_destroy`, that behave about as you would expect.

Voxel geometry is managed as dynamic vertex and index buffers. A function internal to `render.h`, `generate_geometry`, is responsible for filling it with new data each frame.


### box.h
This file is filled with abstractions that make dealing with voxels easier.

It has data structures which enumerate the six faces of a cube, the kinds of boxes in the game (aka blocks, but boxes is shorter), what data is stored in an individual Box, etc.

It has functions for casting rays against boxes (and finding out which face that ray hit), adding and removing boxes from our box storage data structure, etc.

In the game's coordinate system, the actual position of a voxel defines its minimum point on each axis, meaning that the voxel at (-1, -1, -1) will occupy the space until (0, 0, 0), which is the maximum point of that voxel. If you put a voxel at (0, 0, 0), it will occupy space until (1, 1, 1), and so on.

The game makes a marked departure from Minecraft in the way that boxes are stored. Minecraft uses dense chunks of boxes. An individual box does not know its neighbors or its position, but it can deduce them by looking at its index within its chunk and its chunks index in the world. Empty blocks are simply assigned a VOID type. This works well in the context of Minecraft because there are relatively few empty blocks, so relatively little memory is wasted storing empty blocks.

However, I would like to have a game comprised of floating islands made of voxels. These islands are suspended in space, each one has its own grid of voxels, and can rotate independently of the grid of any other island. Each island *could* have its own set of chunks in the way of minecraft, but the islands are shallow compared to Minecraft's deep worlds. Plenty of space would be wasted on empty voxels if we imitated Minecraft's deep chunk storage.

So instead, each island has its own arena of voxels. Inside of this arena, blocks maintain a doubly linked list; each block is aware of all of its neighbors, it knows their index into the arena and can trivially retrieve information about them. When a box is added, each of its neighbors is found and supplied with a pointer to this new block. When a box is removed, each of its neighbors is retrieved from the removed box's storage and the indexes pointing to it are set to `BoxId_NULL`.

This means that -- at least in the most naive implementation -- each voxel must be aware of not only all of its neighbors, but also its own position. Instead of storing a simple `uint8_t` per block indicating the kind of block it is, a voxel must store that and six `uint16_t`s indicating its neighbors, and three `int16_t`s indicating its position. Instead of 8 bytes per block, 152 are stored.

This analysis begs the question: is it worth it? Assume you have a 16x16x16 chunk. On this chunk is an island suspended in space. Only two vertical layers of this chunk are occupied, so 16x16x2 voxels are used as is typical in a sky-based minecraft, and 16x16x14 are wasted. This is a waste of 16x16x14x8 bytes, so 28672 bytes wasted in total. Let's compare this to the same thing using a linked list structure: you store only those 16x16x2 blocks, but instead of costing 8 bytes each, they cost 152. The total cost is 77824 bytes, compared to 32768 bytes done the dense chunks way.

So my "optimization" makes islands take up 2.3x times more memory, why bother? Because the blocks don't need to be doubly linked, and they don't necessarily neeed to store their position. The code to manage that becomes more obtuse than I'm willing to reckon with for a game jam, but you could potentially bring the cost per block to 56 bytes just by only storing three neighbors and the kind of block, bringing the total cost to 28672 blocks, 87% of the size of the dense chunks way.

87% is an improvement, but it assumes mostly hollow sky islands, and storing the indexes of your neighbors in a `uint16_t` means an individual island can only have 65,536 blocks (2^16). Why bother, why not just do things the dense chunks minecraft way? Because this makes it possible to do things you can't do in minecraft: have several voxel islands that don't occupy the same grid, and spin independently. Or maybe it is possible to do that with proper chunks; I'll have to investigate more, but another thing to consider is that even with chunks, we may want to keep this linked list code for maintaining the relationship between individual chunks, which is more complicated than in minecraft because we don't want our chunks to be 2D and very deep, we would prefer proper 3D chunks...

### math.h
Contains two and three dimensional vector, and four dimensional matrix abstractions.
C doesn't have operator overloading, so you're trapped writing code like: `add2(mul2_f(vector_1, 0.1f), vec2(0.3f, -0.5f))`. It's a bit obtuse, and sometimes writing out the math for each dimension longhand is more readable, but more often than not it's alright.

The four dimensional matrices assume 0..1 clip space like DirectX and Vulkan, but unlike OpenGL, so anyone doing an OpenGL port may want to keep that in mind.

### err.h
Some basic win32 error handling constructs. None of them do anything if `USE_DEBUG_MODE` (top of `main.c`) is set to `0`. `log_err` takes an arbitrary string. They dump to `OutputDebugString`, not `stdout`, so you need a debugger to read them.

### controller.h
Contains all of the controller-specific handling code. DirectInput is used. This works all major controllers, XBox, PS4, PS5, etc. but you miss out on stuff like being able to rumble the controller, and you can't tell the difference between the left and right triggers being held and neither being held with an XBox controller. The code that maps the controller actions to the gameplay is in `main.c`.

None of this code is compiled into the executable if `CONTROLLER_SUPPORT` (top of `main.c`) is `0`.

### mem.h
The code doesn't rely on the C runtime, instead using a trimmed down version to keep the executable size low -- typically Windows adds about 90kb of CRT initialization I just don't want -- so this file implements a couple of functions that are typically in the CRT by hand.

### ced_crt.def
This file defines what functions will go into our bootleg version of the CRT, mostly math stuff like `sinf`, `cosf`, etc.

### shader.hlsl
All of the shader source, vertex and fragment.
Gets compiled into "build/d3d11_vshader.h" (vetex) and "build/d3d11_pshader.h" (fragment AKA pixel) headers that contain the compiled shader source.

### std.rdbg
RemedyBG debugger file.
