# Polymorphism

Sometimes interfaces (of modules or other components) have multiple possible
implementations.
- display: can e.g. either use wayland or x11 backend
- audio: can e.g. use alsa or pulseaudio
- music: can use playerctl or player apis such as mpd directly

This can either be solved using *static polymorphism*: determining the
implementation at compile time but allowing no switching at runtime.
This has no runtime overhead and makes the implementation fairly simple
but can be a pain for packaging (because there have to be different
packages for different configurations when multiple are needed).

The other solution is *dynamic polymorphism*: the implementation
will be selected at runtime, e.g. depending on what backend is available.
This has a small runtime overhead, requires to use and implement vtables
but allows to use the application - compiled/packaged once - in different
environments.

Most modules currently use static polymorphism for simplicity,
while the display component uses dynamic polymorphism to allow the application
to run on x11 as well as wayland without recompilation.
