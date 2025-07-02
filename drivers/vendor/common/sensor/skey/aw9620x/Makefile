mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
mkfile_dir := $(dir $(mkfile_path))

obj-$(CONFIG_SKEY_SENSOR_AW9620X) += zte_skey.o
zte_skey-objs := aw9620x.o