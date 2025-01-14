# --------------------------------------------------------------------------
# Build variables
#

DISPATCHER_C_SOURCES := $(wildcard $(DISPATCHER_SOURCES_DIR)/*.c)
DISPATCHER_ASM_SOURCES := $(wildcard $(DISPATCHER_SOURCES_DIR)/*.s)

DISPATCHER_OBJS := $(patsubst $(DISPATCHER_SOURCES_DIR)/%.c,$(DISPATCHER_OBJS_DIR)/%.o,$(DISPATCHER_C_SOURCES))
DISPATCHER_DEPS := $(DISPATCHER_OBJS:.o=.d)
DISPATCHER_OBJS += $(patsubst $(DISPATCHER_SOURCES_DIR)/%.s,$(DISPATCHER_OBJS_DIR)/%.o,$(DISPATCHER_ASM_SOURCES))

DISPATCHER_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(DISPATCHER_SOURCES_DIR)
DISPATCHER_ASM_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(DISPATCHER_SOURCES_DIR)

#DISPATCHER_GENERATE_DEPS = -deps -depfile=$(patsubst $(DISPATCHER_OBJS_DIR)/%.o,$(DISPATCHER_OBJS_DIR)/%.d,$@)
DISPATCHER_GENERATE_DEPS := 
DISPATCHER_AS_DONTWARN := -nowarn=62
DISPATCHER_CC_DONTWARN := -dontwarn=51 -dontwarn=148 -dontwarn=208


# --------------------------------------------------------------------------
# Build rules
#

$(DISPATCHER_OBJS): | $(DISPATCHER_OBJS_DIR)

$(DISPATCHER_OBJS_DIR):
	$(call mkdir_if_needed,$(DISPATCHER_OBJS_DIR))

-include $(DISPATCHER_DEPS)

$(DISPATCHER_OBJS_DIR)/%.o : $(DISPATCHER_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(DISPATCHER_C_INCLUDES) $(DISPATCHER_CC_DONTWARN) $(DISPATCHER_GENERATE_DEPS) -o $@ $<

$(DISPATCHER_OBJS_DIR)/%.o : $(DISPATCHER_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KERNEL_ASM_CONFIG) $(DISPATCHER_ASM_INCLUDES) $(DISPATCHER_AS_DONTWARN) -o $@ $<
