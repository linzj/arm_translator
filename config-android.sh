# Use the build standalone toolchian to get the toolchain first.
ANDROID_TOOLCHAIN=`pwd`/../../toolchain/
ANDROID_BUILD_DIR=.
LLVM_CHECKOUT=../../
BUILD_TYPE=Release

# Always clobber android build tree.
# It has a hidden dependency on clang (through CXX) which is not known to
# the build system.
mkdir $ANDROID_BUILD_DIR
(cd $ANDROID_BUILD_DIR && \
 cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
 -DLLVM_ANDROID_TOOLCHAIN_DIR=$ANDROID_TOOLCHAIN \
 -DCMAKE_TOOLCHAIN_FILE=$LLVM_CHECKOUT/cmake/platforms/Android_x86.cmake \
-DLLVM_ENABLE_PIC=off -DLLVM_ENABLE_RTTI=off -DLLVM_ENABLE_TERMINFO=off -DLLVM_ENABLE_TIMESTAMPS=off -DLLVM_ENABLE_ZLIB=off -DLLVM_TARGETS_TO_BUILD="X86" \
 $LLVM_CHECKOUT)
# We should remove native to force
# cmake reconfigure it again.
rm -rf native
