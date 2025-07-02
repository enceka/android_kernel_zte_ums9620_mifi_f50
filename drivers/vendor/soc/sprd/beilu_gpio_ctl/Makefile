#
# Makefile for zte beilu_gpio_ctl driver.
#

obj-$(CONFIG_ZTE_BEILU_GPIO_CTL) += beilu_gpio_ctl.o

ifeq ($(CONFIG_ZTE_BEILU_GPIO_CTL_SRC),y)
beilu_gpio_ctl-y += beilu_gpio_ctl_src.o
else
beilu_gpio_ctl-y += gpio_ctl_src.o
endif
