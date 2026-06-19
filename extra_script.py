#
# If bsec2 is included in the json dependency, NanoC6 will have trouble compiling,
# Add bsec2 dynamically
#
Import("env")
import os
import shutil
import subprocess

cpu = env.get("BOARD_MCU")

print("==== Check need bsec2? ====")
print("board mcu:", cpu)


if ("esp32c6" in cpu.lower()):
    print("Not need bsec2")
    
#    project_dir = env.subst("$PROJECT_DIR")
#    libdeps_dir = env.subst("$PROJECT_LIBDEPS_DIR")
#    current_env_name = env["PIOENV"]
#    print(f"Current environment: {current_env_name}")

#    bsec2_dir = os.path.join(libdeps_dir, current_env_name, "bsec2")    
#    print(project_dir)
#    print(libdeps_dir)
#    print(bsec2_dir)

#    if os.path.exists(bsec2_dir):
#        shutil.rmtree(bsec2_dir)
#        print("Deleted bsec2 directory to skip build.")

#        deps = env.GetProjectOption("lib_deps")
#        if deps:
#            deps = [d for d in deps if "bsec2" not in d]
#            env.Replace(LIB_DEPS=deps)

#        deps = env.GetProjectOption("lib_deps")
#        print(deps)
            
        
elif not ("esp32c6" in cpu.lower()):
    print("Need bsec2")
#    subprocess.run(["pio", "lib", "install", "boschsensortec/bsec2@1.8.2610"])

# if ("esp32c6" in cpu.lower()):
#     print("bsec2 does not support esp32c6")

#     libdeps_dir = env.subst("$PROJECT_LIBDEPS_DIR")
#     bsec2_path = os.path.join(libdeps_dir, "bsec2")
#     print("Exclude compile:", bsec2_path)

#     src_filter = "+<*> -<" + bsec2_path + ">"
#     env.Append(SRC_FILTER=src_filter)
#     print(f"bsec2 build excluded with filter: {src_filter}")

# Install a PlatformIO library
    project_dir = env.subst("$PROJECT_DIR")
    print(project_dir)
#    subprocess.run(['pio', 'pkg', 'install', '-l', 'boschsensortec/bsec2'], cwd=project_dir)


# Or, you can use the <<!nav>>platformio<<!/nav>> CLI directly:
#    subprocess.run(['platformio', 'cli', 'lib', 'install', 'boschsensortec/bsec2'], check=True)

# Or modify the build environment
#env.Append(CPPPATH=['/path/to/your/include/dir'])  # Example: Add a custom include path

#    print("Custom script executed successfully!")


####	"boschsensortec/bsec2": "@1.8.2610"

print("====")
