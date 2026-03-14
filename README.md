# Stock Manager

## build with make:

```bash
# build
make

# run with:
./stock_manager

# get help:
make help

# make options:
make clean          # remove build files
make release        # build with optimizations and no debug info
make test           # run tests
make avx2           # build with avx2 optimizations
make avx2-release   # build with avx2 optimizations and no debug info

```

avx2 requires a cpu and compiler support. If you get an error about avx2 not being supported, try building without it.
