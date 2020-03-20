# waitfree-vector

Development was done using Ubuntu 16.04 and 18.04 on 64 bit Intel machines.

## Dependencies:

### MRLock

MRLock is included as a submodule. It requires libboost-dev, libboost-random-dev, and libtbb-dev (use `make get_deps`).

After installing the dependencies, to get the mrlock library code, run `git submodule update --init --recursive` in the root of the repo.

Follow the instructions in the [README](/mrlock/README) to build that code. Then you can build the targets of our project.

## Programming Assignment 1 Writeup
