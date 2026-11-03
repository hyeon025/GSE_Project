# Vendored GLEW 2.2.0

Official source archive:
https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.zip

The unmodified release, generated headers, C source and licenses are included.
SimpleGame compiles `glew-2.2.0/src/glew.c` with `GLEW_STATIC` using its own CRT
settings. All consumers include the matching header through Renderer.h or its
explicit versioned path. No machine-installed or old prebuilt GLEW is linked.

The older files in the parent Dependencies directory remain for history, but
the current application and performance tools do not use them.
