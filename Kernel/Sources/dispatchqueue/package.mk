# --------------------------------------------------------------------------
# Build variables
#

DISPATCHQUEUE_C_SOURCES := $(wildcard $(DISPATCHQUEUE_SOURCES_DIR)/*.c)
DISPATCHQUEUE_ASM_SOURCES := $(wildcard $(DISPATCHQUEUE_SOURCES_DIR)/*.s)

DISPATCHQUEUE_OBJS := $(patsubst $(DISPATCHQUEUE_SOURCES_DIR)/%.c,$(DISPATCHQUEUE_OBJS_DIR)/%.o,$(DISPATCHQUEUE_C_SOURCES))
DISPATCHQUEUE_DEPS := $(DISPATCHQUEUE_OBJS:.o=.d)
DISPATCHQUEUE_OBJS += $(patsubst $(DISPATCHQUEUE_SOURCES_DIR)/%.s,$(DISPATCHQUEUE_OBJS_DIR)/%.o,$(DISPATCHQUEUE_ASM_SOURCES))

DISPATCHQUEUE_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(DISPATCHQUEUE_SOURCES_DIR)
DISPATCHQUEUE_ASM_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(DISPATCHQUEUE_SOURCES_DIR)

#DISPATCHQUEUE_GENERATE_DEPS = -deps -depfile=$(patsubst $(DISPATCHQUEUE_OBJS_DIR)/%.o,$(DISPATCHQUEUE_OBJS_DIR)/%.d,$@)
DISPATCHQUEUE_GENERATE_DEPS := 
DISPATCHQUEUE_AS_DONTWARN := -nowarn=62
DISPATCHQUEUE_CC_DONTWARN := -dontwarn=51 -dontwarn=148 -dontwarn=208


# --------------------------------------------------------------------------
# Build rules
#

$(DISPATCHQUEUE_OBJS): | $(DISPATCHQUEUE_OBJS_DIR)

$(DISPATCHQUEUE_OBJS_DIR):
	$(call mkdir_if_needed,$(DISPATCHQUEUE_OBJS_DIR))

-include $(DISPATCHQUEUE_DEPS)

$(DISPATCHQUEUE_OBJS_DIR)/%.o : $(DISPATCHQUEUE_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(DISPATCHQUEUE_C_INCLUDES) $(DISPATCHQUEUE_CC_DONTWARN) $(DISPATCHQUEUE_GENERATE_DEPS) -o $@ $<

$(DISPATCHQUEUE_OBJS_DIR)/%.o : $(DISPATCHQUEUE_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KERNEL_ASM_CONFIG) $(DISPATCHQUEUE_ASM_INCLUDES) $(DISPATCHQUEUE_AS_DONTWARN) -o $@ $<
