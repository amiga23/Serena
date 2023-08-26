# --------------------------------------------------------------------------
# Build variables
#

KERNEL_SOURCES_DIR := $(KERNEL_PROJECT_DIR)/Sources

KERNEL_C_SOURCES := $(wildcard $(KERNEL_SOURCES_DIR)/*.c)
KERNEL_ASM_SOURCES := $(wildcard $(KERNEL_SOURCES_DIR)/*.s)

KERNEL_OBJS := $(patsubst $(KERNEL_SOURCES_DIR)/%.c,$(KERNEL_BUILD_DIR)/%.o,$(KERNEL_C_SOURCES))
KERNEL_DEPS := $(KERNEL_OBJS:.o=.d)
KERNEL_OBJS += $(patsubst $(KERNEL_SOURCES_DIR)/%.s,$(KERNEL_BUILD_DIR)/%.o,$(KERNEL_ASM_SOURCES))


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean_kernel


$(KERNEL_OBJS): | $(KERNEL_BUILD_DIR) $(KERNEL_PRODUCT_DIR)

$(KERNEL_BUILD_DIR):
	$(call mkdir_if_needed,$(KERNEL_BUILD_DIR))

$(KERNEL_PRODUCT_DIR):
	$(call mkdir_if_needed,$(KERNEL_PRODUCT_DIR))


$(KERNEL_BIN_FILE): $(RUNTIME_LIB_FILE) $(KERNEL_OBJS)
	@echo Linking Kernel
	@$(LD) -s -brawbin1 -T $(KERNEL_SOURCES_DIR)/linker.script -o $@ $^


-include $(KERNEL_DEPS)

$(KERNEL_BUILD_DIR)/%.o : $(KERNEL_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) -I$(KERNEL_SOURCES_DIR) -I$(RUNTIME_HEADERS_DIR) -dontwarn=51 -dontwarn=148 -dontwarn=208 -deps -depfile=$(patsubst $(KERNEL_BUILD_DIR)/%.o,$(KERNEL_BUILD_DIR)/%.d,$@) -o $@ $<

$(KERNEL_BUILD_DIR)/%.o : $(KERNEL_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) -I$(KERNEL_SOURCES_DIR) -nowarn=62 -o $@ $<


clean_kernel:
	$(call rm_if_exists,$(KERNEL_BUILD_DIR))