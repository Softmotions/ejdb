#!/usr/bin/env bash

set -e
set -x

function register_clang_version {
    local version=$1
    local priority=$2

    update-alternatives \
        --install /usr/bin/llvm-config       llvm-config      /usr/bin/llvm-config-${version} ${priority} \
        --slave   /usr/bin/llvm-addr2line    llvm-addr2line   /usr/bin/llvm-addr2line-${version} \
        --slave   /usr/bin/llvm-ar           llvm-ar          /usr/bin/llvm-ar-${version} \
        --slave   /usr/bin/llvm-as           llvm-as          /usr/bin/llvm-as-${version} \
        --slave   /usr/bin/llvm-bcanalyzer   llvm-bcanalyzer  /usr/bin/llvm-bcanalyzer-${version} \
        --slave   /usr/bin/llvm-cov          llvm-cov         /usr/bin/llvm-cov-${version} \
        --slave   /usr/bin/llvm-diff         llvm-diff        /usr/bin/llvm-diff-${version} \
        --slave   /usr/bin/llvm-dis          llvm-dis         /usr/bin/llvm-dis-${version} \
        --slave   /usr/bin/llvm-dwarfdump    llvm-dwarfdump   /usr/bin/llvm-dwarfdump-${version} \
        --slave   /usr/bin/llvm-exegesis     llvm-exegesis    /usr/bin/llvm-exegesis-${version} \
        --slave   /usr/bin/llvm-extract      llvm-extract     /usr/bin/llvm-extract-${version} \
        --slave   /usr/bin/llvm-link         llvm-link        /usr/bin/llvm-link-${version} \
        --slave   /usr/bin/llvm-lipo         llvm-lipo        /usr/bin/llvm-lipo-${version} \
        --slave   /usr/bin/llvm-lto          llvm-lto         /usr/bin/llvm-lto-${version} \
        --slave   /usr/bin/llvm-lto2         llvm-lto2        /usr/bin/llvm-lto2-${version} \
        --slave   /usr/bin/llvm-mc           llvm-mc          /usr/bin/llvm-mc-${version} \
        --slave   /usr/bin/llvm-mcmarkup     llvm-mcmarkup    /usr/bin/llvm-mcmarkup-${version} \
        --slave   /usr/bin/llvm-nm           llvm-nm          /usr/bin/llvm-nm-${version} \
        --slave   /usr/bin/llvm-objcopy      llvm-objcopy     /usr/bin/llvm-objcopy-${version} \
        --slave   /usr/bin/llvm-objdump      llvm-objdump     /usr/bin/llvm-objdump-${version} \
        --slave   /usr/bin/llvm-opt-report   llvm-opt-report  /usr/bin/llvm-opt-report-${version} \
        --slave   /usr/bin/llvm-pdbutil      llvm-pdbutil     /usr/bin/llvm-pdbutil-${version} \
        --slave   /usr/bin/llvm-profdata     llvm-profdata    /usr/bin/llvm-profdata-${version} \
        --slave   /usr/bin/llvm-ranlib       llvm-ranlib      /usr/bin/llvm-ranlib-${version} \
        --slave   /usr/bin/llvm-rc           llvm-rc          /usr/bin/llvm-rc-${version} \
        --slave   /usr/bin/llvm-readelf      llvm-readelf     /usr/bin/llvm-readelf-${version} \
        --slave   /usr/bin/llvm-readobj      llvm-readobj     /usr/bin/llvm-readobj-${version} \
        --slave   /usr/bin/llvm-rtdyld       llvm-rtdyld      /usr/bin/llvm-rtdyld-${version} \
        --slave   /usr/bin/llvm-size         llvm-size        /usr/bin/llvm-size-${version} \
        --slave   /usr/bin/llvm-split        llvm-split       /usr/bin/llvm-split-${version} \
        --slave   /usr/bin/llvm-stress       llvm-stress      /usr/bin/llvm-stress-${version} \
        --slave   /usr/bin/llvm-strip        llvm-strip       /usr/bin/llvm-strip-${version} \
        --slave   /usr/bin/llvm-symbolizer   llvm-symbolizer  /usr/bin/llvm-symbolizer-${version} \
        --slave   /usr/bin/llvm-tblgen       llvm-tblgen      /usr/bin/llvm-tblgen-${version} \
        --slave   /usr/bin/llvm-undname      llvm-undname     /usr/bin/llvm-undname-${version}

    update-alternatives \
        --install /usr/bin/clang                 clang                 /usr/bin/clang-${version} ${priority} \
        --slave   /usr/bin/asan_symbolize        asan_symbolize        /usr/bin/asan_symbolize-${version} \
        --slave   /usr/bin/c-index-test          c-index-test          /usr/bin/c-index-test-${version} \
        --slave   /usr/bin/clang++               clang++               /usr/bin/clang++-${version}  \
        --slave   /usr/bin/clang-check           clang-check           /usr/bin/clang-check-${version} \
        --slave   /usr/bin/clang-cl              clang-cl              /usr/bin/clang-cl-${version} \
        --slave   /usr/bin/clang-cpp             clang-cpp             /usr/bin/clang-cpp-${version} \
        --slave   /usr/bin/clang-format          clang-format          /usr/bin/clang-format-${version} \
        --slave   /usr/bin/clang-format-diff     clang-format-diff     /usr/bin/clang-format-diff-${version} \
        --slave   /usr/bin/clang-import-test     clang-import-test     /usr/bin/clang-import-test-${version} \
        --slave   /usr/bin/clang-include-fixer   clang-include-fixer   /usr/bin/clang-include-fixer-${version} \
        --slave   /usr/bin/clang-offload-bundler clang-offload-bundler /usr/bin/clang-offload-bundler-${version} \
        --slave   /usr/bin/clang-query           clang-query           /usr/bin/clang-query-${version} \
        --slave   /usr/bin/clang-rename          clang-rename          /usr/bin/clang-rename-${version} \
        --slave   /usr/bin/clang-reorder-fields  clang-reorder-fields  /usr/bin/clang-reorder-fields-${version} \
        --slave   /usr/bin/clang-tidy            clang-tidy            /usr/bin/clang-tidy-${version} \
        --slave   /usr/bin/clangd                clangd                /usr/bin/clangd-${version}  \
        --slave   /usr/bin/lld                   lld                   /usr/bin/lld-${version}  \
        --slave   /usr/bin/lld-link              lld-link              /usr/bin/lld-link-${version} \
        --slave   /usr/bin/lldb                  lldb                  /usr/bin/lldb-${version} \
        --slave   /usr/bin/lldb-argdumper        lldb-argdumper        /usr/bin/lldb-argdumper-${version} \
        --slave   /usr/bin/lldb-instr            lldb-instr            /usr/bin/lldb-instr-${version} \
        --slave   /usr/bin/lldb-server           lldb-server           /usr/bin/lldb-server-${version} \
        --slave   /usr/bin/lldb-vscode           lldb-vscode           /usr/bin/lldb-vscode-${version} \
        --slave   /usr/bin/lli                   lli                   /usr/bin/lli-${version} \
        --slave   /usr/bin/lli-child-target      lli-child-target      /usr/bin/lli-child-target-${version}
}

register_clang_version $1 $2
