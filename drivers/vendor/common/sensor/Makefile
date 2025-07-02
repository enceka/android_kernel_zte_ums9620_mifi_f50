
# Makefile for the common kernel device drivers.
obj-$(CONFIG_ZTE_TEMPERATURE_SENSOR)	+= temperature/
obj-$(CONFIG_CUSTOM_KERNEL_SAR_SENSOR)	+= sar/
obj-$(CONFIG_CUSTOM_KERNEL_SKEY_SENSOR)	+= skey/
obj-$(CONFIG_CUSTOM_KERNEL_HALL_SENSOR)	+= hall/

ifeq ($(CONFIG_VENDOR_ZTE_MISC),m)
	subdir-ccflags-y += -DCONFIG_VENDOR_ZTE_MISC_COMMON
endif
