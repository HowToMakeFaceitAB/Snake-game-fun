instructions: you need sfml 3.1.0.
download it from their website: https://www.sfml-dev.org/download/
Unzip and extract somewhere. (dont forget to do this)
Then in the project properties (the one with the name of the solution
set configuration to all configurations. and platform to all platforms.
In C/C++. go into general, in additional include directories. and additional #using directories. include the path to the sfml 3.1.0 folder.
Ex: C:\Users\USERNAME\\Desktop\C++\hello world\SFML-3.1.0)
In Linker->General->Additional library directories. edit it and copy paste the path to the lib directory i.e (C:\Users\username\Desktop\C++\hello world\SFML-3.1.0\lib)
Do not copy paste the above path exactly since its gonna be diff on ur pc. Find where you place sfml.
In Linker -> Input > Additional dependecies. Include this:
sfml-graphics.lib;
sfml-window.lib;
sfml-system.lib;
Copy paste them preferably.
Finally, set to release and x 64 if not already and build. Have fun :)
PLZ NOTE: The arial.ttf file may be necessary on linux. idk. Built using MSVC v145 and windows 10 sdk. using c++ 20 standard.
