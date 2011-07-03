
__("Mouse interface")"\0"               "\0"

#ifdef IF_PRGED
__(" Left drag")"\0"            __("Move image")"\0"
__(" Left click")"\0"           __("Forward")"\0"
__(" Middle click")"\0"         __("Toggle mark (only in writing mode)")"\0"
__(" Right click")"\0"          __("Backward")"\0"
__(" Scroll")"\0"               __("Resize image")"\0"
#else
__(" Left drag")"\0"                 __("Move")"\0"
__(" Left click")"\0"                __("Goto image / Forward")"\0"
__(" Double click on directory")"\0" __("Enter directory")"\0"
__(" Double click on space")"\0"     __("Leave directory")"\0"
__(" Middle click")"\0"              __("Play/Stop / Toggle mark (only in writing mode)")"\0"
__(" Right click")"\0"               __("Backward")"\0"
__(" Scroll")"\0"                    __("Zoom in/out")"\0"
#endif


// TODO: remove "only in writing mode" for DIRED + PRGED
" ""\0"                        "\0"
__("Keyboard interface")"\0"   "\0"

#ifdef IF_DIRED
__(" Directory Editor")"\0"    "\0"
__(" m/M")"\0"                 __("Move images from/up to current one to other or new directory")"\0"
__(" j")"\0"                   __("Move all images to other directory or rename directory")"\0"
#elif defined IF_PRGED
__(" Program Editor")"\0"    "\0"
__(" Pageup/Pagedown")"\0"     __("Resize image")"\0"
__(" +/-")"\0"                 __("Add/remove image")"\0"
__(" t")"\0"                   __("Add text")"\0"
__(" i")"\0"                   __("Insert frame")"\0"
__(" x")"\0"                   __("Delete frame")"\0"
__(" m/M")"\0"                 __("Swap frame with next/previous one")"\0"
__(" d")"\0"                   __("Duplicate frame")"\0"
__(" [0-9]+m")"\0"             __("Move frame to position")"\0"
__(" [0-9]+d")"\0"             __("Copy frame to position")"\0"
__(" l/L")"\0"                 __("Move image into foreground/background")"\0"
__(" c")"\0"                   __("Change color of text")"\0"
__(" C")"\0"                   __("Copy color of text to all text on current frame")"\0"
__(" E")"\0"                   __("Refresh current frame")"\0"
#endif
#if defined IF_PRGED || defined IF_DIRED
__(" w")"\0"                   __("Switch window")"\0"
#endif

__(" Navigation")"\0"          "\0"
#ifdef IF_PRGED
__(" Right/Left")"\0"          __("Forward/Backward")"\0"
__(" Up/Down")"\0"             __("Fast forward/backward")"\0"
__(" [0-9]+Enter")"\0"         __("Goto frame with number")"\0"
#else
__(" Space")"\0"               __("Stop/Play / Enter directory")"\0"
__(" Back")"\0"                __("Leave directory / Toggle panorama mode (spherical,plain,fisheye)")"\0"
__(" Right/Left")"\0"          __("Forward/Backward (Zoom: shift right/left)")"\0"
__(" Up/Down")"\0"             __("Fast forward/backward (Zoom: shift up/down)")"\0"
__(" Pageup/Pagedown")"\0"     __("Zoom in/out")"\0"
#endif
__(" [0-9]+Enter")"\0"         __("Goto image with number")"\0"

__(" Directory options")"\0"   "\0"
#ifndef IF_PRGED
__(" S")"\0"                   __("Toggle sorting of current image list")"\0"
#endif
__(" l")"\0"                   __("Toggle loop of image lists")"\0"
#ifdef IF_PRGED
__(" e")"\0"                   __("Leave program editor")"\0"
#elif defined IF_DIRED
__(" e")"\0"                   __("Leave directory editor")"\0"
#else
__(" e")"\0"                   __("Open directory or program editor")"\0"
#endif

__(" Image modification")"\0"  "\0"
#if !defined IF_PRGED && !defined IF_DIRED
__(" w")"\0"                   __("Switch writing mode")"\0"
#endif
__(" r/R")"\0"                 __("Rotate image (only in writing mode permanent)")"\0"
#ifndef IF_PRGED
__(" g/b/c")"\0"               __("+/- mode: gamma/brightness/contrase (only with opengl shader support)")"\0"
__(" +/-")"\0"                 __("Increase/decrease selected")"\0"
#endif
__(" Del")"\0"                 __("Move image to del/ and remove from dpl-list (only in writing mode)")"\0"
__(" o")"\0"                   __("Move image to ori/ and remove from dpl-list (only in writing mode)")"\0"
#if !defined IF_PRGED && !defined IF_DIRED
__(" m")"\0"                   __("Toggle mark (only in writing mode)")"\0"
#endif
#ifndef IF_PRGED
__(" s")"\0"                   __("Enter and toggle image catalog (only in writing mode)")"\0"
#endif
__(" G")"\0"                   __("Edit current image with gimp")"\0"

#ifndef IF_PRGED
__(" Image information")"\0"   "\0"
__(" i/I")"\0"                 __("Show selected/full image info")"\0"
__(" k")"\0"                   __("Show image catalog")"\0"
#endif

__(" Program interface")"\0"   "\0"
__(" h")"\0"                   __("Show help")"\0"
__(" q/Esc")"\0"               __("Quit")"\0"
__(" f")"\0"                   __("Toggle fullscreen")"\0"
#ifndef IF_PRGED
__(" [0-9]+d")"\0"             __("Displayduration [s/ms]")"\0"
#endif
#if !defined IF_PRGED && !defined IF_DIRED
__(" p")"\0"                   __("Toggle panorama fisheye mode (isogonic,equidistant,equal-area)")"\0"
#endif

"\0"

