# Sputterer
## Installation
1. Install `cmake`
2. Set up the build dir
```
cmake -S . -B build
```
3. Build the executable
```
cmake --build build
```
4. The `sputterer` executable will be in the build dir

### Great Lakes
You will need to load, at minimum, the gcc, cuda, and cmake modules before installing.

You will also want to pass `0` to `enable-display` below.

## Usage
`sputterer <input_file> <enable-display>`

See `input.toml` in the root dir for an example input file. 

If `<enable-display>` is set to 1, graphical mode is enabled and a GLFW window will launch showing the user's specified scene. This is useful for debugging. In graphical mode, use the mouse and WASD to move around the scene and spacebar to pause and unpause the simulation.

## Docs
To run the docs, install Doxygen [here](https://doxygen.nl/)
Then, run `doxygen dconfig` in the project root directory. This will read the files from 
src/ in dconfig and generate documentation. Basic doxygen comment is `//! comment`, which indicates
to doxygen that the comment is tied to the line of code below.

## TODO
- Test installation on cluster
- Documentation
- Test suite

