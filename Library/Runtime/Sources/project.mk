# --------------------------------------------------------------------------
# Build variables
#

RUNTIME_SOURCES_DIR := $(WORKSPACE_DIR)/Library/Runtime/Sources

RUNTIME_C_SOURCES := $(wildcard $(RUNTIME_SOURCES_DIR)/*.c)
RUNTIME_ASM_SOURCES := $(wildcard $(RUNTIME_SOURCES_DIR)/*.s)

RUNTIME_OBJS := $(patsubst $(RUNTIME_SOURCES_DIR)/%.c, $(RUNTIME_BUILD_DIR)/%.o, $(RUNTIME_C_SOURCES))
RUNTIME_OBJS += $(patsubst $(RUNTIME_SOURCES_DIR)/%.s, $(RUNTIME_BUILD_DIR)/%.o, $(RUNTIME_ASM_SOURCES))


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean_runtime


$(RUNTIME_OBJS): | $(RUNTIME_BUILD_DIR)

$(RUNTIME_BUILD_DIR):
	$(call mkdir_if_needed,$(RUNTIME_BUILD_DIR))


$(RUNTIME_LIB_FILE): $(RUNTIME_OBJS)
	@echo Linking librt.a
	@$(LD) -baoutnull -r -o $@ $^


$(RUNTIME_BUILD_DIR)/%.o : $(RUNTIME_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) -I$(RUNTIME_SOURCES_DIR) -o $@ $<

$(RUNTIME_BUILD_DIR)/%.o : $(RUNTIME_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) -I$(RUNTIME_SOURCES_DIR) -o $@ $<


clean_runtime:
	$(call rm_if_exists,$(RUNTIME_BUILD_DIR))
