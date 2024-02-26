package=zlib
$(package)_version=1.2.13
$(package)_download_path=https://www.zlib.net
$(package)_file_name=$(package)-$($(package)_version).tar.xz
$(package)_sha256_hash=d14c38e313afc35a9a8760dadf26042f51ea0f5d154b0630a31da0540107fb98

define $(package)_set_vars
$(package)_build_opts= CC="$($(package)_cc)"
$(package)_build_opts+=CFLAGS="$($(package)_cflags) $($(package)_cppflags) -fPIC"
$(package)_build_opts+=RANLIB="$($(package)_ranlib)"
$(package)_build_opts+=AR="$($(package)_ar)"
$(package)_build_opts_darwin+=AR="$($(package)_libtool)"
$(package)_build_opts_darwin+=ARFLAGS="-o"
endef

define $(package)_config_cmds
  ./configure --static --prefix=$(host_prefix)
endef

define $(package)_build_cmds
  $(MAKE) $($(package)_build_opts) libz.a
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install $($(package)_build_opts)
endef

