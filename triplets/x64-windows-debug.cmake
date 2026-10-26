# Debug-only triplet：仅安装 debug 变体，省约一半磁盘。
# 注意：llvm port 不支持此 triplet（portfile 无条件删除 debug/include 与 debug/share，
# 且 vcpkg_cmake_config_fixup 会删掉 debug 侧唯一的 LLVMConfig.cmake），
# 因此 CMakePresets 使用标准 x64-windows triplet；本文件保留给其他不受影响的 port。
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

set(VCPKG_BUILD_TYPE debug)
