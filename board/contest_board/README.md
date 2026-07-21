# VelaGuard 黄山派构建配置

本目录映射到 openvela：

```text
vendor/openvela/boards/contest2026_148_board
```

VelaGuard 使用官方已经支持的黄山派 SF32LB52 BSP：

```text
vendor/sifli/boards/sf32lb52/lckfb_huangshan_pi
```

因此本目录不是复制一份 BSP，也不替换官方的启动、pinmux、屏幕或触摸驱动。
`configs/nsh/defconfig` 是本队维护的构建配置：它将
`CONFIG_ARCH_BOARD_CUSTOM_DIR` 指向官方 BSP，并在同一份配置中启用
`CONFIG_LVX_USE_DEMO_CONTEST2026_148_VELAGUARD`。

从 openvela 根目录构建：

```sh
export PYTHONPATH="$PWD/prebuilts/tools/python/dist-packages/pyelftools:$PWD/prebuilts/tools/python/dist-packages/cxxfilt:$PYTHONPATH"
export PATH="$PWD/prebuilts/tools/linux/x86_64:$PATH"

cmake -B cmake_out/velaguard_huangshan -S "$PWD/nuttx" -GNinja \
  -DBOARD_CONFIG=../vendor/openvela/boards/contest2026_148_board/configs/nsh \
  -DEXTRA_FLAGS="-Wno-cpp -Wno-deprecated-declarations"

cmake --build cmake_out/velaguard_huangshan
```

产物为 `cmake_out/velaguard_huangshan/nuttx.bin`。

`src/board_boot.c` 仍是比赛模板留下的占位示例；当前构建不会使用它。
要修改官方黄山派 BSP 本身，需要单独向 `vendor_sifli` 提交补丁或 PR，不能把
这类改动藏在 VelaGuard 应用提交里。
