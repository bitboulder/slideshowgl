
#if   defined IF_DIRED
const char *dlphelp_dired =
#elif defined IF_PRGED
const char *dlphelp_prged =
#elif defined IF_MAPED
const char *dlphelp_maped =
#elif defined IF_MAP
const char *dlphelp_map =
#elif defined IF_WRM
const char *dlphelp_wrm =
#else
const char *dlphelp_def =
#endif

__("Mouse interface")"\0"               "\0"

#ifdef IF_PRGED
__(" Left drag")"\0"                 __("Move image")"\0"
__(" Left click")"\0"                __("Forward")"\0"
__(" Middle click")"\0"              __("Toggle mark")"\0"
__(" Right click")"\0"               __("Backward")"\0"
__(" Scroll")"\0"                    __("Resize image")"\0"
#elif defined IF_MAP || defined IF_MAPED
__(" Left drag")"\0"                 __("Move map")"\0"
__(" Left click")"\0"                __("Center to position")"\0"
__(" Double click on marker")"\0"    __("Open marker images")"\0"
__(" Hold on marker")"\0"            __("Show folders in marker")"\0"
__(" Scroll")"\0"                    __("Zoom in/out")"\0"
#ifdef IF_MAPED
__(" Right drag")"\0"                __("Move marker")"\0"
__(" Right click")"\0"               __("Set new marker")"\0"
#else
__(" Middle click")"\0"              __("Paste position from clipboard")"\0"
__(" Right click")"\0"               __("Copy position to clipboard")"\0"
#endif
#else
__(" Left drag")"\0"                 __("Move")"\0"
__(" Left click")"\0"                __("Goto image / Forward")"\0"
__(" Double click on directory")"\0" __("Enter directory")"\0"
__(" Double click on movie")"\0"     __("Play movie")"\0"
__(" Double click on space")"\0"     __("Leave directory")"\0"
#if defined IF_WRM || defined IF_DIRED
__(" Middle click")"\0"              __("Play/Stop / Toggle mark")"\0"
#else
__(" Middle click")"\0"              __("Play/Stop")"\0"
#endif
__(" Right click")"\0"               __("Backward")"\0"
__(" Scroll")"\0"                    __("Zoom in/out")"\0"
#endif


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
#if defined IF_MAP || defined IF_MAPED
__(" Right/Left")"\0"          __("Move map")"\0"
__(" Up/Down")"\0"             __("Move map")"\0"
__(" Pageup/Pagedown")"\0"     __("Zoom in/out")"\0"
__(" Home/End")"\0"            __("Goto begin/end")"\0"
__(" Back")"\0"                __("Leave map")"\0"
__(" /")"\0"                   __("Search for marker")"\0"
__(" p")"\0"                   __("Paste position from clipboard")"\0"
__(" P")"\0"                   __("Copy position to clipboard")"\0"
__(" m")"\0"                   __("Switch map type")"\0"
__(" s")"\0"                   __("Toggle direction star display")"\0"
#else
#ifdef IF_PRGED
__(" Right/Left")"\0"          __("Forward/Backward")"\0"
__(" Up/Down")"\0"             __("Fast forward/backward")"\0"
__(" [0-9]+Enter")"\0"         __("Goto frame with number")"\0"
#else
__(" Space")"\0"               __("Stop/Play / Play Movie / Enter directory")"\0"
__(" Back")"\0"                __("Leave directory / Toggle panorama mode (spherical,plain,fisheye)")"\0"
__(" Right/Left")"\0"          __("Forward/Backward (Zoom: shift right/left)")"\0"
__(" Up/Down")"\0"             __("Fast forward/backward (Zoom: shift up/down)")"\0"
__(" Pageup/Pagedown")"\0"     __("Zoom in/out")"\0"
#endif
__(" [0-9]+Enter")"\0"         __("Goto image with number")"\0"
__(" /")"\0"                   __("Search for image or directory")"\0"
__(" n")"\0"                   __("Continue search")"\0"
#endif

#if !defined IF_MAP && !defined IF_MAPED
__(" Directory options")"\0"   "\0"
#ifdef IF_WRM
__(" S")"\0"                   __("Toggle sorting of current image list (only if no image is active)")"\0"
#elif !defined IF_PRGED
__(" S")"\0"                   __("Toggle sorting of current image list")"\0"
#endif
__(" l")"\0"                   __("Toggle loop of image lists")"\0"
#ifdef IF_PRGED
__(" e")"\0"                   __("Leave program editor")"\0"
#elif defined IF_DIRED
__(" e")"\0"                   __("Leave directory editor")"\0"
#else
__(" e")"\0"                   __("Open directory or program editor (only in real directories)")"\0"
#ifdef IF_WRM
__(" m")"\0"                   __("Open map (only if no image is active)")"\0"
#else
__(" m")"\0"                   __("Open map")"\0"
#endif
#endif
#endif

__(" Image modification")"\0"  "\0"
#if defined IF_WRM || defined IF_MAPED
__(" w")"\0"                   __("Disable writing mode")"\0"
#elif !defined IF_PRGED && !defined IF_DIRED
__(" w")"\0"                   __("Enable writing mode")"\0"
#endif
#ifdef IF_MAPED
__(" e")"\0"                   __("Switch mode for new markers (add/replace)")"\0"
#endif
#if defined IF_MAP || defined IF_MAPED
__(" d")"\0"                   __("Switch display mode for markers")"\0"
__(" H")"\0"                   __("Toggle elevation display")"\0"
#else
#if defined IF_WRM || defined IF_DIRED || defined IF_PRGED
__(" r/R")"\0"                 __("Rotate image")"\0"
#else
__(" r/R")"\0"                 __("Rotate image (only in writing mode permanent)")"\0"
#endif
#ifndef IF_PRGED
__(" g/b/c")"\0"               __("+/- mode: gamma/brightness/contrase (only with opengl shader support)")"\0"
__(" +/-")"\0"                 __("Increase/decrease selected")"\0"
#endif
#if defined IF_WRM || defined IF_DIRED || defined IF_PRGED
__(" Del")"\0"                 __("Move image to del/ and remove from dpl-list")"\0"
__(" o")"\0"                   __("Move image to ori/ and remove from dpl-list")"\0"
#endif
#ifdef IF_WRM
__(" m")"\0"                   __("Toggle mark")"\0"
__(" M")"\0"                   __("Switch mark file")"\0"
#elif !defined IF_DIRED && !defined IF_PRGED
__(" m")"\0"                   __("Show map")"\0"
#endif
#if defined IF_WRM || defined IF_DIRED
__(" s")"\0"                   __("Enter and toggle image catalog")"\0"
__(" S")"\0"                   __("Copy catalogs from last marked image (only if an image is active)")"\0"
#endif
__(" G")"\0"                   __("Edit current image with gimp")"\0"
__(" U")"\0"                   __("Edit raw image with ufraw")"\0"
#ifndef IF_PRGED
__(" C")"\0"                   __("Open current image/directory for file-convert-utility")"\0"
#endif

#ifndef IF_PRGED
__(" Image information")"\0"   "\0"
__(" i/I")"\0"                 __("Show selected/full image info")"\0"
__(" H")"\0"                   __("Show histograms")"\0"
__(" k")"\0"                   __("Show image catalog")"\0"
#endif
#endif

__(" Program interface")"\0"   "\0"
__(" h")"\0"                   __("Show help")"\0"
__(" q/Esc")"\0"               __("Quit")"\0"
__(" f")"\0"                   __("Toggle fullscreen")"\0"
#ifndef IF_PRGED
__(" [0-9]+d")"\0"             __("Displayduration [s/ms]")"\0"
#endif
#if !defined IF_PRGED && !defined IF_DIRED && !defined IF_MAP && !defined IF_MAPED
__(" p")"\0"                   __("Toggle panorama fisheye mode (isogonic,equidistant,equal-area)")"\0"
#endif

"\0"
;

