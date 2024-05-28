
Import("env")
FILE_NAME = "src/main.cpp" 


def increment_version_num(old):
    A, B, C = map(int, old.split("."))
    version = (A * 100 + B * 10 + C) + 1
    if (version > 999):
        version = 0

    return f"{version // 100}.{(version // 10) % 10}.{version % 10}"


def build_or_upload(build): 
    with open(FILE_NAME, "r") as f: 
        lines = f.readlines()

    # Find matching string and replace
    for i in range(len(lines)):
        if ("#   - Build version: ") in lines[i]:
            old = lines[i].split(": ")[1][:5]

            # Build
            if build:
                new = increment_version_num(old)
                lines[i] = lines[i].replace(old, new)

            # Upload
            elif ("#   - Uploaded version: ") in lines[i-1]:   
                prev_line = lines[i-1].split(": ")[1][:5]
                lines[i-1] = lines[i-1].replace(prev_line, old)

            # Write
            with open(FILE_NAME, "w") as main_cpp: 
                main_cpp.writelines(lines)
            break


def post_build(source, target, env):
    build_or_upload(True)

def post_upload(source, target, env):
    build_or_upload(False)

env.AddPostAction("buildprog", post_build)
env.AddPostAction("upload", post_upload)
