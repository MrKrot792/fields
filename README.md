# Fields
Little 1-D wave propagation simulator. You will have to recompile the
app if you want to add/remove waves, unfortunatelly. Also you have to
restart it if you want to reset the waves.

# How to run
First you have to compile it. Just run the `./build.sh` script.
```sh
$ chmod +x build.sh
$ ./build.sh
```

Then you run it:
```sh
./main
```

# Controls
- Space: Unpause the simulation.
- h/j/k/l: Scope's scale, x & y-wise.
- Arrows: Move the scope around.
- X: Turn the "fast mode" (allows you to move the camera around
  faster) on or off. It's on by default.
- +/-/=: Change simulation speed.
- A: Enable wave lerping. Makes the waves smoother. It's on by
  default.
- N: Reset the camera.
- S: Step the simulation 0.5 seconds.
- Q: Quit the app.
