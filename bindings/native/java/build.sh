#!/usr/bin/env bash
# Build the JNI native library and run the self-test.
set -euo pipefail
cd "$(dirname "$0")"
ROOT=$(cd ../../.. && pwd)

JAVA_HOME=${JAVA_HOME:-$(dirname "$(dirname "$(readlink -f "$(command -v javac)")")")}

# 1. Compile Java + generate the JNI header (Vibe.h) with `javac -h .`
javac -h . Vibe.java

# 2. Compile the JNI C into libvibejni.so, linking the static libvibe.
cc -shared -fPIC \
   -I"$JAVA_HOME/include" -I"$JAVA_HOME/include/linux" \
   -I"$ROOT" \
   vibe_jni.c "$ROOT/libvibe.a" \
   -o libvibejni.so

# 3. Run the self-test.
java -Dvibe.jni.path="$PWD/libvibejni.so" \
     -Dvibe.sample="$ROOT/bindings/sample.vibe" \
     Vibe
