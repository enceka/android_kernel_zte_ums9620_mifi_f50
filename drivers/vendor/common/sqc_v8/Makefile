
subdir-ccflags-$(CONFIG_VENDOR_SQC_CHARGER_V8)+= -I$(srctree)/drivers/vendor/common/sqc_v8

sqc_charger-$(CONFIG_VENDOR_SQC_CHARGER_V8) += sqc_charger_hal.o 
sqc_charger-$(CONFIG_VENDOR_SQC_CHARGER_V8) += sqc_usb1d2_hal.o
sqc_charger-$(CONFIG_VENDOR_SQC_CHARGER_V8) += sqc_notity.o
sqc_charger-$(CONFIG_VENDOR_SQC_CHARGER_V8) += sqc_misc.o
sqc_charger-$(CONFIG_VENDOR_SQC_CHARGER_V8) += sqc_platform.o
sqc_charger-$(CONFIG_VENDOR_SQC_CHARGER_V8) += sqc_usb_pd_hal.o
sqc_charger-$(CONFIG_VENDOR_SQC_CHARGER_V8) += sqc_usb_pd_hal.o
sqc_charger-$(CONFIG_VENDOR_SQC_CHARGER_V8) += pmic-voter.o
obj-$(CONFIG_VENDOR_SQC_MODULE) += sqc_charger.o


obj-$(CONFIG_VENDOR_SQC_NETLINK) += sqc_netlink.o

########## BUCK ############
obj-$(CONFIG_VENDOR_SQC_BQ2560X) += sqc_bq2560x.o
obj-$(CONFIG_VENDOR_SQC_SC8989X) += sqc_sc8989x.o
obj-$(CONFIG_VENDOR_SQC_SGM41510) += sqc_sgm41510.o
obj-$(CONFIG_VENDOR_SQC_HL7009) += sqc_hl7009.o
obj-$(CONFIG_VENDOR_SQC_SD76930) += sqc_sd76930.o

########## CP ############
obj-$(CONFIG_VENDOR_SQC_BQ2597X) += bq25970.o
obj-$(CONFIG_VENDOR_SQC_NU2115)  += nu2115.o
obj-$(CONFIG_VENDOR_SQC_SC8541)	 += sc8541_charger.o
obj-$(CONFIG_VENDOR_SQC_UPM6720) += upm6720_charger.o

ifeq ($(CONFIG_VENDOR_CHARGER_POLICY_SERVICE),m)
	ccflags-y += -DCONFIG_VENDOR_CHARGER_POLICY_SERVICE_COMMON
endif

ifeq ($(CONFIG_VENDOR_ZTE_MISC),m)
	ccflags-y += -DCONFIG_VENDOR_ZTE_MISC_COMMON
endif

ifeq ($(CONFIG_FAST_CHARGER_SC27XX),m)
	ccflags-y += -DCONFIG_FAST_CHARGER
endif

ifeq ($(CONFIG_VENDOR_ZTE_LOG_EXCEPTION),m)
	ccflags-y += -DCONFIG_VENDOR_ZTE_LOG_EXCEPTION
endif
