# PlatformIO-Extra-Skript: -m32 auch an den Linker geben (build_flags gelten nur fürs Kompilieren)
Import("env")
env.Append(LINKFLAGS=["-m32"])
