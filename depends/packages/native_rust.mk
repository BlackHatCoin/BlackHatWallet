package=native_rust
$(package)_version=1.69.0
$(package)_download_path=https://static.rust-lang.org/dist
$(package)_file_name_x86_64_linux=rust-$($(package)_version)-x86_64-unknown-linux-gnu.tar.xz
$(package)_sha256_hash_x86_64_linux=fe0d3eb8604a72cd70030b72b3199a2eb7ed2a427ac7462e959e93b367ff5855
$(package)_file_name_arm_linux=rust-$($(package)_version)-arm-unknown-linux-gnueabihf.tar.xz
$(package)_sha256_hash_arm_linux=e756f90ddd3c591de79d256d476a93cd55847d9e3295854926ffa186af1c9c13
$(package)_file_name_armv7l_linux=rust-$($(package)_version)-armv7-unknown-linux-gnueabihf.tar.xz
$(package)_sha256_hash_armv7l_linux=e756f90ddd3c591de79d256d476a93cd55847d9e3295854926ffa186af1c9c13
$(package)_file_name_aarch64_linux=rust-$($(package)_version)-aarch64-unknown-linux-gnu.tar.xz
$(package)_sha256_hash_aarch64_linux=1d92fc3dd807c0182d0f87b90a9d5dac8be4164d3096634e2da23cc2a6c39792
$(package)_file_name_x86_64_darwin=rust-$($(package)_version)-x86_64-apple-darwin.tar.xz
$(package)_sha256_hash_x86_64_darwin=a000929ab82a55186b718c20e75780328e5edc9c08660bd38454ca8b468184b1
$(package)_file_name_aarch64_darwin=rust-$($(package)_version)-aarch64-apple-darwin.tar.xz
$(package)_sha256_hash_aarch64_darwin=88f9ac5000f2e5541ddafde10374c8933ccf1e40f1dcdc09b1a616fb30fb8712
$(package)_file_name_x86_64_freebsd=rust-$($(package)_version)-x86_64-unknown-freebsd.tar.xz
$(package)_sha256_hash_x86_64_freebsd=ac084c6e952c2f673d1115eac0e42a79fc2af0c78d5789aa8f3923c58588c512

# Mapping from GCC canonical hosts to Rust targets
# If a mapping is not present, we assume they are identical, unless $host_os is
# "darwin", in which case we assume x86_64-apple-darwin.
$(package)_rust_target_x86_64-w64-mingw32=x86_64-pc-windows-gnu
$(package)_rust_target_i686-pc-linux-gnu=i686-unknown-linux-gnu
$(package)_rust_target_riscv64-linux-gnu=riscv64gc-unknown-linux-gnu
$(package)_rust_target_riscv64-unknown-linux-gnu=riscv64gc-unknown-linux-gnu
$(package)_rust_target_x86_64-linux-gnu=x86_64-unknown-linux-gnu
$(package)_rust_target_x86_64-pc-linux-gnu=x86_64-unknown-linux-gnu
$(package)_rust_target_armv7l-unknown-linux-gnueabihf=arm-unknown-linux-gnueabihf

# Mapping from Rust targets to SHA-256 hashes
$(package)_rust_std_sha256_hash_arm-unknown-linux-gnueabihf=c2f4a3332dfe1520a3761a9d072b8edcb6b5c0a84b1b24c3a7ac621e86b4e13e
$(package)_rust_std_sha256_hash_aarch64-unknown-linux-gnu=c3c5346b1e95ea9bd806b0dd9ff9aa618976fb38f4f3a615af4964bb4dd15633
$(package)_rust_std_sha256_hash_i686-unknown-linux-gnu=bef330af5bfb381a01349186e05402983495a3e2d4d1c35723a8443039d19a2d
$(package)_rust_std_sha256_hash_x86_64-unknown-linux-gnu=4c95739e6f0f1d4defd937f6d60360b566e051dfb2fa71879d0f9751392f3709
$(package)_rust_std_sha256_hash_riscv64gc-unknown-linux-gnu=8c32a848e2688b2900c3e073da8814ce5649ce6e0362be30d53517d7a9ef21ff
$(package)_rust_std_sha256_hash_x86_64-apple-darwin=20161f5c41856762d1ce946737feb833bb7acd2817a4068f4e3044b176e5f73c
$(package)_rust_std_sha256_hash_aarch64-apple-darwin=fdb1f29341f51e8b119f69e98b657a12fa60f12edfccfa494ae282de0553d4fd
$(package)_rust_std_sha256_hash_x86_64-pc-windows-gnu=aa1d30f2f66d0198ea304047262f9142c406618a35acc466c7ad2b2c1469435d

define rust_target
$(if $($(1)_rust_target_$(2)),$($(1)_rust_target_$(2)),$(if $(findstring darwin,$(3)),$(if $(findstring aarch64,$(host_arch)),aarch64-apple-darwin,x86_64-apple-darwin),$(2)))
endef

ifneq ($(canonical_host),$(build))
$(package)_rust_target=$(call rust_target,$(package),$(canonical_host),$(host_os))
$(package)_exact_file_name=rust-std-$($(package)_version)-$($(package)_rust_target).tar.xz
$(package)_exact_sha256_hash=$($(package)_rust_std_sha256_hash_$($(package)_rust_target))
$(package)_build_subdir=buildos
$(package)_extra_sources=$($(package)_file_name_$(build_arch)_$(build_os))

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_download_file),$($(package)_file_name),$($(package)_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_file_name_$(build_arch)_$(build_os)),$($(package)_file_name_$(build_arch)_$(build_os)),$($(package)_sha256_hash_$(build_arch)_$(build_os)))
endef

define $(package)_extract_cmds
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_sha256_hash_$(build_arch)_$(build_os))  $($(package)_source_dir)/$($(package)_file_name_$(build_arch)_$(build_os))" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  mkdir $(canonical_host) && \
  tar --strip-components=1 -xf $($(package)_source) -C $(canonical_host) && \
  mkdir buildos && \
  tar --strip-components=1 -xf $($(package)_source_dir)/$($(package)_file_name_$(build_arch)_$(build_os)) -C buildos
endef

define $(package)_stage_cmds
  bash ./install.sh --destdir=$($(package)_staging_dir) --prefix=$(build_prefix) --disable-ldconfig && \
  ../$(canonical_host)/install.sh --without=rust-docs --destdir=$($(package)_staging_dir) --prefix=$(build_prefix) --disable-ldconfig
endef
else

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_file_name_$(build_arch)_$(build_os)),$($(package)_file_name_$(build_arch)_$(build_os)),$($(package)_sha256_hash_$(build_arch)_$(build_os)))
endef

define $(package)_extract_cmds
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash_$(build_arch)_$(build_os))  $($(package)_source_dir)/$($(package)_file_name_$(build_arch)_$(build_os))" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  mkdir $(canonical_host) && \
  tar --strip-components=1 -xf $($(package)_source_dir)/$($(package)_file_name_$(build_arch)_$(build_os)) -C $(canonical_host)
endef

define $(package)_stage_cmds
  bash ./$(canonical_host)/install.sh --without=rust-docs --destdir=$($(package)_staging_dir) --prefix=$(build_prefix) --disable-ldconfig
endef
endif
